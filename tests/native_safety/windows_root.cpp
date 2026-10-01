#ifdef _WIN32
#include "windows_test_support.h"
#include "common/exception/io.h"
#include "common/file_system/local_file_system.h"
#include "main/connection.h"
#include "main/database.h"
#include "main/query_result.h"
#include "processor/result/flat_tuple.h"
#include <functional>
#include <iostream>
using namespace windows_test;
using namespace lbug::common;

template<class F> void refuses(F action, const std::string& message, const std::string& reason = "") {
    bool rejected = false;
    try { action(); } catch (const IOException& e) {
        rejected = reason.empty() || std::string(e.what()).find(reason) != std::string::npos;
    }
    require(rejected, message);
}

#ifdef MAESTRO_NATIVE_OPEN_TEST
namespace lbug::common {
extern int (*maestroWindowsCall)(const char*, void*, void*);
}
namespace {
std::function<int(const char*, void*, void*)> intercept;
int bridge(const char* op, void* handle, void* info) { return intercept(op, handle, info); }
struct Hook {
    explicit Hook(std::function<int(const char*, void*, void*)> f) { intercept = std::move(f); maestroWindowsCall = bridge; }
    ~Hook() { maestroWindowsCall = nullptr; intercept = {}; }
};
bool leaf(void* handle, const wchar_t* name) {
    wchar_t path[32768];
    const auto size = GetFinalPathNameByHandleW(handle, path, 32768, FILE_NAME_NORMALIZED | VOLUME_NAME_GUID);
    if (!size || size >= 32768) return false;
    return fs::path(std::wstring(path, size)).filename() == name;
}
}
#endif

int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string(argv[1]) == "--ancestor") {
            auto target = fs::path(argv[2]);
            auto renamed = target; renamed += ".moved";
            SetLastError(ERROR_SUCCESS);
            const BOOL moved = MoveFileExW(target.c_str(), renamed.c_str(), 0);
            auto renameError = GetLastError();
            if (moved) { MoveFileExW(renamed.c_str(), target.c_str(), 0); return 1; }
            SetLastError(ERROR_SUCCESS);
            const BOOL removed = RemoveDirectoryW(target.c_str());
            auto deleteError = GetLastError();
            if (removed) { fs::create_directory(target); return 1; }
            // An empty held root exercises real deletion; nonempty ancestors also
            // need a DELETE-access open to isolate sharing from directory contents.
            auto deleting = CreateFileW(target.c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            auto openError = GetLastError();
            if (deleting != INVALID_HANDLE_VALUE) { CloseHandle(deleting); return 1; }
            std::cout << "rename_error=" << renameError << " delete_error=" << deleteError << " delete_open_error=" << openError << '\n';
            return renameError == ERROR_SHARING_VIOLATION && openError == ERROR_SHARING_VIOLATION &&
                (deleteError == ERROR_SHARING_VIOLATION || deleteError == ERROR_DIR_NOT_EMPTY) ? 0 : 2;
        }
        require(argc >= 2, "SETUP scratch argument");
        user(true);
        auto parent = fs::path(argv[1]) / ("root-" + std::to_string(GetCurrentProcessId()));
        fs::create_directories(parent / "root");
        auto path = parent / "root";
        makePrivate(path);
        auto outside = parent / "outside";
        std::ofstream(outside) << "outside sentinel";
        Snapshot sentinel(outside);
        auto config = lbug::main::SystemConfig(16 * 1024 * 1024, 1);
        config.maxDBSize = 64 * 1024 * 1024;
        config.forceCheckpointOnClose = false;
        config.autoCheckpoint = false;
        {
            lbug::main::Database db(utf8(path / "db.lbdb"), config);
            lbug::main::Connection connection(&db);
            require(connection.query("CREATE NODE TABLE Item(id INT64, PRIMARY KEY(id))")->isSuccess(), "SETUP table");
            require(connection.query("CREATE (:Item {id: 7})")->isSuccess(), "SETUP row");
            require(connection.query("CHECKPOINT")->isSuccess(), "SETUP checkpoint");
        }
        config.readOnly = true;
        unsigned failures = 0, count = 0;
        auto test = [&](const char* name, const std::function<void()>& action) {
            if (argc > 2 && std::string(argv[2]) != name) return;
            ++count;
            auto listing = entries(path);
            try {
                action();
                sentinel.unchanged(outside);
                require(listing == entries(path), "read-only directory snapshot changed");
                std::cout << "PASS " << name << '\n';
            } catch (const std::exception& e) {
                ++failures; std::cerr << "FAIL " << name << ": " << e.what() << '\n';
            }
        };
        test("rooted_read_only_roundtrip", [&] {
            lbug::main::Database db(RootDirectory::open(utf8(path)), "db.lbdb", config);
            lbug::main::Connection connection(&db);
            auto result = connection.query("MATCH (i:Item) RETURN i.id");
            require(result->isSuccess() && result->hasNext() && result->getNext()->getValue(0)->getValue<int64_t>() == 7,
                "rooted read-only query returned wrong row");
        });
        test("root_kept_after_wrapper_drop", [&] {
            auto root = RootDirectory::open(utf8(path));
            lbug::main::Database db(root, "db.lbdb", config);
            root.reset();
            lbug::main::Connection connection(&db);
            require(connection.query("MATCH (i:Item) RETURN i.id")->isSuccess(), "database lost held root");
        });
        test("missing_root_not_created", [&] {
            bool refused = false;
            try { RootDirectory::open(utf8(parent / "missing")); } catch (const IOException&) { refused = true; }
            require(refused && !fs::exists(parent / "missing"), "missing root created or accepted");
        });
        test("writable_constructor_refuses_before_io", [&] {
            auto root = RootDirectory::open(utf8(path));
            auto writable = config; writable.readOnly = false;
            refuses([&] { lbug::main::Database db(root, "missing-write.lbdb", writable); },
                "writable Windows construction accepted", "writable");
            require(!fs::exists(path / "missing-write.lbdb"), "writable constructor performed I/O");
        });
        test("failed_read_and_seek_propagate", [&] {
            LocalFileSystem local("db.lbdb");
            LocalFileInfo invalid("invalid", nullptr, &local);
            char buffer[8]{};
            refuses([&] { invalid.readFile(buffer, sizeof(buffer)); }, "ReadFile failure ignored", "read");
            refuses([&] { invalid.seek(0, FILE_BEGIN); }, "SetFilePointerEx failure ignored", "pointer");
        });
        test("held_ancestor_cannot_move", [&] {
            auto ancestors = parent / "ancestor";
            auto emptyRoot = ancestors / "empty";
            fs::create_directories(emptyRoot);
            makePrivate(emptyRoot);
            {
                auto root = RootDirectory::open(utf8(emptyRoot));
                for (auto p : {emptyRoot, ancestors, parent}) {
                    require(process(L"\"" + executable() + L"\" --ancestor \"" + p.wstring() + L"\" unused") == 0,
                        "second process moved or deleted held ancestor");
                }
            }
            fs::remove_all(ancestors);
        });
        test("trusted_initial_junction_and_unicode_long_root", [&] {
            auto unicode = parent / fs::path(L"unicode-\u03bb-\u6f22");
            for (int i = 0; i < 5; ++i) unicode /= std::wstring(55, L'a' + i);
            // Extended paths are fixture setup, not an engine pathname fallback.
            auto extended = fs::path(L"\\\\?\\" + unicode.wstring());
            fs::create_directories(extended);
            makePrivate(extended);
            std::ofstream(extended / L"normal-\u03bb") << "unicode";
            {
            auto cap = RootDirectory::open(utf8(extended));
            LocalFileSystem local("db.lbdb", cap);
            auto f = local.openFile("normal-\xce\xbb", FileOpenFlags(FileFlags::READ_ONLY));
            char value[8]{};
            require(f->readFile(value, sizeof(value)) == 7 && std::string(value, 7) == "unicode", "long Unicode root read");
            }
            auto alias = parent / "trusted-junction";
            junction(alias, path);
            {
                auto root = RootDirectory::open(utf8(alias));
                LocalFileSystem linked("db.lbdb", root);
                linked.openFile("db.lbdb", FileOpenFlags(FileFlags::READ_ONLY));
            }
            fs::remove(alias);
            fs::remove_all(fs::path(L"\\\\?\\" + (parent / fs::path(L"unicode-\u03bb-\u6f22")).wstring()));
        });
        test("read_modes_refuse_links_kinds_and_aliases", [&] {
            auto fixtures = parent / "kinds";
            fs::create_directory(fixtures); makePrivate(fixtures);
            std::ofstream(fixtures / "normal") << "normal";
            fs::create_directory(fixtures / "directory");
            require(CreateHardLinkW((fixtures / "hardlink").c_str(), outside.c_str(), nullptr), "SETUP mandatory hardlink");
            require(CreateSymbolicLinkW((fixtures / "filelink").c_str(), outside.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE), "SETUP mandatory file symlink");
            junction(fixtures / "junction", parent);
            {
                auto root = RootDirectory::open(utf8(fixtures));
                LocalFileSystem local("normal", root);
                for (auto name : {"directory", "filelink", "hardlink", "junction", "NORMAL"}) {
                    for (auto lock : {FileLockType::NO_LOCK, FileLockType::READ_LOCK}) {
                        refuses([&] { local.openFile(name, FileOpenFlags(FileFlags::READ_ONLY, lock)); }, "unsafe read mode accepted");
                    }
                    refuses([&] { local.fileOrPathExists(name); }, "unsafe metadata accepted");
                }
                require(!local.fileOrPathExists("missing"), "unobserved missing file is not absent");
                for (auto flags : std::vector<int>{FileFlags::WRITE, FileFlags::READ_ONLY | FileFlags::WRITE,
                        FileFlags::WRITE | FileFlags::CREATE_IF_NOT_EXISTS,
                        FileFlags::WRITE | FileFlags::CREATE_AND_TRUNCATE_IF_EXISTS}) {
                    refuses([&] { local.openFile("normal", FileOpenFlags(flags)); }, "writable rooted open accepted", "writable");
                }
                // A disappearing previously observed name is an error, not absence.
                require(local.fileOrPathExists("normal"), "regular metadata failed");
                fs::remove(fixtures / "normal");
                refuses([&] { local.fileOrPathExists("normal"); }, "remembered disappearance reported absent");
            }
            fs::remove(fixtures / "hardlink");
            fs::remove_all(fixtures);
        });
        test("active_wal_read_only_replay_has_no_mutations", [&] {
            auto fixture = parent / "wal";
            fs::create_directory(fixture); makePrivate(fixture);
            auto writable = config; writable.readOnly = false;
            {
                lbug::main::Database db(utf8(fixture / "wal.lbdb"), writable);
                lbug::main::Connection connection(&db);
                require(connection.query("CREATE NODE TABLE Item(id INT64, PRIMARY KEY(id))")->isSuccess(), "SETUP WAL table");
                require(connection.query("CREATE (:Item {id: 9})")->isSuccess(), "SETUP WAL row");
            }
            require(fs::exists(fixture / "wal.lbdb.wal"), "SETUP active WAL missing");
            auto listing = entries(fixture);
            std::vector<std::pair<fs::path, Snapshot>> snapshots;
            for (const auto& entry : fs::directory_iterator(fixture)) snapshots.emplace_back(entry.path(), Snapshot(entry.path()));
            {
                auto root = RootDirectory::open(utf8(fixture));
                lbug::main::Database db(root, "wal.lbdb", config);
                lbug::main::Connection connection(&db);
                auto result = connection.query("MATCH (i:Item) RETURN i.id");
                require(result->isSuccess() && result->hasNext() && result->getNext()->getValue(0)->getValue<int64_t>() == 9,
                    "active WAL read-only replay returned wrong row");
            }
            require(listing == entries(fixture), "read-only replay created or removed a sidecar");
            for (auto& [file, snapshot] : snapshots) snapshot.unchanged(file);
            fs::remove_all(fixture);
        });
        test("private_root_policy_is_wired_into_acquisition_and_reads", [&] {
            auto fixture = parent / "security";
            fs::create_directory(fixture); makePrivate(fixture);
            auto broad = [&](const fs::path& p) {
                auto h = hold(CreateFileW(p.c_str(), WRITE_DAC, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
                PSECURITY_DESCRIPTOR sd = nullptr;
                auto sddl = "D:P(A;OICI;FA;;;" + user() + ")(A;OICI;FA;;;WD)";
                require(ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr), "SETUP unsafe DACL");
                BOOL present, defaulted; PACL acl;
                require(GetSecurityDescriptorDacl(sd, &present, &acl, &defaulted), "SETUP unsafe DACL decode");
                auto error = SetSecurityInfo(h.get(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                    nullptr, nullptr, acl, nullptr);
                LocalFree(sd); require(error == ERROR_SUCCESS, "SETUP unsafe DACL apply");
            };
            std::ofstream(fixture / "child") << "private";
            {
                auto root = RootDirectory::open(utf8(fixture));
                broad(fixture / "child");
                refuses([&] { root->openFile("child", 0); }, "unsafe child ACL accepted", "maestro-private-root/1");
                refuses([&] { root->probeRegularFile("child"); }, "unsafe child metadata accepted", "maestro-private-root/1");
            }
            broad(fixture);
            refuses([&] { RootDirectory::open(utf8(fixture)); }, "unsafe root ACL accepted", "maestro-private-root/1");
            makePrivate(fixture); fs::remove_all(fixture);
        });
        test("read_only_mutators_remain_closed", [&] {
            auto root = RootDirectory::open(utf8(path));
            LocalFileSystem local("db.lbdb", root);
            auto file = local.openFile("db.lbdb", FileOpenFlags(FileFlags::READ_ONLY));
            refuses([&] { file->writeFile(reinterpret_cast<const uint8_t*>("x"), 1, 0); }, "rooted write accepted");
            refuses([&] { file->truncate(0); }, "rooted truncate accepted");
            refuses([&] { file->syncFile(); }, "rooted sync accepted");
            refuses([&] { local.syncDirectoryForFile("db.lbdb"); }, "directory sync became a no-op");
            refuses([&] { local.adoptCompanionFiles(); }, "read-only adoption accepted");
            refuses([&] { local.createPKValidatorSpillFile("db.lbdb"); }, "read-only temp creation accepted");
            refuses([&] { local.renameFile("db.lbdb", "other"); }, "read-only rename accepted");
            refuses([&] { local.removeFileIfExists("db.lbdb.wal"); }, "read-only unlink accepted");
            refuses([&] { local.copyFile("db.lbdb", "copy"); }, "read-only copy accepted");
            refuses([&] { local.overwriteFile("db.lbdb", "copy"); }, "read-only overwrite accepted");
            refuses([&] { local.createDir("dir"); }, "read-only directory create accepted");
            refuses([&] { local.glob(nullptr, "*"); }, "rooted glob accepted");
            refuses([&] { local.expandPath(nullptr, "db.lbdb"); }, "rooted expansion accepted");
        });
#ifdef MAESTRO_NATIVE_OPEN_TEST
        test("unsafe_windows_name_never_calls_native_open", [&] {
            auto root = RootDirectory::open(utf8(path));
            unsigned calls = 0;
            Hook hook([&](const char* op, void*, void*) {
                if (std::string(op) == "native-open") ++calls;
                return 0;
            });
            for (const auto& name : {std::string(""), std::string("."), std::string(".."), std::string("a/b"),
                    std::string("a\\b"), std::string("a:b"), std::string("a*"), std::string("a?"), std::string("a<"),
                    std::string("a>"), std::string("a|"), std::string("a\""), std::string("a."), std::string("a "),
                    std::string("NUL"), std::string("con.txt"), std::string("COM1"), std::string("LPT9.log"),
                    std::string("CONIN$"), std::string("COM\xc2\xb9"), std::string("a\0b", 3), std::string("a\x01"),
                    std::string("bad-\xff"), std::string(256, 'a')}) {
                refuses([&] { root->probeRegularFile(name); }, "unsafe Windows name accepted");
                refuses([&] { root->openFile(name, 0); }, "unsafe Windows open name accepted");
                require(calls == 0, "unsafe Windows name reached native open");
            }
        });
        test("read_probe_identity_interleave_refuses", [&] {
            for (bool volume : {false, true}) {
                auto root = RootDirectory::open(utf8(path));
                LocalFileSystem local("db.lbdb", root);
                require(local.fileOrPathExists("db.lbdb"), "SETUP observed file");
                unsigned content = 0;
                Hook hook([&](const char* op, void* h, void* info) {
                    if (std::string(op) == "identity" && leaf(h, L"db.lbdb")) {
                        auto id = static_cast<FILE_ID_INFO*>(info);
                        if (volume) ++id->VolumeSerialNumber;
                        else id->FileId.Identifier[0] ^= 1;
                    }
                    if (std::string(op) == "read") ++content;
                    return 0;
                });
                refuses([&] { local.openFile("db.lbdb", FileOpenFlags(FileFlags::READ_ONLY))->readFile(nullptr, 0); },
                    volume ? "changed volume accepted" : "changed file ID accepted", "identity");
                require(content == 0, "identity refusal reached content API");
            }
        });
        test("held_ancestor_identity_interleave_refuses", [&] {
            for (bool volume : {false, true}) {
                auto root = RootDirectory::open(utf8(path));
                Hook hook([&](const char* op, void* h, void* info) {
                    if (std::string(op) == "identity" && leaf(h, L"root")) {
                        auto id = static_cast<FILE_ID_INFO*>(info);
                        if (volume) ++id->VolumeSerialNumber;
                        else id->FileId.Identifier[0] ^= 1;
                    }
                    return 0;
                });
                refuses([&] { root->probeRegularFile("db.lbdb"); },
                    volume ? "changed ancestor volume accepted" : "changed ancestor ID accepted", "ancestor identity");
            }
        });
        test("after_probe_identity_interleave_refuses", [&] {
            auto root = RootDirectory::open(utf8(path));
            LocalFileSystem local("db.lbdb", root);
            bool substitute = false; unsigned content = 0;
            Hook hook([&](const char* op, void* h, void* info) {
                if (std::string(op) == "after-probe") substitute = true;
                if (substitute && std::string(op) == "identity" && leaf(h, L"db.lbdb"))
                    static_cast<FILE_ID_INFO*>(info)->FileId.Identifier[0] ^= 1;
                if (std::string(op) == "read") ++content;
                return 0;
            });
            refuses([&] { local.openFile("db.lbdb", FileOpenFlags(FileFlags::READ_ONLY))->readFile(nullptr, 0); },
                "changed identity between probe and read accepted", "identity");
            require(substitute && content == 0, "after-probe guard did not isolate content access");
        });
        test("junction_below_root_refuses", [&] {
            auto fixture = parent / "reparse";
            fs::create_directory(fixture); makePrivate(fixture);
            junction(fixture / "junction", path);
            {
                auto root = RootDirectory::open(utf8(fixture));
                Hook hook([](const char* op, void* h, void* info) {
                    if (leaf(h, L"junction")) {
                        // Isolate the reparse guard from the independent directory guard.
                        if (std::string(op) == "attributes") static_cast<FILE_ATTRIBUTE_TAG_INFO*>(info)->FileAttributes &= ~FILE_ATTRIBUTE_DIRECTORY;
                        if (std::string(op) == "standard") static_cast<FILE_STANDARD_INFO*>(info)->Directory = FALSE;
                    }
                    return 0;
                });
                refuses([&] { root->probeRegularFile("junction"); }, "junction reparse attribute accepted", "reparse");
            }
            fs::remove(fixture / "junction"); fs::remove(fixture);
        });
        test("no_follow_open_is_required", [&] {
            auto fixture = parent / "nofollow";
            fs::create_directory(fixture); makePrivate(fixture);
            std::ofstream(fixture / "target") << "safe";
            require(CreateSymbolicLinkW((fixture / "filelink").c_str(), (fixture / "target").c_str(),
                SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE), "SETUP mandatory no-follow file link");
            {
                auto root = RootDirectory::open(utf8(fixture));
                unsigned opens = 0;
                Hook hook([&](const char* op, void*, void* info) {
                    if (std::string(op) == "native-open") {
                        ++opens;
                        require((*static_cast<unsigned long*>(info) & 0x00200000) != 0, "native open omitted FILE_OPEN_REPARSE_POINT");
                    }
                    return 0;
                });
                refuses([&] { root->probeRegularFile("filelink"); }, "file link accepted", "reparse");
                require(opens > 0, "no-follow test did not reach NtCreateFile");
            }
            fs::remove_all(fixture);
        });
        test("link_count_is_required", [&] {
            auto root = RootDirectory::open(utf8(path));
            Hook hook([](const char* op, void* h, void* info) {
                if (std::string(op) == "standard" && leaf(h, L"db.lbdb"))
                    static_cast<FILE_STANDARD_INFO*>(info)->NumberOfLinks = 2;
                return 0;
            });
            refuses([&] { root->probeRegularFile("db.lbdb"); }, "multi-link file accepted", "links");
        });
        test("writable_constructor_never_probes", [&] {
            auto root = RootDirectory::open(utf8(path));
            unsigned calls = 0;
            Hook hook([&](const char* op, void*, void*) { if (std::string(op) == "native-open") ++calls; return 0; });
            auto writable = config; writable.readOnly = false;
            refuses([&] { lbug::main::Database db(root, "missing-write.lbdb", writable); },
                "writable constructor guard missing", "writable");
            require(calls == 0, "writable constructor probed before refusal");
        });
#endif
        require(count > 0, "mandatory root tests did not run");
        fs::remove_all(parent);
        std::cout << "windows_root groups=" << count << " failures=" << failures << '\n';
        return failures ? 1 : 0;
    } catch (const std::exception& e) { std::cerr << "SETUP " << e.what() << '\n'; return 2; }
}

#endif
