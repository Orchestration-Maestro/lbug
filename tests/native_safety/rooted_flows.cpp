#include "rooted_test_support.h"
#include "main/client_context.h"
#include "common/exception/buffer_manager.h"
#include "common/exception/io.h"
#include "main/connection.h"
#include "main/database.h"
#include "main/query_result.h"
#include "processor/operator/persistent/node_batch_insert.h"
#include "storage/buffer_manager/buffer_manager.h"
#include "storage/table/chunked_node_group.h"
#include "storage/storage_manager.h"
#ifndef _WIN32
#include <map>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include "rooted_startup_tests.h"
namespace lbug::common {
extern void (*maestroBeforeRestrictedOpen)(int directory, const char* name);
}
using namespace lbug::main;
using namespace lbug::storage;
SystemConfig config() {
    SystemConfig c(16 * 1024 * 1024, 1);
    c.maxDBSize = 256 * 1024 * 1024;
    c.autoCheckpoint = false;
    c.forceCheckpointOnClose = false;
    return c;
}
void query(Connection& connection, const std::string& sql) {
    auto result = connection.query(sql);
    if (!result->isSuccess()) { throw std::runtime_error(result->getErrorMessage()); }
}
int64_t rows(Connection& connection) {
    auto result = connection.query("MATCH (i:Item) RETURN count(*)");
    require(result->isSuccess(), "count query failed");
    return result->getNext()->getValue(0)->getValue<int64_t>();
}
void prepare(Connection& connection) {
    query(connection, "CREATE NODE TABLE Item(id INT64, PRIMARY KEY(id))");
    query(connection, "CREATE (:Item {id: 7})");
}
std::vector<std::string> entries(const Fixture& f) {
    std::vector<std::string> result;
    for (const auto& entry : fs::directory_iterator(f.path)) { result.push_back(entry.path().filename().string()); }
    std::sort(result.begin(), result.end());
    return result;
}
void noSidecars(Fixture& f, const std::string& db) {
    for (const auto& name : entries(f)) {
        require(!name.starts_with(db + "."), "sidecar leaked on checkpoint/close");
    }
}
int crashReady = -1;
void awaitKill(char cut) {
    require(write(crashReady, &cut, 1) == 1, "crash ready pipe");
    for (;;) { pause(); }
}
void stopAfterRename(int, const char* name) {
    if (std::string(name).ends_with(".wal.checkpoint")) {
        awaitKill('H');
    }
}
int stopAfterCheckpointRecord(const char* operation, int, const char* name, struct stat*) {
    if (std::string(operation) == "file-sync" && std::string(name).ends_with(".wal.checkpoint")) {
        awaitKill('H');
    }
    return 0;
}
bool shadowUnlinked = false;
bool killBeforeShadowUnlink = false;
int stopAtShadowUnlink(const char* operation, int directory, const char* name, struct stat*) {
    if (std::string(operation) == "unlink" && name && std::string(name) == "crash.lbdb.shadow") {
        if (killBeforeShadowUnlink) { awaitKill('H'); }
        shadowUnlinked = true;
    } else if (std::string(operation) == "directory-sync" && shadowUnlinked) {
        require(fsync(directory) == 0, "sync shadow unlink before kill");
        awaitKill('H');
    }
    return 0;
}
bool walReadInjected = false;
int failWALReadOnce(const char* operation, int, const char* name, struct stat*) {
    if (!walReadInjected && std::string(operation) == "read" && name &&
        std::string(name) == "strict-wal.lbdb.wal") {
        walReadInjected = true;
        throw IOException("transient rooted WAL read failure");
    }
    return 0;
}
int failShadowUnlink(const char* operation, int, const char* name, struct stat*) {
    return std::string(operation) == "unlink" && name &&
        std::string(name) == "crash.lbdb.shadow" ? EIO : 0;
}
int recoveryDataOpens = 0;
void countRecoveryDataOpens(int, const char* name) {
    if (std::string(name).ends_with(".lbdb")) { ++recoveryDataOpens; }
}
void recoverCommittedRow(Fixture& f, const std::string& name) {
    auto root = RootDirectory::open(f.path.string());
    { RecoveryOpenCounter counter;
      Database db(root, name, config()); Connection conn(&db);
      require(rows(conn) == 1, "crash lost committed row"); query(conn, "CHECKPOINT");
      require(recoveryDataOpens == 1, "rooted recovery reopened the locked data file"); }
    noSidecars(f, name);
    auto c = config(); c.readOnly = true;
    { Database db(RootDirectory::open(f.path.string()), name, c); Connection conn(&db);
      require(rows(conn) == 1, "recovery publication row"); }
    f.unchanged();
}
void crashCheckpoint(Fixture& f, const std::string& phase, bool recover = true) {
    int ready[2]; require(pipe(ready) == 0, "crash pipe");
    std::cout.flush();
    auto child = fork(); require(child >= 0, "fork");
    if (child == 0) {
        close(ready[0]); crashReady = ready[1];
        try {
            Database db(RootDirectory::open(f.path.string()), "crash.lbdb", config());
            Connection conn(&db); prepare(conn);
            if (phase == "rename") { maestroBeforeRestrictedOpen = stopAfterRename; }
            if (phase == "checkpoint record") { maestroRestrictedCall = stopAfterCheckpointRecord; }
            if (phase == "before shadow unlink" || phase == "after shadow unlink") {
                killBeforeShadowUnlink = phase == "before shadow unlink";
                shadowUnlinked = false; maestroRestrictedCall = stopAtShadowUnlink;
            }
            if (phase != "WAL commit") { query(conn, "CHECKPOINT"); }
            awaitKill('D');
        } catch (const std::exception& e) { std::cerr << e.what() << '\n'; _exit(1); }
    }
    close(ready[1]);
    pollfd event{ready[0], POLLIN, 0};
    const auto reached = poll(&event, 1, 30000);
    char cut{};
    const auto received = reached > 0 ? read(ready[0], &cut, 1) : 0;
    close(ready[0]);
    int status{};
    const auto killed = kill(child, SIGKILL);
    require(waitpid(child, &status, 0) == child, "wait for killed child");
    require(received == 1, "child did not reach crash point");
    require(cut == ((phase == "WAL commit" || phase == "checkpoint") ? 'D' : 'H'),
        "checkpoint completed without reaching the requested cut");
    require(killed == 0 && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL, "SIGKILL");
    for (auto suffix : {".wal.checkpoint", ".shadow"}) {
        const auto path = f.path / (std::string("crash.lbdb") + suffix);
        std::cout << "crash cut " << phase << ' ' << suffix << " bytes=";
        if (fs::exists(path)) { std::cout << fs::file_size(path); } else { std::cout << "missing"; }
        std::cout << '\n';
    }
    f.unchanged();
    if (recover) { recoverCommittedRow(f, "crash.lbdb"); }
}
struct FileSnapshot { std::string content; struct stat identity{}; };
std::map<std::string, FileSnapshot> snapshots(const Fixture& f, Database& db) {
    std::map<std::string, FileSnapshot> result;
    for (const auto& name : entries(f)) {
        auto& snapshot = result[name];
        if (name == db.getStorageManager()->getDatabasePath()) {
            // Closing another fd for this inode releases A's POSIX record lock.
            auto held = db.getStorageManager()->getDataFH()->getFileInfo();
            snapshot.content.resize(held->getFileSize());
            held->readFromFile(reinterpret_cast<uint8_t*>(snapshot.content.data()),
                snapshot.content.size(), 0);
        } else { snapshot.content = bytes(f.path / name); }
        require(lstat((f.path / name).c_str(), &snapshot.identity) == 0, "snapshot stat");
    }
    return result;
}
std::unique_ptr<lbug::processor::NoIndexPKValidator> validator(ClientContext& context) {
    context.getClientConfigUnsafe()->pkValidatorSpillThreshold = 1;
    return lbug::processor::batch_insert::createNoIndexPKValidator(LogicalType::INT64(), &context);
}
void validateRuns(lbug::processor::NoIndexPKValidator& v, Database& db, bool duplicate = false) {
    ColumnChunkData chunk(*db.getMemoryManager(), LogicalType::INT64(), 8, false,
        ResidencyState::IN_MEMORY, false);
    chunk.setValue<int64_t>(3, 0); chunk.setValue<int64_t>(1, 1);
    v.validate(chunk, 0, 2);
    chunk.setValue<int64_t>(duplicate ? 1 : 2, 0);
    v.validate(chunk, 0, 1);
    if (duplicate) { refused([&] { v.finalize(); }); } else { v.finalize(); }
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
    startupTests(test);
    for (bool readOnly : {true, false}) {
        test(readOnly ? "strict rooted read-only WAL replay" : "strict rooted writable WAL replay", [=] {
            Fixture f;
            { Database db(f.root, "strict-wal.lbdb", config()); Connection conn(&db); prepare(conn); }
            const auto wal = f.path / "strict-wal.lbdb.wal";
            require(fs::exists(wal) && fs::file_size(wal) > 0, "SETUP active WAL missing");
            const auto originalWAL = bytes(wal);
            auto c = config(); c.readOnly = readOnly; c.throwOnWalReplayFailure = false;
            auto root = RootDirectory::open(f.path.string());
            bool rejected = false;
            walReadInjected = false;
            {
                NativeHook hook(failWALReadOnce);
                try { Database db(root, "strict-wal.lbdb", c); }
                catch (const IOException& e) {
                    rejected = std::string(e.what()).find("transient rooted WAL read failure") != std::string::npos;
                }
            }
            require(walReadInjected, "SETUP transient WAL read was not injected");
            require(rejected, "transient WAL read failure silently accepted");
            require(bytes(wal) == originalWAL, "failed replay changed WAL bytes");
            { Database db(RootDirectory::open(f.path.string()), "strict-wal.lbdb", c); Connection conn(&db);
              require(rows(conn) == 1, "clean WAL replay lost committed row"); }
            f.unchanged();
        });
    }
    test("checkpoint rollback repeated writes and cleanup", [] {
        Fixture f;
        {
            Database db(f.root, "flow.lbdb", config()); Connection c(&db); prepare(c);
            query(c, "BEGIN TRANSACTION"); query(c, "CREATE (:Item {id: 8})"); query(c, "ROLLBACK");
            require(rows(c) == 1, "rollback persisted a row");
            query(c, "CHECKPOINT"); noSidecars(f, "flow.lbdb");
            query(c, "CREATE (:Item {id: 9})"); query(c, "CHECKPOINT"); noSidecars(f, "flow.lbdb");
        }
        auto before = entries(f);
        auto readonly = config(); readonly.readOnly = true;
        { Database db(f.root, "flow.lbdb", readonly); Connection c(&db); require(rows(c) == 2, "checkpoint rows"); }
        require(entries(f) == before, "read-only created a sidecar"); f.unchanged();
    });
    test("default close checkpoints and cleans sidecars", [] {
        Fixture f; auto c = config(); c.forceCheckpointOnClose = true;
        { Database db(f.root, "close.lbdb", c); Connection conn(&db); prepare(conn); }
        noSidecars(f, "close.lbdb");
        c.readOnly = true;
        { Database db(f.root, "close.lbdb", c); Connection conn(&db); require(rows(conn) == 1, "close lost row"); }
        noSidecars(f, "close.lbdb"); f.unchanged();
    });
    test("auto checkpoint rooted flow", [] {
        Fixture f; auto c = config(); c.autoCheckpoint = true; c.checkpointThreshold = 0;
        { Database db(f.root, "auto.lbdb", c); Connection conn(&db); prepare(conn); noSidecars(f, "auto.lbdb"); }
        c.readOnly = true;
        { Database db(f.root, "auto.lbdb", c); Connection conn(&db); require(rows(conn) == 1, "auto checkpoint row"); }
        f.unchanged();
    });
    for (auto phase : {"WAL commit", "rename", "checkpoint record", "checkpoint"}) {
        test((std::string("SIGKILL reopen after ") + phase).c_str(), [=] {
            Fixture f;
            crashCheckpoint(f, phase);
        });
    }
    test("SIGKILL reopen before shadow unlink", [] {
        Fixture f; crashCheckpoint(f, "before shadow unlink");
    });
    test("SIGKILL reopen after shadow unlink", [] {
        Fixture f; crashCheckpoint(f, "after shadow unlink");
    });
    test("checkpoint shadow unlink EIO remains reopenable", [] {
        Fixture f;
        {
            Database db(f.root, "crash.lbdb", config()); Connection conn(&db); prepare(conn);
            { NativeHook hook(failShadowUnlink);
              auto result = conn.query("CHECKPOINT");
              require(!result->isSuccess(), "checkpoint ignored shadow unlink EIO"); }
        }
        f.unchanged(); recoverCommittedRow(f, "crash.lbdb");
    });
    test("second rooted writer preserves live spill on lock refusal", [] {
        Fixture f; auto c = config(); c.bufferPoolSize = 2 * 1024 * 1024;
        {
            Database db(f.root, "live.lbdb", c); Connection conn(&db); prepare(conn);
            auto& mm = *db.getMemoryManager();
            std::vector<std::unique_ptr<InMemChunkedNodeGroup>> groups;
            for (int i = 0; i < 16; ++i) {
                std::vector<std::unique_ptr<ColumnChunkData>> chunks;
                auto chunk = std::make_unique<ColumnChunkData>(mm, LogicalType::INT64(), 16384,
                    false, ResidencyState::IN_MEMORY, false);
                chunk->setValue<int64_t>(100 + i, 0); chunk->setValue<int64_t>(0, 16383);
                chunks.push_back(std::move(chunk));
                auto group = std::make_unique<InMemChunkedNodeGroup>(std::move(chunks), 0);
                group->setUnused(mm); groups.push_back(std::move(group));
            }
            require(fs::exists(f.path / "live.lbdb.tmp") && fs::file_size(f.path / "live.lbdb.tmp") > 0,
                "writer A never spilled");
            auto before = snapshots(f, db);
            std::cout.flush();
            const auto child = fork(); require(child >= 0, "second writer fork");
            if (child == 0) {
                startupAdopts = 0;
                NativeHook hook(countStartupAdopts);
                try {
                    Database second(RootDirectory::open(f.path.string()), "live.lbdb", c);
                    std::cerr << "second writer: unexpectedly opened database\n"; _exit(2);
                }
                catch (const std::exception& e) {
                    std::cerr << "second writer: " << e.what() << '\n';
                    if (startupAdopts != 0) {
                        std::cerr << "lock-refused writer adopted companions: " << startupAdopts << '\n';
                        _exit(4);
                    }
                    _exit(std::string(e.what()).find("Could not set lock") != std::string::npos ? 0 : 3);
                }
            }
            int status{}; require(waitpid(child, &status, 0) == child, "second writer wait");
            f.unchanged();
            std::cout << "second writer wait status=" << status << '\n';
            auto after = snapshots(f, db);
            require(before.size() == after.size(), "lock refusal changed live file names");
            for (const auto& [name, snapshot] : before) {
                auto current = after.find(name);
                require(current != after.end(), "lock refusal removed live file");
                require(current->second.content == snapshot.content &&
                    current->second.identity.st_dev == snapshot.identity.st_dev &&
                    current->second.identity.st_ino == snapshot.identity.st_ino,
                    "lock refusal changed live bytes/dev/ino");
            }
            require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "second writer did not refuse for lock contention");
            for (int i = 0; i < 16; ++i) {
                groups[i]->loadFromDisk(mm);
                require(groups[i]->getColumnChunk(0).getValue<int64_t>(0) == 100 + i, "A spill reload bytes");
                groups[i].reset();
            }
            auto bm = mm.getBufferManager(); bm->resetSpiller("");
            require(!fs::exists(f.path / "live.lbdb.tmp"), "A spill reset left temp");
            bm->resetSpiller(StorageUtils::getTmpFilePath("live.lbdb"));
            require(rows(conn) == 1, "second writer lost committed row"); query(conn, "CHECKPOINT");
        }
        f.unchanged(); recoverCommittedRow(f, "live.lbdb");
    });
    test("rooted startup recovery keeps the locked data handle", [] {
        Fixture f;
        {
            RecoveryOpenCounter counter;
            Database db(f.root, "retained.lbdb", config()); Connection conn(&db); prepare(conn);
            auto sm = db.getStorageManager();
            auto held = sm->getDataFH();
            ClientContext context(&db);
            sm->initDataFileHandle(db.getVFS(), &context);
            require(sm->getDataFH() == held, "rooted recovery reinitialized its locked data handle");
            require(sm->getRecoveryDataFile() == held->getFileInfo(), "recovery lost its held data file");
            require(recoveryDataOpens == 1, "rooted startup reopened its data file");
            query(conn, "CHECKPOINT");
        }
        f.unchanged(); recoverCommittedRow(f, "retained.lbdb");
    });
    test("spill forced memory pressure reset and close cleanup", [] {
        Fixture f;
        {
            BufferManager bm("db.lbdb", StorageUtils::getTmpFilePath("db.lbdb"), 2 * 1024 * 1024,
                256 * 1024 * 1024, f.vfs.get(), false);
            MemoryManager mm(&bm, f.vfs.get());
            std::vector<std::unique_ptr<InMemChunkedNodeGroup>> groups;
            for (int i = 0; i < 16; ++i) {
                std::vector<std::unique_ptr<ColumnChunkData>> chunks;
                // Non-page-sized chunks use the spillable malloc path, not pinned MM pages.
                auto chunk = std::make_unique<ColumnChunkData>(mm, LogicalType::INT64(), 16384,
                    false, ResidencyState::IN_MEMORY, false);
                chunk->setValue<int64_t>(100 + i, 0);
                chunk->setValue<int64_t>(0, 16383);
                chunks.push_back(std::move(chunk));
                auto group = std::make_unique<InMemChunkedNodeGroup>(std::move(chunks), 0);
                group->setUnused(mm); groups.push_back(std::move(group));
            }
            require(fs::exists(f.path / "db.lbdb.tmp") && fs::file_size(f.path / "db.lbdb.tmp") > 0,
                "memory pressure never spilled");
            for (int i = 0; i < 16; ++i) {
                groups[i]->loadFromDisk(mm);
                require(groups[i]->getColumnChunk(0).getValue<int64_t>(0) == 100 + i, "spill reload bytes");
                groups[i].reset();
            }
            groups.clear();
            bm.resetSpiller(""); require(!fs::exists(f.path / "db.lbdb.tmp"), "disable did not unlink temp");
            bm.resetSpiller(StorageUtils::getTmpFilePath("db.lbdb"));
            refused([&] { bm.resetSpiller((f.dir / "outside" / "other.tmp").string()); });
        }
        require(!fs::exists(f.path / "db.lbdb.tmp"), "close left spill"); f.unchanged();
    });
    test("bulk no-index validator generated registration collision and cleanup", [] {
        Fixture f;
        std::ofstream(f.path / StorageUtils::getPKValidatorSpillFilePath("bulk.lbdb", 0)) << "crashed session";
        Database db(f.root, "bulk.lbdb", config()); ClientContext context(&db);
        {
            auto v = validator(context); validateRuns(*v, db);
            require(bytes(f.path / "bulk.lbdb.pk_validator.0.tmp") == "crashed session", "old temp adopted or truncated");
            bool spilled = false;
            for (const auto& name : entries(f)) {
                if (name.starts_with("bulk.lbdb.pk_validator.") && name != "bulk.lbdb.pk_validator.0.tmp") {
                    require(fs::file_size(f.path / name) > 0, "bulk validator never spilled"); spilled = true;
                }
            }
            require(spilled, "bulk temp not created");
        }
        for (const auto& name : entries(f)) {
            require(!name.starts_with("bulk.lbdb.pk_validator.") || name == "bulk.lbdb.pk_validator.0.tmp", "bulk temp leaked");
        }
        db.getVFS()->removeFileIfExists("bulk.lbdb.tmp");
        refused([&] { db.getVFS()->removeFileIfExists("bulk.lbdb.pk_validator.0.tmp"); }); f.unchanged();
    });
    test("bulk duplicate runs rollback cleanup", [] {
        Fixture f; Database db(f.root, "bulk.lbdb", config()); ClientContext context(&db);
        { auto v = validator(context); validateRuns(*v, db, true); }
        for (const auto& name : entries(f)) { require(!name.starts_with("bulk.lbdb.pk_validator."), "duplicate failure leaked temp"); }
        f.unchanged();
    });
    test("bulk allocator collision bound refuses without adoption", [] {
        Fixture f; Database db(f.root, "bulk.lbdb", config()); ClientContext context(&db);
        for (int i = 0; i < 16; ++i) {
            std::ofstream(f.path / StorageUtils::getPKValidatorSpillFilePath("bulk.lbdb", i)) << "unowned";
        }
        refused([&] { validator(context); });
        for (int i = 0; i < 16; ++i) {
            require(bytes(f.path / StorageUtils::getPKValidatorSpillFilePath("bulk.lbdb", i)) == "unowned", "collision changed bytes");
        }
        f.unchanged();
    });
    test("read-only WAL replay never adopts writable companions", [] {
        Fixture f;
        { Database db(f.root, "readonly-wal.lbdb", config()); Connection conn(&db); prepare(conn); }
        const auto before = entries(f);
        auto fresh = RootDirectory::open(f.path.string());
        auto c = config(); c.readOnly = true;
        { Database db(fresh, "readonly-wal.lbdb", c); Connection conn(&db);
          require(rows(conn) == 1, "read-only WAL rows");
          refused([&] { db.getVFS()->removeFileIfExists("readonly-wal.lbdb.wal"); }); }
        require(before == entries(f), "read-only adopted or removed companion"); f.unchanged();
    });
    test("unrestricted read-only spill reset retains upstream behavior", [] {
        Fixture f; const auto base = (f.path / "legacy.lbdb").string();
        VirtualFileSystem legacy(base);
        BufferManager bm(base, StorageUtils::getTmpFilePath(base), 2 * 1024 * 1024,
            256 * 1024 * 1024, &legacy, true);
        bm.resetSpiller(StorageUtils::getTmpFilePath(base)); bm.resetSpiller(""); f.unchanged();
    });
    test("read-only rooted spill reset refuses before allocation under pressure", [] {
        Fixture f; auto c = config(); c.bufferPoolSize = 2 * 1024 * 1024;
        { Database db(f.root, "readonly.lbdb", c); }
        const auto before = entries(f); c.readOnly = true;
        {
            Database db(f.root, "readonly.lbdb", c); Connection conn(&db);
            auto call = conn.query("CALL spill_to_disk=true");
            require(!call->isSuccess(), "read-only CALL enabled spill");
            bool typed = false;
            try { db.getMemoryManager()->getBufferManager()->resetSpiller(StorageUtils::getTmpFilePath("readonly.lbdb")); }
            catch (const BufferManagerException&) { typed = true; }
            require(typed, "read-only rooted native reset accepted");
            query(conn, "CALL spill_to_disk=false");
            db.getMemoryManager()->getBufferManager()->resetSpiller("");
            std::vector<std::unique_ptr<InMemChunkedNodeGroup>> groups;
            bool pressure = false;
            try {
                for (int i = 0; i < 32; ++i) {
                    std::vector<std::unique_ptr<ColumnChunkData>> chunks;
                    auto chunk = std::make_unique<ColumnChunkData>(*db.getMemoryManager(), LogicalType::INT64(),
                        16384, false, ResidencyState::IN_MEMORY, false);
                    chunk->setValue<int64_t>(i, 0); chunk->setValue<int64_t>(0, 16383);
                    chunks.push_back(std::move(chunk));
                    auto group = std::make_unique<InMemChunkedNodeGroup>(std::move(chunks), 0);
                    group->setUnused(*db.getMemoryManager()); groups.push_back(std::move(group));
                }
            } catch (const BufferManagerException&) { pressure = true; }
            require(pressure, "read-only pressure did not exhaust no-spill pool");
            require(entries(f) == before, "read-only reset or pressure created a sidecar");
        }
        require(entries(f) == before, "read-only close changed entries"); f.unchanged();
    });
    test("read-only published open creates no sidecar or spill", [] {
        Fixture f; auto c = config();
        { Database db(f.root, "readonly.lbdb", c); }
        std::ofstream(f.path / "readonly.lbdb.tmp") << "inert temp";
        auto before = entries(f); c.readOnly = true;
        { Database db(f.root, "readonly.lbdb", c); }
        require(before == entries(f), "read-only sidecar created"); f.unchanged();
    });
#endif
    std::cout << "cases: " << count << " failures: " << failures << '\n';
    return failures ? 1 : 0;
}
