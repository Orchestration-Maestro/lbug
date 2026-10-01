#include "windows_test_support.h"
#include "common/exception/io.h"
#include "common/file_system/local_file_system.h"
#include "main/connection.h"
#include "main/database.h"
#include "main/query_result.h"
#include <functional>
#include <iostream>
using namespace windows_test;
using namespace lbug::common;

int main(int argc, char** argv) {
    try {
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
        require(count > 0, "mandatory root tests did not run");
        fs::remove_all(parent);
        std::cout << "windows_root groups=" << count << " failures=" << failures << '\n';
        return failures ? 1 : 0;
    } catch (const std::exception& e) { std::cerr << "SETUP " << e.what() << '\n'; return 2; }
}
