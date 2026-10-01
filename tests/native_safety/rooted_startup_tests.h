#pragma once
#include "rooted_test_support.h"
#include "main/client_context.h"
#include "main/connection.h"
#include "main/database.h"
#include "storage/partition_storage_registry.h"
#include "storage/storage_manager.h"
#include <sys/wait.h>
using namespace lbug::main;
using namespace lbug::storage;
namespace lbug::common {
extern void (*maestroBeforeRestrictedOpen)(int directory, const char* name);
}
SystemConfig config();
void prepare(Connection& connection);
void query(Connection& connection, const std::string& sql);
int64_t rows(Connection& connection);
void crashCheckpoint(Fixture& f, const std::string& phase, bool recover);
extern int recoveryDataOpens;
void countRecoveryDataOpens(int, const char* name);
struct RecoveryOpenCounter {
    RecoveryOpenCounter() { recoveryDataOpens = 0; maestroBeforeRestrictedOpen = countRecoveryDataOpens; }
    ~RecoveryOpenCounter() { maestroBeforeRestrictedOpen = nullptr; }
};

int startupAdopts = 0;
int countStartupAdopts(const char* operation, int, const char*, struct stat*) {
    if (std::string(operation) == "adopt") { ++startupAdopts; }
    return 0;
}
bool firstAdoptAfterOpen = false;
bool firstAdoptWithWriteLock = false;
int observeStartupAdoption(const char* operation, int directory, const char* name, struct stat* info) {
    countStartupAdopts(operation, directory, name, info);
    if (std::string(operation) != "adopt" || startupAdopts != 1) { return 0; }
    firstAdoptAfterOpen = recoveryDataOpens == 1;
    // A different process must inspect the lock: F_GETLK ignores the caller's own locks.
    // Never open/close this inode in the parent, which would release its POSIX lock.
    const auto parent = getpid();
    const auto child = fork();
    require(child >= 0, "startup lock observer fork");
    if (child == 0) {
        const auto fd = openat(directory, "ordered.lbdb", O_RDWR);
        struct flock lock{};
        lock.l_type = F_WRLCK; lock.l_whence = SEEK_SET;
        const auto locked = fd >= 0 && fcntl(fd, F_GETLK, &lock) == 0 &&
            lock.l_type == F_WRLCK && lock.l_pid == parent;
        if (fd >= 0) { close(fd); }
        _exit(locked ? 0 : 1);
    }
    int status{};
    require(waitpid(child, &status, 0) == child, "startup lock observer wait");
    firstAdoptWithWriteLock = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    return 0;
}

template<class Test> void startupTests(Test& test) {
    test("rooted startup takes write lock before first companion adoption", [] {
        Fixture f;
        { Database db(f.root, "ordered.lbdb", config()); Connection conn(&db);
          prepare(conn); query(conn, "CHECKPOINT"); }
        std::ofstream(f.path / "ordered.lbdb.tmp") << "pending spill";
        startupAdopts = 0; firstAdoptAfterOpen = false; firstAdoptWithWriteLock = false;
        {
            RecoveryOpenCounter counter;
            NativeHook hook(observeStartupAdoption);
            Database db(RootDirectory::open(f.path.string()), "ordered.lbdb", config());
            require(startupAdopts > 0, "startup never exercised companion adoption");
            require(firstAdoptAfterOpen, "companion adoption preceded startup data open");
            require(firstAdoptWithWriteLock, "first companion adoption lacked startup write lock");
            Connection conn(&db); require(rows(conn) == 1, "startup lost committed row");
        }
        f.unchanged();
    });
    test("unrooted partition rename reinitializes an existing data handle", [] {
        Fixture f;
        Database db((f.path / "legacy.lbdb").string(), config()); ClientContext context(&db);
        require(!db.getVFS()->isRestricted(), "partition test unexpectedly rooted");
        PartitionStorageRegistry registry;
        auto& sm = registry.getOrCreate(&context, 42, "old_child");
        auto* held = sm.getDataFH();
        const auto oldPath = sm.getDatabasePath();
        const auto offset = held->getFileInfo()->getFileSize();
        const std::string before = "before rename", after = "after rename!";
        held->getFileInfo()->writeFile(reinterpret_cast<const uint8_t*>(before.data()), before.size(), offset);
        held->getFileInfo()->syncFile();
        registry.renameChild(&context, 42, "new_child");
        require(sm.getDataFH() != held, "unrooted rename retained the closed data handle");
        auto* file = sm.getDataFH()->getFileInfo();
        require(file != nullptr, "unrooted rename left no open data file");
        const auto newPath = StorageUtils::getGraphPath(context.getDatabasePath(), "new_child");
        require(sm.getDatabasePath() == newPath && file->path == newPath,
            "unrooted rename opened the wrong data path");
        require(!fs::exists(oldPath) && fs::exists(newPath), "partition rename did not move data file");
        std::string readback(before.size(), '\0');
        file->readFromFile(reinterpret_cast<uint8_t*>(readback.data()), readback.size(), offset);
        require(readback == before, "fresh partition handle lost existing bytes");
        file->writeFile(reinterpret_cast<const uint8_t*>(after.data()), after.size(), offset);
        file->syncFile();
        file->readFromFile(reinterpret_cast<uint8_t*>(readback.data()), readback.size(), offset);
        require(readback == after, "fresh partition handle cannot read its writes");
        require(bytes(newPath).substr(offset, after.size()) == after, "partition write missed renamed file");
        f.unchanged();
    });
    test("unrooted SIGKILL checkpoint record recovers pending shadow", [] {
        // The disk format is shared: use the rooted crash hook, then recover unrooted.
        Fixture f; crashCheckpoint(f, "checkpoint record", false);
        require(fs::exists(f.path / "crash.lbdb.wal.checkpoint") &&
            fs::file_size(f.path / "crash.lbdb.wal.checkpoint") > 0, "crash left no checkpoint record");
        require(fs::exists(f.path / "crash.lbdb.shadow") &&
            fs::file_size(f.path / "crash.lbdb.shadow") > 0, "crash left no pending shadow");
        std::cout.flush();
        const auto child = fork(); require(child >= 0, "unrooted recovery fork");
        if (child == 0) {
            try {
                Database db((f.path / "crash.lbdb").string(), config()); Connection conn(&db);
                require(rows(conn) == 1, "unrooted recovery lost committed row");
                query(conn, "CHECKPOINT");
                _exit(0);
            } catch (const std::exception& e) {
                std::cerr << "unrooted recovery: " << e.what() << '\n'; _exit(1);
            }
        }
        int status{}; require(waitpid(child, &status, 0) == child, "unrooted recovery wait");
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            throw std::runtime_error("unrooted reopen failed: " +
                (WIFSIGNALED(status) ? "child signal=" + std::to_string(WTERMSIG(status)) :
                                      "child exit=" + std::to_string(WEXITSTATUS(status))));
        }
        auto c = config(); c.readOnly = true;
        { Database db((f.path / "crash.lbdb").string(), c); Connection conn(&db);
          require(rows(conn) == 1, "unrooted recovered publication lost committed row"); }
        f.unchanged();
    });
}
