#include "rooted_test_support.h"
#include "common/exception/io.h"
#ifndef _WIN32
Fixture* activeFixture = nullptr;
int syncCount = 0;
int injectedError = 0;
int nativeCalls = 0;
int observe(const char* operation, int directory, const char*, struct stat*) {
    if (std::string(operation) == "directory-sync") {
        struct stat held{}, current{};
        require(fstat(directory, &held) == 0 && lstat(activeFixture->path.c_str(), &current) == 0 &&
            held.st_dev == current.st_dev && held.st_ino == current.st_ino, "sync did not use held directory");
        ++syncCount; return injectedError;
    }
    return 0;
}
int unsupportedRename(const char* operation, int, const char*, struct stat*) {
    if (std::string(operation) == "rename-no-replace") { ++nativeCalls; return injectedError; }
    return 0;
}
int raceDestination(const char* operation, int directory, const char* name, struct stat*) {
    if (std::string(operation) == "rename-no-replace") {
        auto fd = openat(directory, name, O_WRONLY | O_CREAT | O_EXCL, 0600);
        require(fd >= 0 && write(fd, "intruder", 8) == 8, "destination interleave"); close(fd);
    }
    return 0;
}
int unlinkFailure(const char* operation, int, const char*, struct stat*) {
    if (std::string(operation) == "unlink") { ++nativeCalls; return EIO; }
    return 0;
}
int countSync(const char* operation, int, const char*, struct stat*) {
    if (std::string(operation) == "directory-sync") { ++syncCount; }
    return 0;
}
int foreignOwner(const char* operation, int, const char*, struct stat* info) {
    if (std::string(operation) == "adopt") { ++info->st_uid; ++nativeCalls; }
    return 0;
}
int swapAncestorDuringSync(const char* operation, int, const char*, struct stat*) {
    if (std::string(operation) == "directory-sync") {
        fs::rename(activeFixture->dir / "parent", activeFixture->dir / "old-parent");
        fs::create_directories(activeFixture->path);
    }
    return 0;
}
int swapCreatedTemp(const char* operation, int directory, const char* name, struct stat*) {
    if (std::string(operation) == "temp-opened") {
        require(renameat(directory, name, directory, "original-temp") == 0, "temp rename");
        auto fd = openat(directory, name, O_WRONLY | O_CREAT | O_EXCL, 0600);
        require(fd >= 0 && write(fd, "replacement", 11) == 11, "temp replacement"); close(fd);
    }
    return 0;
}
int swapTempAfterSync(const char* operation, int directory, const char*, struct stat* info) {
    if (std::string(operation) == "directory-sync") {
        auto name = lbug::storage::StorageUtils::getPKValidatorSpillFilePath("db.lbdb", 0);
        return swapCreatedTemp("temp-opened", directory, name.c_str(), info);
    }
    return 0;
}

int fileSyncCalls = 0;
int countFileSync(const char* operation, int, const char*, struct stat*) {
    if (std::string(operation) == "file-sync") { ++fileSyncCalls; }
    return 0;
}
int checkFreshSyncOrdering(const char* operation, int directory, const char* name, struct stat* info) {
    if (std::string(operation) == "file-sync") {
        ++fileSyncCalls; require(syncCount == 1, "file sync preceded fresh directory sync");
    }
    return observe(operation, directory, name, info);
}
int poisonDuringFileSync(const char* operation, int, const char*, struct stat*) {
    if (std::string(operation) == "directory-sync") { return EIO; }
    if (std::string(operation) == "file-sync") {
        ++fileSyncCalls;
        refused([&] { activeFixture->vfs->syncDirectoryForFile("db.lbdb"); });
    }
    return 0;
}

#endif
int main(int argc, char** argv) {
    int failures = 0, count = 0;
    auto test = [&](const char* name, auto op) {
        if (argc > 1 && std::string(name).find(argv[1]) == std::string::npos) { return; }
        ++count;
        try { op(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failures; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    };
#ifndef _WIN32
    test("rooted filesystem validates the database name before operations", [] {
        Fixture f;
        for (auto name : {std::string(""), std::string(".."), std::string("a/b"), std::string("a\0b", 3)}) {
            refused([&] { LocalFileSystem local(name, f.root); });
            refused([&] { VirtualFileSystem vfs(name, f.root); });
        }
        require(fs::is_empty(f.path), "invalid filesystem name touched root"); f.unchanged();
    });
    test("rooted rename refuses owned graph and unrelated sources", [] {
        Fixture f;
        for (const auto& name : {lbug::storage::StorageUtils::getGraphPath("db.lbdb", "graph"),
                std::string("other.lbdb"), std::string("db.graph.lbdb.wal")}) {
            f.owned(name);
            refused([&] { f.vfs->renameFile(name, "published.lbdb"); });
            require(bytes(f.path / name) == "owned", "unrelated source renamed");
        }
        require(!fs::exists(f.path / "published.lbdb"), "graph source published"); f.unchanged();
    });
    test("rename publication and identity ownership", [] {
        Fixture f;
        f.owned("db.lbdb");
        f.local->renameFile("db.lbdb", "published.lbdb");
        require(!f.local->fileOrPathExists("db.lbdb"), "source identity not forgotten");
        auto published = f.local->openFile("published.lbdb", FileOpenFlags(FileFlags::READ_ONLY));
        require(bytes(f.path / "published.lbdb") == "owned", "publication bytes");
        fs::rename(f.path / "published.lbdb", f.path / "saved");
        std::ofstream(f.path / "published.lbdb") << "replacement";
        refused([&] { f.local->openFile("published.lbdb", FileOpenFlags(FileFlags::READ_ONLY)); });
        f.unchanged();
    });
    test("base rename uses the held root", [] {
        Fixture f;
        f.owned("db.lbdb.wal");
        f.vfs->FileSystem::renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint");
        require(bytes(f.path / "db.lbdb.wal.checkpoint") == "owned", "base rename missed root");
        f.vfs->removeFileIfExists("db.lbdb.wal.checkpoint");
        refused([&] { f.vfs->FileSystem::renameFile(f.sentinel.string(), (f.dir / "outside" / "renamed").string()); });
        f.unchanged();
    });
    test("rename owned companion replacement", [] {
        Fixture f;
        f.owned("db.lbdb.wal", "new"); f.owned("db.lbdb.wal.checkpoint", "old");
        f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint");
        require(bytes(f.path / "db.lbdb.wal.checkpoint") == "new", "owned replacement");
        f.vfs->removeFileIfExists("db.lbdb.wal.checkpoint");
        std::ofstream(f.path / "db.lbdb.wal") << "unowned recreated source";
        refused([&] { f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint"); });
        fs::remove(f.path / "db.lbdb.wal");
        f.owned("db.lbdb.wal");
        f.unchanged();
    });
    test("rename existing destination never publishes over it", [] {
        Fixture f; f.owned("db.lbdb"); f.owned("published.lbdb", "existing");
        refused([&] { f.local->renameFile("db.lbdb", "published.lbdb"); });
        require(bytes(f.path / "published.lbdb") == "existing", "destination overwritten");
        require(bytes(f.path / "db.lbdb") == "owned", "source lost"); f.unchanged();
    });
    test("rename unowned source", [] {
        Fixture f; std::ofstream(f.path / "db.lbdb.wal") << "unowned";
        refused([&] { f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint"); });
        require(bytes(f.path / "db.lbdb.wal") == "unowned", "unowned source renamed"); f.unchanged();
    });
    test("rename unowned existing companion", [] {
        Fixture f; f.owned("db.lbdb.wal");
        std::ofstream(f.path / "db.lbdb.wal.checkpoint") << "unowned";
        refused([&] { f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint"); });
        require(bytes(f.path / "db.lbdb.wal.checkpoint") == "unowned", "unowned overwritten"); f.unchanged();
    });
    for (auto endpoint : {"source", "destination"}) {
        test((std::string("rename ") + endpoint + " link").c_str(), [=] {
            Fixture f;
            if (std::string(endpoint) == "source") { fs::create_symlink(f.sentinel, f.path / "db.lbdb.wal"); }
            else { f.owned("db.lbdb.wal"); fs::create_symlink(f.sentinel, f.path / "db.lbdb.wal.checkpoint"); }
            refused([&] { f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint"); }); f.unchanged();
        });
        test((std::string("rename ") + endpoint + " hard alias").c_str(), [=] {
            Fixture f; auto name = std::string(endpoint) == "source" ? "db.lbdb.wal" : "db.lbdb.wal.checkpoint";
            f.owned("db.lbdb.wal");
            if (std::string(endpoint) == "source") { fs::create_hard_link(f.path / name, f.path / "alias"); }
            else { fs::create_hard_link(f.sentinel, f.path / name); }
            refused([&] { f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint"); });
            require(bytes(f.path / "db.lbdb.wal") == "owned", "aliased source changed");
            fs::remove(f.path / (std::string(endpoint) == "source" ? "alias" : name)); f.unchanged();
        });
        test((std::string("rename ") + endpoint + " replaced identity").c_str(), [=] {
            Fixture f; f.owned("db.lbdb.wal"); f.owned("db.lbdb.wal.checkpoint");
            auto name = std::string(endpoint) == "source" ? "db.lbdb.wal" : "db.lbdb.wal.checkpoint";
            fs::rename(f.path / name, f.path / "saved"); std::ofstream(f.path / name) << "replacement";
            refused([&] { f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint"); });
            require(bytes(f.path / name) == "replacement", "replacement changed"); f.unchanged();
        });
    }
    test("remove exact helper companion set and idempotence", [] {
        Fixture f;
        using lbug::storage::StorageUtils;
        const std::vector<std::string> helperNames = {StorageUtils::getWALFilePath("db.lbdb"),
                 StorageUtils::getCheckpointWALFilePath("db.lbdb"), StorageUtils::getShadowFilePath("db.lbdb"),
                 StorageUtils::getTmpFilePath("db.lbdb"), StorageUtils::getCheckpointIntentLockFilePath("db.lbdb"),
                 StorageUtils::getCheckpointApplyLockFilePath("db.lbdb")};
        require(StorageUtils::getCompanionFilePaths("db.lbdb") == helperNames, "companion policy/helper set mismatch");
        for (const auto& name : helperNames) {
            f.owned(name); f.vfs->removeFileIfExists(name); f.vfs->removeFileIfExists(name);
            require(!f.local->fileOrPathExists(name), "removed identity not forgotten");
            f.owned(name); f.vfs->removeFileIfExists(name);
        }
        for (auto name : {"db.lbdb", "other.lbdb.wal", "db.lbdb.tag.tmp", "db.graph.lbdb",
                "db.lbdb.pk_validator.0.tmp", "db.lbdb.checkpoint", "db.lbdb.lock"}) {
            f.owned(name); refused([&] { f.vfs->removeFileIfExists(name); });
            require(bytes(f.path / name) == "owned", "unrelated name removed");
        }
        f.unchanged();
    });
    test("remove forgets ownership after intentional unlink", [] {
        Fixture f; f.owned("db.lbdb.wal"); f.vfs->removeFileIfExists("db.lbdb.wal");
        std::ofstream(f.path / "db.lbdb.wal") << "unowned";
        refused([&] { f.vfs->removeFileIfExists("db.lbdb.wal"); });
        require(bytes(f.path / "db.lbdb.wal") == "unowned", "stale ownership removed a new file"); f.unchanged();
    });
    test("remove refuses remembered disappearance", [] {
        Fixture f; f.owned("db.lbdb.wal"); fs::rename(f.path / "db.lbdb.wal", f.path / "saved");
        refused([&] { f.vfs->removeFileIfExists("db.lbdb.wal"); });
        require(bytes(f.path / "saved") == "owned", "disappeared file changed"); f.unchanged();
    });
    test("opening existing companions never implicitly adopts ownership", [] {
        Fixture f; std::ofstream(f.path / "db.lbdb.wal") << "unowned";
        f.local->openFile("db.lbdb.wal", FileOpenFlags(FileFlags::WRITE | FileFlags::CREATE_IF_NOT_EXISTS));
        refused([&] { f.vfs->removeFileIfExists("db.lbdb.wal"); });
        require(bytes(f.path / "db.lbdb.wal") == "unowned", "open adopted existing companion"); f.unchanged();
    });
    test("remove unowned companion refuses adoption", [] {
        Fixture f; std::ofstream(f.path / "db.lbdb.wal") << "unowned";
        require(f.local->fileOrPathExists("db.lbdb.wal"), "probe");
        refused([&] { f.vfs->removeFileIfExists("db.lbdb.wal"); });
        require(bytes(f.path / "db.lbdb.wal") == "unowned", "probe authorized deletion"); f.unchanged();
    });
    test("remove refuses directory and recursive deletion", [] {
        Fixture f; fs::create_directory(f.path / "db.lbdb.tmp");
        std::ofstream(f.path / "db.lbdb.tmp" / "keep") << "keep";
        refused([&] { f.vfs->removeFileIfExists("db.lbdb.tmp"); });
        require(bytes(f.path / "db.lbdb.tmp" / "keep") == "keep", "recursive removal"); f.unchanged();
    });
    test("remove link hard alias replaced identity", [] {
        Fixture f; fs::create_symlink(f.sentinel, f.path / "db.lbdb.wal");
        refused([&] { f.vfs->removeFileIfExists("db.lbdb.wal"); }); fs::remove(f.path / "db.lbdb.wal");
        f.owned("db.lbdb.wal"); fs::create_hard_link(f.path / "db.lbdb.wal", f.path / "alias");
        refused([&] { f.vfs->removeFileIfExists("db.lbdb.wal"); }); fs::remove(f.path / "alias");
        fs::rename(f.path / "db.lbdb.wal", f.path / "saved");
        std::ofstream(f.path / "db.lbdb.wal") << "replacement";
        refused([&] { f.vfs->removeFileIfExists("db.lbdb.wal"); });
        require(bytes(f.path / "db.lbdb.wal") == "replacement", "replaced file removed"); f.unchanged();
    });
    test("rename remove and probe real ancestor swap", [] {
        Fixture f; f.owned("db.lbdb.wal");
        fs::rename(f.dir / "parent", f.dir / "old-parent"); fs::create_directories(f.path);
        std::ofstream(f.path / "db.lbdb.wal") << "replacement";
        refused([&] { f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint"); });
        refused([&] { f.vfs->removeFileIfExists("db.lbdb.wal"); });
        refused([&] { f.vfs->fileOrPathExists("db.lbdb.wal"); });
        refused([&] { f.vfs->syncDirectoryForFile("db.lbdb.wal"); });
        refused([&] { f.vfs->adoptCompanionFiles(); });
        refused([&] { f.vfs->createPKValidatorSpillFile("db.lbdb"); });
        const auto heldPath = f.dir / "old-parent" / "root";
        require(bytes(heldPath / "db.lbdb.wal") == "owned", "old ancestor changed");
        require(std::distance(fs::directory_iterator(heldPath), fs::directory_iterator()) == 1, "unsafe ancestor allowed temp creation");
        f.unchanged();
    });
    test("rename remove endpoints validate plain names", [] {
        Fixture f; f.owned("db.lbdb.wal");
        for (auto name : {std::string(""), std::string(".."), std::string("nested/db.lbdb.wal"),
                f.sentinel.string(), (f.dir / "outside" / "renamed").string(), std::string("db.lbdb.wal\0x", 14)}) {
            refused([&] { f.vfs->renameFile(name, "db.lbdb.wal.checkpoint"); });
            refused([&] { f.vfs->renameFile("db.lbdb.wal", name); });
            refused([&] { f.vfs->removeFileIfExists(name); });
        }
        require(bytes(f.path / "db.lbdb.wal") == "owned", "invalid endpoint moved source");
        f.unchanged();
    });
    test("directory durability uses held descriptor after each name change", [] {
        Fixture f; activeFixture = &f; syncCount = 0; injectedError = 0;
        NativeHook hook(observe);
        f.owned("db.lbdb.wal"); require(syncCount == 1, "create missing directory sync");
        f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint"); require(syncCount == 2, "rename missing directory sync");
        f.vfs->removeFileIfExists("db.lbdb.wal.checkpoint"); require(syncCount == 3, "unlink missing directory sync");
        auto temp = f.vfs->createPKValidatorSpillFile("db.lbdb"); require(syncCount == 4, "bulk temp missing directory sync");
        const auto name = temp->path; temp.reset(); f.vfs->removeFileIfExists(name);
        require(syncCount == 5, "bulk temp unlink missing directory sync");
        f.vfs->syncDirectoryForFile("db.lbdb"); require(syncCount == 6, "WAL sync reopened a pathname"); f.unchanged();
    });
    for (auto operation : {"create", "bulk create", "rename", "remove", "WAL sync"}) {
        test((std::string("directory durability errors fail closed on ") + operation).c_str(), [=] {
            Fixture f; f.owned("db.lbdb.wal"); activeFixture = &f; syncCount = 0; injectedError = EIO;
            NativeHook hook(observe);
            bool typed = false;
            try {
                if (std::string(operation) == "create") { f.owned("db.lbdb.shadow"); }
                if (std::string(operation) == "bulk create") { f.vfs->createPKValidatorSpillFile("db.lbdb"); }
                if (std::string(operation) == "rename") { f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint"); }
                if (std::string(operation) == "remove") { f.vfs->removeFileIfExists("db.lbdb.wal"); }
                if (std::string(operation) == "WAL sync") { f.vfs->syncDirectoryForFile("db.lbdb"); }
            } catch (const IOException&) { typed = true; }
            require(syncCount == 1 && typed, "durability failure was ignored"); f.unchanged();
        });
    }
    for (auto operation : {"probe", "open", "rename", "remove", "adopt", "bulk create", "directory sync", "held WAL sync"}) {
        test((std::string("poison refuses later ") + operation).c_str(), [=] {
            Fixture f; f.owned("db.lbdb.wal");
            auto held = f.local->openFile("db.lbdb.wal", FileOpenFlags(FileFlags::READ_ONLY | FileFlags::WRITE));
            activeFixture = &f; syncCount = 0; injectedError = EIO;
            { NativeHook hook(observe); refused([&] { f.vfs->syncDirectoryForFile("db.lbdb"); }); }
            require(syncCount == 1, "directory fault was not reached");
            // Remove the injected fault: refusal must now come from shared poisoned state.
            fileSyncCalls = 0; NativeHook noFileSync(countFileSync);
            refused([&] {
                if (std::string(operation) == "probe") { f.vfs->fileOrPathExists("db.lbdb.wal"); }
                if (std::string(operation) == "open") { f.vfs->openFile("db.lbdb.wal", FileOpenFlags(FileFlags::WRITE | FileFlags::CREATE_IF_NOT_EXISTS)); }
                if (std::string(operation) == "rename") { f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint"); }
                if (std::string(operation) == "remove") { f.vfs->removeFileIfExists("db.lbdb.wal"); }
                if (std::string(operation) == "adopt") { f.vfs->adoptCompanionFiles(); }
                if (std::string(operation) == "bulk create") { f.vfs->createPKValidatorSpillFile("db.lbdb"); }
                if (std::string(operation) == "directory sync") { f.vfs->syncDirectoryForFile("db.lbdb"); }
                if (std::string(operation) == "held WAL sync") { held->syncFile(); }
            });
            require(fileSyncCalls == 0, "poisoned capability reached file sync syscall");
            require(bytes(f.path / "db.lbdb.wal") == "owned", "poisoned capability changed WAL name or bytes");
            require(std::distance(fs::directory_iterator(f.path), fs::directory_iterator()) == 1, "poisoned capability created a name");
            f.unchanged();
        });
    }
    test("sync identity error permanently poisons held WAL handle", [] {
        Fixture f; f.owned("db.lbdb.wal");
        auto held = f.local->openFile("db.lbdb.wal", FileOpenFlags(FileFlags::READ_ONLY | FileFlags::WRITE));
        activeFixture = &f;
        { NativeHook hook(swapAncestorDuringSync); refused([&] { f.vfs->syncDirectoryForFile("db.lbdb"); }); }
        fs::remove_all(f.dir / "parent"); fs::rename(f.dir / "old-parent", f.dir / "parent");
        refused([&] { held->syncFile(); });
        refused([&] { f.vfs->fileOrPathExists("db.lbdb.wal"); }); f.unchanged();
    });
    test("fresh capability establishes directory durability before first file sync", [] {
        Fixture f; f.owned("db.lbdb.wal");
        auto fresh = RootDirectory::open(f.path.string()); LocalFileSystem local("db.lbdb", fresh);
        auto held = local.openFile("db.lbdb.wal", FileOpenFlags(FileFlags::READ_ONLY | FileFlags::WRITE));
        activeFixture = &f; syncCount = 0; fileSyncCalls = 0; injectedError = 0;
        NativeHook hook(checkFreshSyncOrdering);
        held->syncFile(); held->syncFile();
        require(syncCount == 1 && fileSyncCalls == 2, "fresh first sync missing or repeated"); f.unchanged();
    });
    test("fresh first directory sync failure poisons capability and held file", [] {
        Fixture f; f.owned("db.lbdb.wal");
        auto fresh = RootDirectory::open(f.path.string()); LocalFileSystem local("db.lbdb", fresh);
        auto held = local.openFile("db.lbdb.wal", FileOpenFlags(FileFlags::READ_ONLY | FileFlags::WRITE));
        activeFixture = &f; syncCount = 0; injectedError = EIO;
        { NativeHook hook(observe); refused([&] { held->syncFile(); }); }
        require(syncCount == 1, "fresh directory sync not reached");
        refused([&] { held->syncFile(); }); refused([&] { local.fileOrPathExists("db.lbdb.wal"); }); f.unchanged();
    });
    test("file sync refuses poison arriving during the held handle sync", [] {
        Fixture f; f.owned("db.lbdb.wal");
        auto held = f.local->openFile("db.lbdb.wal", FileOpenFlags(FileFlags::READ_ONLY | FileFlags::WRITE));
        activeFixture = &f; fileSyncCalls = 0; NativeHook hook(poisonDuringFileSync);
        refused([&] { held->syncFile(); }); require(fileSyncCalls == 1, "held sync interleave not reached"); f.unchanged();
    });
    test("directory sync refuses changed ancestors before syscall", [] {
        Fixture f; fs::rename(f.dir / "parent", f.dir / "old-parent"); fs::create_directories(f.path);
        syncCount = 0; NativeHook hook(countSync);
        refused([&] { f.vfs->syncDirectoryForFile("db.lbdb"); });
        require(syncCount == 0, "unsafe ancestor reached directory syscall"); f.unchanged();
    });
    test("unlink syscall errors fail closed without losing ownership", [] {
        Fixture f; f.owned("db.lbdb.wal"); nativeCalls = 0;
        { NativeHook hook(unlinkFailure); bool typed = false;
          try { f.vfs->removeFileIfExists("db.lbdb.wal"); } catch (const IOException&) { typed = true; }
          require(typed && nativeCalls == 1, "unlink error ignored"); }
        require(bytes(f.path / "db.lbdb.wal") == "owned", "failed unlink changed bytes");
        f.vfs->removeFileIfExists("db.lbdb.wal"); require(!fs::exists(f.path / "db.lbdb.wal"), "failed unlink lost ownership"); f.unchanged();
    });
    test("directory sync revalidates identity after syscall", [] {
        Fixture f; activeFixture = &f; NativeHook hook(swapAncestorDuringSync);
        refused([&] { f.vfs->syncDirectoryForFile("db.lbdb"); }); f.unchanged();
    });
    test("atomic no-replace catches a destination appearing after probe", [] {
        Fixture f; f.owned("db.lbdb.wal"); NativeHook hook(raceDestination);
        refused([&] { f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint"); });
        require(bytes(f.path / "db.lbdb.wal") == "owned", "racing publication lost source");
        require(bytes(f.path / "db.lbdb.wal.checkpoint") == "intruder", "racing destination overwritten"); f.unchanged();
    });
    test("atomic no-replace unsupported errors have no fallback", [] {
        Fixture f; f.owned("db.lbdb.wal"); NativeHook hook(unsupportedRename);
        for (auto error : {EINVAL, ENOSYS, ENOTSUP}) {
            injectedError = error; nativeCalls = 0; bool typed = false;
            try { f.vfs->renameFile("db.lbdb.wal", "db.lbdb.wal.checkpoint"); }
            catch (const IOException&) { typed = true; }
            require(typed && nativeCalls == 1, "unsupported atomic rename accepted");
            require(bytes(f.path / "db.lbdb.wal") == "owned" && !fs::exists(f.path / "db.lbdb.wal.checkpoint"), "unsupported rename mutated names");
        }
        f.unchanged();
    });
    test("recovery adoption requires companion owner", [] {
        Fixture f; std::ofstream(f.path / "db.lbdb.wal") << "unowned";
        nativeCalls = 0; NativeHook hook(foreignOwner);
        refused([&] { f.vfs->adoptCompanionFiles(); });
        require(nativeCalls == 1, "owner check not exercised");
        require(bytes(f.path / "db.lbdb.wal") == "unowned", "foreign companion changed"); f.unchanged();
    });
    test("recovery adoption validates fixed companions and identity", [] {
        Fixture f; std::ofstream(f.path / "db.lbdb.wal") << "recovery";
        f.vfs->adoptCompanionFiles(); f.vfs->removeFileIfExists("db.lbdb.wal");
        require(!fs::exists(f.path / "db.lbdb.wal"), "validated recovery companion not owned");
        for (auto kind : {"link", "alias", "directory"}) {
            if (std::string(kind) == "link") { fs::create_symlink(f.sentinel, f.path / "db.lbdb.shadow"); }
            if (std::string(kind) == "alias") { fs::create_hard_link(f.sentinel, f.path / "db.lbdb.shadow"); }
            if (std::string(kind) == "directory") { fs::create_directory(f.path / "db.lbdb.shadow"); }
            refused([&] { f.vfs->adoptCompanionFiles(); }); fs::remove(f.path / "db.lbdb.shadow");
        }
        f.unchanged();
    });
    test("bulk temp creation identity interleave fails before registration", [] {
        Fixture f; NativeHook hook(swapCreatedTemp);
        refused([&] { f.vfs->createPKValidatorSpillFile("db.lbdb"); });
        require(bytes(f.path / "db.lbdb.pk_validator.0.tmp") == "replacement", "temp replacement bytes changed");
        refused([&] { f.vfs->removeFileIfExists("db.lbdb.pk_validator.0.tmp"); });
        fs::remove(f.path / "db.lbdb.pk_validator.0.tmp");
        fs::rename(f.path / "original-temp", f.path / "db.lbdb.pk_validator.0.tmp");
        f.local->openFile("db.lbdb.pk_validator.0.tmp", FileOpenFlags(FileFlags::READ_ONLY));
        refused([&] { f.vfs->removeFileIfExists("db.lbdb.pk_validator.0.tmp"); }); f.unchanged();
    });
    test("bulk temp identity must survive creation until content open", [] {
        Fixture f; NativeHook hook(swapTempAfterSync);
        refused([&] { f.vfs->createPKValidatorSpillFile("db.lbdb"); });
        require(bytes(f.path / "db.lbdb.pk_validator.0.tmp") == "replacement", "replacement content accessed"); f.unchanged();
    });
    test("bulk temp unlink revokes its registration", [] {
        Fixture f; auto temp = f.vfs->createPKValidatorSpillFile("db.lbdb");
        auto name = temp->path; temp.reset(); f.vfs->removeFileIfExists(name);
        f.owned(name); refused([&] { f.vfs->removeFileIfExists(name); });
        require(bytes(f.path / name) == "owned", "unregistered recreation removed"); f.unchanged();
    });
    test("bulk temp allocation refuses unrelated database stem", [] {
        Fixture f; refused([&] { f.vfs->createPKValidatorSpillFile("other.lbdb"); });
        require(fs::is_empty(f.path), "other stem allocated"); f.unchanged();
    });
#endif
    std::cout << "cases: " << count << " failures: " << failures << '\n';
    return failures ? 1 : 0;
}
