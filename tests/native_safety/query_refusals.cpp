#include "query_refusal_support.h"
#ifdef _WIN32
#include "common/exception/io.h"
#else
#include "refusal_syscalls.h" // IWYU pragma: keep
#endif
#ifndef _WIN32
#include "query_refusal_helpers.h"
#include "planner/operator/simple/logical_export_db.h"
#include "processor/plan_mapper.h"
#include "common/file_system/local_file_system.h"
void apiRefusal(const QueryCase& row, bool readOnly) {
    QueryFixture f;
    { Database db(f.root, "db.lbdb", config()); Connection c(&db);
      query(c, "CREATE NODE TABLE Item(id INT64, PRIMARY KEY(id))"); query(c, "CHECKPOINT"); }
    auto settings = config(); settings.readOnly = readOnly;
    Database db(f.root, "db.lbdb", settings); Connection c(&db);
    const auto before = snapshots(f, db);
    const std::string expected = readOnly && row.readOnlyError ? row.readOnlyError :
        "Binder exception: Rooted mode refuses " + std::string(row.feature) + ".";
    const auto sql = row.sql(f);
    std::string failures;
    auto check = [&](const char* api, auto op) {
        std::string actual; bool success = false;
        { NoFileCalls calls;
          auto result = op(); success = result->isSuccess(); actual = result->getErrorMessage(); }
        const auto calls = filesystemCalls;
        const auto unchanged = snapshots(f, db) == before;
        f.unchanged();
        std::cout << "OBSERVE " << (readOnly ? "read-only " : "writable ") << row.feature
                  << " " << api << ": " << actual << " | filesystemCalls=" << calls
                  << " | snapshotsUnchanged=" << unchanged << '\n';
        if (success || actual != expected || calls != 0 || !unchanged) {
            failures += std::string(api) + ": actual=" + actual + "; expected=" + expected +
                "; filesystemCalls=" + std::to_string(calls) +
                "; snapshotsUnchanged=" + std::to_string(unchanged) + "\n";
        }
    };
    check("prepare", [&] { return c.prepare(sql); });
    check("query", [&] { return c.query(sql); });
    require(failures.empty(), failures.c_str());
}
#endif
int main(int argc, char** argv) {
    int count = 0, failures = 0;
    auto test = [&](const std::string& name, auto op) {
        if (argc > 1 && name.find(argv[1]) == std::string::npos) { return; }
        ++count;
        try { op(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failures; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    };
#ifndef _WIN32
    helperTests(test);
#ifdef MAESTRO_SYSCALL_WRAP
    test("ambient existence observer is effective", [] {
        QueryFixture f; LocalFileSystem local("");
        { NoFileCalls calls; require(local.fileOrPathExists(f.csv.string()), "fixture exists"); }
        require(filesystemCalls > 0, "ambient exists probe was invisible");
    });
#endif
    for (const auto& row : queryCases) {
        test("rooted read-only API refusal " + std::string(row.feature), [&] { apiRefusal(row, true); });
        test("rooted writable API no-I/O " + std::string(row.feature), [&] { apiRefusal(row, false); });
    }
    for (const auto& row : functionCases) {
        test("rooted CALL canonical reader " + std::string(row.feature), [&] {
            helperRefusal(row.feature, [&](auto& f, auto& c) {
                auto functions = row.functions();
                auto path = row.type == FileType::NPY ? f.array : row.type == FileType::PARQUET ? f.parquet : f.csv;
                auto statements = lbug::parser::Parser::parseQuery(
                    "CALL " + functions[0]->name + "(" + quoted(path) + ") RETURN *");
                Binder binder(c.getClientContext()); binder.bind(*statements[0]);
            });
        });
    }
    test("rooted direct export mapper", [] {
        helperRefusal("database export", [](auto&, auto& c) {
            lbug::planner::LogicalExportDatabase logical(
                FileScanInfo{FileTypeInfo{FileType::CSV, "CSV"}, {"db.lbdb.shadow"}},
                std::vector<std::shared_ptr<lbug::planner::LogicalOperator>>{}, true);
            ExecutionContext context(nullptr, c.getClientContext(), 0);
            PlanMapper mapper(&context); mapper.mapOperator(&logical);
        });
    });
    for (const auto& row : queryCases) {
        test("rooted " + std::string(row.feature), [&] { rooted(row); });
        test("unrooted " + std::string(row.feature), [&] { unrooted(row); });
    }
    for (const auto& row : functionCases) {
        test("rooted " + std::string(row.feature), [&] {
            QueryFixture f; Database db(f.root, "db.lbdb", config()); Connection c(&db);
            const auto before = snapshots(f, db);
            typedRefusal([&] { bindFunction(row, c, f); }, row.feature);
            require(snapshots(f, db) == before, "function refusal changed filesystem"); f.unchanged();
        });
        test("rooted invalid options " + std::string(row.feature), [&] {
            helperRefusal(row.feature, [&](auto& f, auto& c) { bindFunction(row, c, f, true); });
        });
        test("unrooted " + std::string(row.feature), [&] {
            QueryFixture f; Database db(":memory:", config()); Connection c(&db); bindFunction(row, c, f);
        });
    }
    for (const auto& sql : {"COPY Item FROM 'outside/*.csv'", "EXPLAIN COPY Item FROM 'outside/*.csv'",
             "PROFILE COPY Item FROM 'outside/*.csv'"}) {
        test(std::string("rooted glob ") + sql, [&] {
            QueryFixture f; Database db(f.root, "db.lbdb", config()); Connection c(&db);
            auto before = snapshots(f, db);
            auto statements = lbug::parser::Parser::parseQuery(sql); Binder binder(c.getClientContext());
            typedRefusal([&] { binder.bind(*statements[0]); }, "COPY FROM");
            require(snapshots(f, db) == before, "glob refusal changed filesystem");
        });
    }
#else
    test("Windows rooted capability remains closed", [] {
        bool typed = false;
        try { RootDirectory::open(fs::temp_directory_path().string()); }
        catch (const lbug::common::IOException&) { typed = true; }
        require(typed, "Windows rooted capability accepted");
    });
#endif
    std::cout << "cases: " << count << " failures: " << failures << '\n';
    return failures ? 1 : 0;
}
