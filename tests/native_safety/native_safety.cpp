#include "common/file_system/local_file_system.h"
#include "common/file_system/virtual_file_system.h"
#include "main/connection.h"
#include "main/database.h"
#include "main/query_result.h"
#include "storage/buffer_manager/buffer_manager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <tuple>
#include <type_traits>
#include <utility>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
using namespace lbug::common;
namespace fs = std::filesystem;

void require(bool ok, const char* message) {
    if (!ok) { throw std::runtime_error(message); }
}
template<class F> void refused(F operation, const std::string& reason = "") {
    bool failed = false;
    try { operation(); } catch (const std::exception& e) {
        failed = reason.empty() || std::string(e.what()).find(reason) != std::string::npos;
    }
    require(failed, "unsafe operation was accepted or returned the wrong error");
}
std::string bytes(const fs::path& p) {
    std::ifstream file(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
#ifndef _WIN32
using Snapshot = std::map<std::string, std::tuple<fs::file_type, uintmax_t, fs::file_time_type>>;
Snapshot outsideSnapshot(const fs::path& parent, const fs::path& root, const fs::path& sibling) {
    Snapshot result;
    auto record = [&](const fs::path& p) {
        auto kind = fs::symlink_status(p).type();
        struct stat info{};
        require(lstat(p.c_str(), &info) == 0, "snapshot metadata");
        auto size = static_cast<uintmax_t>(info.st_size);
        result.emplace(p.string(), std::make_tuple(kind, size, fs::last_write_time(p)));
    };
    for (auto base : {parent, sibling}) {
        record(base);
        for (auto it = fs::recursive_directory_iterator(base); it != fs::recursive_directory_iterator(); ++it) {
            // Only the application-owned root may change; its parent's own metadata
            // and every sibling entry (including sibling directories) must not change.
            if (it->path() == root) { it.disable_recursion_pending(); continue; }
            record(it->path());
        }
    }
    return result;
}
namespace lbug::common {
extern void (*maestroBeforeRestrictedOpen)(int directory, const char* name);
}
void replaceBeforeRestrictedOpen(int directory, const char* name) {
    maestroBeforeRestrictedOpen = nullptr;
    require(renameat(directory, name, directory, "interleave-original") == 0, "rename original");
    auto fd = openat(directory, name, O_CREAT | O_EXCL | O_WRONLY, 0600);
    require(fd >= 0, "create replacement");
    require(write(fd, "replacement", 11) == 11, "write replacement");
    close(fd);
}
#endif
int main() {
    int failures = 0;
    auto test = [&](const char* name, auto operation) {
        try { operation(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failures; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    };
#ifdef _WIN32
    test("windows fails closed", [] {
        refused([] { RootDirectory::open(fs::temp_directory_path().string()); }, "unsupported");
    });
#else
    auto dir = fs::temp_directory_path() / ("maestro-native-" + std::to_string(getpid()));
    fs::remove_all(dir);
    fs::create_directories(dir / "parent" / "root");
    fs::create_directory(dir / "outside");
    auto rootPath = fs::canonical(dir / "parent" / "root");
    auto sibling = fs::canonical(dir / "outside");
    auto sentinel = sibling / "sentinel";
    std::ofstream(sentinel) << "outside sentinel";
    struct stat before{};
    require(stat(sentinel.c_str(), &before) == 0, "stat sentinel");
    auto checkedAt = [&](const fs::path& ownedRoot, auto operation) -> decltype(operation()) {
        auto snapshot = outsideSnapshot(ownedRoot.parent_path(), ownedRoot, sibling);
        auto unchanged = [&] {
            require(snapshot == outsideSnapshot(ownedRoot.parent_path(), ownedRoot, sibling), "outside metadata changed");
            struct stat after{};
            require(stat(sentinel.c_str(), &after) == 0 && before.st_dev == after.st_dev &&
                before.st_ino == after.st_ino && bytes(sentinel) == "outside sentinel", "outside sentinel changed");
        };
        try {
            if constexpr (std::is_void_v<decltype(operation())>) { operation(); unchanged(); }
            else { auto value = operation(); unchanged(); return value; }
        } catch (...) { unchanged(); throw; }
    };
    auto checked = [&](auto operation) -> decltype(operation()) {
        return checkedAt(rootPath, std::move(operation));
    };
    auto root = checked([&] { return RootDirectory::open(rootPath.string()); });
    LocalFileSystem local("db.lbdb", root);
    VirtualFileSystem vfs("db.lbdb", root);
    auto open = [&](const std::string& child, FileOpenFlags flags) {
        return checked([&] { return local.openFile(child, flags); });
    };
    const std::vector<int> modes = {FileFlags::READ_ONLY, FileFlags::WRITE,
        FileFlags::READ_ONLY | FileFlags::WRITE, FileFlags::WRITE | FileFlags::CREATE_IF_NOT_EXISTS,
        FileFlags::WRITE | FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS};
    for (auto mode : modes) {
        auto leaf = "leaf-" + std::to_string(mode);
        fs::create_symlink(sentinel, rootPath / leaf);
        test(("leaf link mode " + std::to_string(mode)).c_str(), [&] {
            refused([&] { open(leaf, FileOpenFlags(mode)); });
        });
        fs::remove(rootPath / leaf);
    }
    test("invalid open flags fail closed", [&] {
        for (auto flags : {FileFlags::READ_ONLY | FileFlags::CREATE_IF_NOT_EXISTS,
                FileFlags::WRITE | FileFlags::CREATE_IF_NOT_EXISTS | FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS,
                FileFlags::WRITE | 0x80}) {
            refused([&] { open("invalid-flags", FileOpenFlags(flags)); }, "flags");
        }
        require(!fs::exists(rootPath / "invalid-flags"), "invalid flags created a file");
    });
    test("hard link alias all modes", [&] {
        fs::create_hard_link(sentinel, rootPath / "alias");
        for (auto mode : modes) { refused([&] { open("alias", FileOpenFlags(mode)); }); }
        fs::remove(rootPath / "alias");
    });
    test("directory refused all modes", [&] {
        fs::create_directory(rootPath / "directory");
        for (auto mode : modes) { refused([&] { open("directory", FileOpenFlags(mode)); }); }
    });
    test("normal create reopen sidecars truncate and handle IO", [&] {
        for (auto child : {"db.lbdb", "db.lbdb.wal", "db.lbdb.shadow", "db.lbdb.tmp",
                "db.lbdb.checkpoint", "db.lbdb.checkpoint.intent.lock", "db.lbdb.checkpoint.apply.lock"}) {
            auto f = open(child, FileOpenFlags(FileFlags::WRITE | FileFlags::CREATE_IF_NOT_EXISTS));
            checked([&] { f->writeFile(reinterpret_cast<const uint8_t*>("content"), 7, 0); f->syncFile(); });
            f.reset();
            f = open(child, FileOpenFlags(FileFlags::READ_ONLY));
            char value[7];
            checked([&] { f->readFromFile(value, 7, 0); });
            require(std::string(value, 7) == "content", "reopen bytes");
            f.reset();
            f = open(child, FileOpenFlags(FileFlags::WRITE | FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS));
            require(f->getFileSize() == 0, "truncate");
        }
    });
    test("leaf replacement all modes", [&] {
        open("replace", FileOpenFlags(FileFlags::WRITE | FileFlags::CREATE_IF_NOT_EXISTS));
        fs::rename(rootPath / "replace", rootPath / "saved");
        std::ofstream(rootPath / "replace") << "replacement";
        for (auto mode : modes) { refused([&] { open("replace", FileOpenFlags(mode)); }); }
        require(bytes(rootPath / "replace") == "replacement", "replacement truncated");
        refused([&] { checked([&] { local.fileOrPathExists("replace"); }); });
    });
    test("native lock contention uses held handle", [&] {
        auto f = open("locked", FileOpenFlags(FileFlags::WRITE | FileFlags::CREATE_IF_NOT_EXISTS, FileLockType::WRITE_LOCK));
        checked([&] { f->writeFile(reinterpret_cast<const uint8_t*>("locked"), 6, 0); });
        auto child = fork();
        require(child >= 0, "fork");
        if (child == 0) {
            try { refused([&] { open("locked", FileOpenFlags(FileFlags::WRITE | FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS, FileLockType::WRITE_LOCK)); }, "Could not set lock"); _exit(0); }
            catch (...) { _exit(1); }
        }
        int status{};
        require(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0, "child lock was accepted");
        require(bytes(rootPath / "locked") == "locked", "failed lock truncated content");
        f.reset();
        open("locked", FileOpenFlags(FileFlags::READ_ONLY, FileLockType::READ_LOCK));
    });
    for (auto operation : {"renameFile", "copyFile", "overwriteFile", "createDir", "removeFileIfExists", "glob", "expandPath"}) {
        test(operation, [&] {
            refused([&] { checked([&] {
                std::string op(operation);
                if (op == "renameFile") { vfs.renameFile(sentinel.string(), (sibling / "renamed").string()); }
                if (op == "copyFile") { vfs.copyFile(sentinel.string(), (sibling / "copied").string()); }
                if (op == "overwriteFile") { vfs.overwriteFile(sentinel.string(), sentinel.string()); }
                if (op == "createDir") { vfs.createDir((sibling / "created").string()); }
                if (op == "removeFileIfExists") { vfs.removeFileIfExists((sibling / "db.lbdb.wal").string()); }
                if (op == "glob") { vfs.glob(nullptr, sentinel.string()); }
                if (op == "expandPath") { vfs.expandPath(nullptr, sentinel.string()); }
            }); }, operation);
        });
    }
    test("plain names validated before FS calls", [&] {
        for (auto invalid : {std::string(""), std::string("."), std::string(".."),
                std::string("a/b"), std::string("a\\b"), sentinel.string(), std::string("a\0b", 3)}) {
            refused([&] { open(invalid, FileOpenFlags(FileFlags::WRITE | FileFlags::CREATE_IF_NOT_EXISTS)); }, "name");
            refused([&] { checked([&] { lbug::main::Database db(root, invalid); }); }, "name");
        }
    });
    test("metadata refuses leaf links", [&] {
        fs::create_symlink(sentinel, rootPath / "probe-link");
        refused([&] { checked([&] { local.fileOrPathExists("probe-link"); }); });
        require(!checked([&] { return local.fileOrPathExists("missing"); }), "missing regular file");
    });
    test("startup directory probe refuses directory", [&] {
        refused([&] { checked([&] { lbug::main::Database db(root, "directory"); }); });
    });
    test("native database create reopen and no startup deletion", [&] {
        auto config = lbug::main::SystemConfig(16 * 1024 * 1024, 1);
        config.maxDBSize = 64 * 1024 * 1024;
        config.forceCheckpointOnClose = false;
        // Rooted removeFileIfExists always throws: green create/reopen is the
        // behavioural oracle that startup did not call it. No production counter.
        checked([&] { lbug::main::Database db(root, "normal.lbdb", config); });
        config.readOnly = true;
        checked([&] { lbug::main::Database db(root, "normal.lbdb", config); });
        require(!fs::exists(rootPath / "normal.lbdb.tmp"), "startup created temp file");
    });
    auto checkpointConfig = [] {
        auto config = lbug::main::SystemConfig(16 * 1024 * 1024, 1);
        config.maxDBSize = 64 * 1024 * 1024;
        config.forceCheckpointOnClose = false;
        return config;
    };
    auto reopenCheckpoint = [&](const char* name, auto config) {
        config.readOnly = true;
        checked([&] { lbug::main::Database db(root, name, config); });
        config.readOnly = false;
        checked([&] { lbug::main::Database db(root, name, config); });
    };
    auto noCheckpointSidecars = [&](const char* name) {
        for (const auto& entry : fs::directory_iterator(rootPath)) {
            require(!entry.path().filename().string().starts_with(std::string(name) + "."),
                "checkpoint created a sidecar");
        }
    };
    test("empty checkpoint refusal keeps database reopenable", [&] {
        auto config = checkpointConfig();
        {
            lbug::main::Database db(root, "checkpoint-repro.lbdb", config);
            const auto beforeBytes = bytes(rootPath / "checkpoint-repro.lbdb");
            lbug::main::Connection connection(&db);
            auto result = checked([&] { return connection.query("CHECKPOINT"); });
            require(!result->isSuccess(), "unsupported checkpoint succeeded");
            require(beforeBytes == bytes(rootPath / "checkpoint-repro.lbdb"), "checkpoint changed base bytes");
            noCheckpointSidecars("checkpoint-repro.lbdb");
        }
        reopenCheckpoint("checkpoint-repro.lbdb", config);
    });
    test("default rooted close keeps empty database reopenable", [&] {
        auto config = checkpointConfig();
        std::string beforeBytes;
        config.forceCheckpointOnClose = true;
        {
            lbug::main::Database db(root, "close-repro.lbdb", config);
            beforeBytes = bytes(rootPath / "close-repro.lbdb");
        }
        require(beforeBytes == bytes(rootPath / "close-repro.lbdb"), "close changed base bytes");
        noCheckpointSidecars("close-repro.lbdb");
        config.forceCheckpointOnClose = false;
        reopenCheckpoint("close-repro.lbdb", config);
    });
    test("rooted auto checkpoint refuses before changing base", [&] {
        auto config = checkpointConfig();
        config.autoCheckpoint = true;
        config.checkpointThreshold = 0;
        lbug::main::Database db(root, "auto-checkpoint.lbdb", config);
        const auto beforeBytes = bytes(rootPath / "auto-checkpoint.lbdb");
        lbug::main::Connection connection(&db);
        auto result = checked([&] {
            return connection.query("CREATE NODE TABLE Auto(id INT64, PRIMARY KEY(id))");
        });
        require(!result->isSuccess() &&
            result->getErrorMessage().find("post-commit checkpoint failed") != std::string::npos &&
            result->getErrorMessage().find("Restricted filesystem checkpoint") != std::string::npos,
            "auto checkpoint did not reach restricted refusal");
        require(beforeBytes == bytes(rootPath / "auto-checkpoint.lbdb"), "auto checkpoint changed base bytes");
        require(fs::exists(rootPath / "auto-checkpoint.lbdb.wal"), "committed WAL missing");
        for (auto suffix : {".shadow", ".wal.checkpoint", ".checkpoint.intent.lock", ".checkpoint.apply.lock"}) {
            require(!fs::exists(rootPath / (std::string("auto-checkpoint.lbdb") + suffix)),
                "auto checkpoint created a sidecar");
        }
    });
    test("rooted writable recovery refuses existing sidecars", [&] {
        auto config = checkpointConfig();
        for (auto suffix : {".wal", ".wal.checkpoint", ".shadow"}) {
            auto name = std::string("recovery") + suffix + ".lbdb";
            checked([&] { lbug::main::Database db(root, name, config); });
            const auto beforeBytes = bytes(rootPath / name);
            std::ofstream(rootPath / (name + suffix)) << "pending recovery";
            refused([&] { checked([&] { lbug::main::Database db(root, name, config); }); },
                "Restricted writable WAL recovery");
            require(beforeBytes == bytes(rootPath / name), "recovery changed base bytes");
            require(bytes(rootPath / (name + suffix)) == "pending recovery", "recovery changed sidecar");
        }
    });
    test("forced spill fails closed without temp file", [&] {
        lbug::storage::BufferManager bm("spill.lbdb", "spill.lbdb.tmp", 16 * 1024 * 1024,
            64 * 1024 * 1024, &vfs, true);
        refused([&] { checked([&] { bm.resetSpiller("spill.lbdb.tmp"); }); }, "restricted spill unsupported");
        require(!fs::exists(rootPath / "spill.lbdb.tmp"), "spill created temp file");
    });
    test("alternative FS dispatch fails closed", [&] {
        refused([&] { checked([&] { vfs.registerFileSystem(std::make_unique<LocalFileSystem>("")); }); }, "registerFileSystem");
        refused([&] { checked([&] { vfs.openFile("https://invalid/db", FileOpenFlags(FileFlags::READ_ONLY)); }); });
        refused([&] { checked([&] { vfs.FileSystem::renameFile(sentinel.string(), (sibling / "renamed").string()); }); }, "renameFile");
        auto flags = FileOpenFlags(FileFlags::READ_ONLY);
        flags.compressionType = FileCompressionType::GZIP;
        refused([&] { checked([&] { vfs.openFile("db.lbdb", flags); }); }, "compressed openFile");
    });
    test("root path links resolved once", [&] {
        fs::create_directory_symlink(rootPath, dir / "root-alias");
        auto alias = checked([&] { return RootDirectory::open((dir / "root-alias").string()); });
        LocalFileSystem linked("db.lbdb", alias);
        checked([&] { linked.openFile("db.lbdb", FileOpenFlags(FileFlags::READ_ONLY)); });
    });
    test("intermediate links below root refused", [&] {
        fs::create_directory_symlink(sibling, rootPath / "nested-link");
        refused([&] { open("nested-link/sentinel", FileOpenFlags(FileFlags::WRITE)); }, "name");
    });
    test("root replacement refused all modes", [&] {
        auto isolated = dir / "isolated";
        fs::create_directory(isolated);
        auto cap = checkedAt(isolated, [&] { return RootDirectory::open(isolated.string()); });
        LocalFileSystem isolatedFS("db.lbdb", cap);
        fs::rename(isolated, dir / "old-isolated");
        fs::create_directory_symlink(sibling, isolated);
        for (auto mode : modes) {
            refused([&] { checkedAt(isolated, [&] { isolatedFS.openFile("sentinel", FileOpenFlags(mode)); }); }, "ancestor");
        }
    });
    test("check open replacement refuses truncation", [&] {
        std::ofstream(rootPath / "interleave") << "original";
        maestroBeforeRestrictedOpen = replaceBeforeRestrictedOpen;
        refused([&] { open("interleave", FileOpenFlags(FileFlags::WRITE | FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS)); }, "identity changed during open");
        require(!maestroBeforeRestrictedOpen, "interleave seam not reached");
        require(bytes(rootPath / "interleave") == "replacement", "replacement truncated");
        require(bytes(rootPath / "interleave-original") == "original", "original changed");
    });
    test("real held ancestor replacement refused on open and probe", [&] {
        auto parent = dir / "real-parent";
        auto held = parent / "root";
        fs::create_directories(held);
        std::ofstream(held / "db.lbdb") << "original";
        auto cap = RootDirectory::open(held.string());
        LocalFileSystem isolatedFS("db.lbdb", cap);
        fs::rename(parent, dir / "real-parent-old");
        fs::create_directories(held);
        std::ofstream(held / "db.lbdb") << "replacement";
        refused([&] { isolatedFS.openFile("db.lbdb", FileOpenFlags(FileFlags::READ_ONLY)); }, "ancestor identity");
        refused([&] { isolatedFS.fileOrPathExists("db.lbdb"); }, "ancestor identity");
        require(bytes(dir / "real-parent-old" / "root" / "db.lbdb") == "original", "held inode changed");
    });
    test("held ancestor replacement refused all modes and probe", [&] {
        fs::rename(dir / "parent", dir / "old-parent");
        fs::create_directory_symlink(sibling, dir / "parent");
        for (auto mode : modes) { refused([&] { open("db.lbdb", FileOpenFlags(mode)); }, "ancestor"); }
        refused([&] { checked([&] { local.fileOrPathExists("db.lbdb"); }); }, "ancestor");
    });
    fs::remove_all(dir);
#endif
    std::cout << "failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
