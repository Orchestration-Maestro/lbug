#include "query_refusal_support.h"
#ifdef _WIN32
#include "common/exception/io.h"
#else
#include "refusal_syscalls.h" // IWYU pragma: keep
#endif
#ifndef _WIN32
#include "query_refusal_helpers.h"
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
