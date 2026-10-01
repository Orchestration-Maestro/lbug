#pragma once
#include "binder/binder.h"
#include "common/exception/binder.h"
#include "extension/extension.h"
#include "function/table/bind_input.h"
#include "function/table/table_function.h"
#include "main/connection.h"
#include "main/prepared_statement.h"
#include "parser/parser.h"
#include "processor/operator/persistent/reader/csv/parallel_csv_reader.h"
#include "processor/operator/persistent/reader/csv/serial_csv_reader.h"
#include "processor/operator/persistent/reader/npy/npy_reader.h"
#include "processor/operator/persistent/reader/parquet/parquet_reader.h"
#include "storage/storage_manager.h"
// Import common names only after SDK-facing engine headers (Windows UUID).
#include "rooted_test_support.h"
#include <map>
#ifndef _WIN32
using namespace lbug::main;
using namespace lbug::binder;
using namespace lbug::function;
using namespace lbug::processor;
using namespace lbug::extension;
SystemConfig config() {
    SystemConfig c(16 * 1024 * 1024, 1);
    c.maxDBSize = 256 * 1024 * 1024;
    c.autoCheckpoint = false;
    c.forceCheckpointOnClose = false;
    return c;
}
void query(Connection& c, const std::string& sql) {
    auto result = c.query(sql);
    if (!result->isSuccess()) { throw std::runtime_error(result->getErrorMessage()); }
}
std::string quoted(const fs::path& p) { return "'" + p.string() + "'"; }
struct Snapshot {
    dev_t dev; ino_t ino; std::string content;
    bool operator==(const Snapshot&) const = default;
};
using Snapshots = std::map<std::string, Snapshot>;
Snapshots snapshots(Fixture& f, Database* db = nullptr) {
    Snapshots result;
    for (const auto& e : fs::recursive_directory_iterator(f.dir)) {
        struct stat s{};
        require(lstat(e.path().c_str(), &s) == 0, "snapshot stat");
        std::string content;
        if (e.is_regular_file()) {
            if (db && e.path() == f.path / "db.lbdb") {
                // Opening/closing another descriptor would release the database's POSIX lock.
                auto held = db->getStorageManager()->getDataFH()->getFileInfo();
                content.resize(held->getFileSize());
                held->readFromFile(content.data(), content.size(), 0);
            } else { content = bytes(e.path()); }
        }
        result.emplace(e.path().string(), Snapshot{s.st_dev, s.st_ino, std::move(content)});
    }
    return result;
}
Snapshots snapshots(Fixture& f, Database& db) { return snapshots(f, &db); }
int filesystemCalls = 0;
bool observeFilesystem = false;
std::string observedExternalPrefix;
int countNative(const char*, int, const char*, struct stat*) { if (observedExternalPrefix.empty()) { ++filesystemCalls; } return 0; }
void countOpen(int, const char*) { if (observedExternalPrefix.empty()) { ++filesystemCalls; } }
namespace lbug::common { extern void (*maestroBeforeRestrictedOpen)(int, const char*); }
struct NoFileCalls {
    NoFileCalls() {
        filesystemCalls = 0;
        observeFilesystem = true;
        maestroRestrictedCall = countNative;
        maestroBeforeRestrictedOpen = countOpen;
    }
    ~NoFileCalls() { observeFilesystem = false; maestroRestrictedCall = nullptr; maestroBeforeRestrictedOpen = nullptr; }
};
template<class F> void typedRefusal(F op, const std::string& feature) {
    bool typed = false;
    { NoFileCalls calls;
      try { op(); }
      catch (const BinderException& e) {
          typed = std::string(e.what()) == "Binder exception: Rooted mode refuses " + feature + ".";
      } catch (const std::exception&) { /* A different error is not the policy refusal. */ }
    }
    require(typed, "missing feature-specific typed early refusal");
    require(filesystemCalls == 0, "refusal performed native filesystem work");
}
void npy(const fs::path& path) {
    std::string header = "{'descr': '<i8', 'fortran_order': False, 'shape': (1,), }";
    header.append((16 - ((10 + header.size() + 1) % 16)) % 16, ' ');
    header += '\n';
    std::ofstream out(path, std::ios::binary);
    out.write("\x93NUMPY\x01\x00", 8);
    const char length[]{char(header.size() & 255), char(header.size() >> 8)};
    out.write(length, 2); out << header;
    const char value[]{7, 0, 0, 0, 0, 0, 0, 0}; out.write(value, 8);
}
struct QueryFixture : Fixture {
    fs::path csv = dir / "outside" / "input.csv";
    fs::path array = dir / "outside" / "input.npy";
    fs::path parquet = dir / "outside" / "input.parquet";
    fs::path imported = dir / "outside" / "import";
    fs::path attached = dir / "outside" / "attached.lbdb";
    QueryFixture() {
        const auto csvSentinel = sentinel.string() + ".csv";
        fs::rename(sentinel, csvSentinel); sentinel = csvSentinel;
        std::ofstream(csv) << "id\n7\n"; npy(array);
        fs::create_directory(imported);
        std::ofstream(imported / "schema.cypher") << "CREATE NODE TABLE Imported(id INT64, PRIMARY KEY(id));";
        { Database db(attached.string(), config()); }
        { Database db(":memory:", config()); Connection c(&db);
          query(c, "COPY (RETURN 7 AS id) TO " + quoted(parquet)); }
    }
};
struct QueryCase {
    const char* feature;
    std::function<std::string(QueryFixture&)> sql;
    const char* readOnlyError = nullptr;
};
const QueryCase queryCases[]{
    {"extension install", [](auto&) { return "INSTALL httpfs"; }},
    {"extension load", [](auto&) { return "LOAD EXTENSION '" MAESTRO_REFUSAL_EXTENSION "'"; }},
    {"extension uninstall", [](auto&) { return "UNINSTALL httpfs"; }},
    {"COPY FROM", [](auto& f) { return "COPY Item FROM " + quoted(f.csv); }, "Connection exception: Cannot execute write operations in a read-only database!"},
    {"COPY TO", [](auto& f) { return "COPY (RETURN 7 AS id) TO " + quoted(f.sentinel); }},
    {"database import", [](auto& f) { return "IMPORT DATABASE " + quoted(f.imported); }},
    {"database export", [](auto& f) { return "EXPORT DATABASE " + quoted(f.dir / "outside" / "exported") + " (FORMAT='CSV')"; }},
    {"ATTACH", [](auto& f) { return "ATTACH " + quoted(f.attached) + " AS other (DBTYPE LBUG)"; }},
    {"DETACH", [](auto&) { return "DETACH other"; }},
    {"file scan", [](auto& f) { return "LOAD FROM " + quoted(f.csv) + " RETURN *"; }},
    {"external Parquet storage", [](auto& f) { return "CREATE NODE TABLE External(id INT64, PRIMARY KEY(id)) WITH (STORAGE=" + quoted(f.parquet) + ", FORMAT='icebug-disk')"; }, "Connection exception: Cannot execute write operations in a read-only database!"},
};
struct FunctionCase {
    const char* feature; FileType type; const char* format;
    function_set (*functions)();
};
const FunctionCase functionCases[]{
    {"NPY mapping", FileType::NPY, "NPY", NpyScanFunction::getFunctionSet},
    {"CSV parallel scan", FileType::CSV, "CSV", ParallelCSVScan::getFunctionSet},
    {"CSV serial scan", FileType::CSV, "CSV", SerialCSVScan::getFunctionSet},
    {"Parquet scan", FileType::PARQUET, "PARQUET", ParquetScanFunction::getFunctionSet},
};
void bindFunction(const FunctionCase& row, Connection& c, QueryFixture& f, bool invalidOption = false) {
    auto functions = row.functions();
    auto func = functions[0]->ptrCast<TableFunction>();
    auto path = row.type == FileType::NPY ? f.array : row.type == FileType::PARQUET ? f.parquet : f.csv;
    TableFuncBindInput input;
    input.addLiteralParam(Value::createValue(path.string()));
    auto extra = std::make_unique<ExtraScanTableFuncBindInput>();
    extra->fileScanInfo = FileScanInfo{FileTypeInfo{row.type, row.format}, {path.string()}};
    if (invalidOption) { extra->fileScanInfo.options.emplace("INVALID_OPTION", Value::createValue(true)); }
    input.extraInput = std::move(extra);
    Binder binder(c.getClientContext()); input.binder = &binder;
    auto data = func->bindFunc(c.getClientContext(), &input);
    require(data->getNumColumns() == 1, "unrooted function lost its column");
}
void rooted(const QueryCase& row) {
    QueryFixture f; Database db(f.root, "db.lbdb", config()); Connection c(&db);
    query(c, "CREATE NODE TABLE Item(id INT64, PRIMARY KEY(id))"); query(c, "CHECKPOINT");
    query(c, "BEGIN TRANSACTION");
    const auto before = snapshots(f, db);
    auto statements = lbug::parser::Parser::parseQuery(row.sql(f));
    Binder binder(c.getClientContext());
    typedRefusal([&] { binder.bind(*statements[0]); }, row.feature);
    require(snapshots(f, db) == before, "bind refusal changed bytes, identity or directory listing");
    f.unchanged(); query(c, "ROLLBACK");
    auto prepared = c.prepare(row.sql(f));
    require(!prepared->isSuccess() && prepared->getErrorMessage() ==
        "Binder exception: Rooted mode refuses " + std::string(row.feature) + ".", "prepare missed early refusal");
}
void unrooted(const QueryCase& row) {
    // IMPORT runs a nested query from an engine worker; reserve another worker for it.
    auto unrootedConfig = config(); unrootedConfig.maxNumThreads = 2;
    QueryFixture f; Database db(":memory:", unrootedConfig); Connection c(&db);
    query(c, "CREATE NODE TABLE Item(id INT64, PRIMARY KEY(id))");
    auto ctx = c.getClientContext(); ctx->getClientConfigUnsafe()->homeDirectory = (f.dir / "home").string();
    const auto feature = std::string(row.feature);
    if (feature == "extension install") {
        // The harness disables network installation. Its upstream bind/plan still succeeds.
        require(c.prepare(row.sql(f))->isSuccess(), "unrooted extension install no longer prepares");
    } else {
        if (feature == "extension uninstall") {
            auto lib = ExtensionUtils::getLocalPathForExtensionLib(ctx, "httpfs");
            fs::create_directories(fs::path(lib).parent_path()); std::ofstream(lib) << "installed";
            fs::create_directory(fs::path(lib).parent_path() / "nested");
            std::ofstream(fs::path(lib).parent_path() / "nested" / "sentinel") << "nested";
        }
        if (feature == "DETACH") { query(c, "ATTACH " + quoted(f.attached) + " AS other (DBTYPE LBUG)"); }
        query(c, row.sql(f));
        if (feature == "external Parquet storage") {
            auto result = c.query("MATCH (n:External) RETURN n.id");
            require(result->isSuccess() && result->hasNext() && result->getNext()->getValue(0)->getValue<int64_t>() == 7, "unrooted external scan failed");
        }
        if (feature == "COPY TO") { require(bytes(f.sentinel) != "outside sentinel", "unrooted CSV did not truncate"); }
        if (feature == "extension uninstall") { require(!fs::exists(ExtensionUtils::getLocalDirForExtension(ctx, "httpfs")), "unrooted uninstall not recursive"); }
        if (feature == "database export") { require(fs::exists(f.dir / "outside" / "exported" / "schema.cypher"), "unrooted export did not create directory"); }
    }
}
#endif
