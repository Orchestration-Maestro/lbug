#pragma once
#include "query_refusal_support.h"
#ifndef _WIN32
#include "catalog/catalog.h"
#include "extension/extension_installer.h"
#include "extension/extension_manager.h"
#include "function/export/export_function.h"
#include "main/attached_database.h"
#include "processor/execution_context.h"
#include "processor/operator/simple/attach_database.h"
#include "processor/operator/simple/detach_database.h"
#include "processor/operator/simple/export_db.h"
#include "processor/operator/simple/import_db.h"
#include "processor/operator/simple/uninstall_extension.h"
#include "processor/result/factorized_table_util.h"
#include "processor/operator/persistent/reader/csv/csv_boundary_scanner.h"
#include "storage/table/ice_disk_node_table.h"
#include "storage/table/ice_disk_rel_table.h"
template<class F> void helperRefusal(const char* feature, F op) {
    QueryFixture f; Database db(f.root, "db.lbdb", config()); Connection c(&db);
    query(c, "BEGIN TRANSACTION");
    const auto before = snapshots(f, db);
    typedRefusal([&] { op(f, c); }, feature);
    require(snapshots(f, db) == before, "direct helper changed filesystem"); f.unchanged();
    query(c, "ROLLBACK");
}
std::string externalNode(QueryFixture& f) {
    return "CREATE NODE TABLE External(id INT64, PRIMARY KEY(id)) WITH (STORAGE=" + quoted(f.parquet) + ", FORMAT='icebug-disk')";
}
template<class T> void helperTests(T test) {
    test("rooted persisted external storage startup", [] {
        QueryFixture f;
        { Database db((f.path / "db.lbdb").string(), config()); Connection c(&db);
          query(c, externalNode(f)); query(c, "CHECKPOINT"); }
        const auto before = snapshots(f);
        // Startup may read the rooted database, but must not probe/open external storage.
        observedExternalPrefix = (f.dir / "outside").string();
        try {
            typedRefusal([&] { Database db(RootDirectory::open(f.path.string()), "db.lbdb", config()); }, "external Parquet storage");
        } catch (...) { observedExternalPrefix.clear(); throw; }
        observedExternalPrefix.clear();
        require(snapshots(f) == before, "refused external startup changed filesystem"); f.unchanged();
    });
    test("rooted external rel DDL", [] {
        helperRefusal("external Parquet storage", [](auto& f, auto& c) {
            auto statements = lbug::parser::Parser::parseQuery("CREATE REL TABLE ExternalRel(FROM Missing TO Missing) WITH (STORAGE=" + quoted(f.parquet) + ", FORMAT='icebug-disk')");
            Binder binder(c.getClientContext()); binder.bind(*statements[0]);
        });
    });
    for (bool rel : {false, true}) {
        test(std::string("rooted direct storage ") + (rel ? "rel" : "node"), [=] {
            QueryFixture f; Database db(f.root, "db.lbdb", config()); Connection c(&db);
            Database source(":memory:", config()); Connection other(&source);
                query(other, externalNode(f));
                if (rel) { query(other, "CREATE REL TABLE ExternalRel(FROM External TO External) WITH (STORAGE=" + quoted(f.parquet) + ", FORMAT='icebug-disk')"); }
                query(other, "BEGIN TRANSACTION");
                auto catalog = lbug::catalog::Catalog::Get(*other.getClientContext());
                auto transaction = lbug::transaction::Transaction::Get(*other.getClientContext());
                auto node = catalog->getTableCatalogEntry(transaction, "External")->template ptrCast<lbug::catalog::NodeTableCatalogEntry>();
                auto entry = rel ? catalog->getTableCatalogEntry(transaction, "ExternalRel")->template ptrCast<lbug::catalog::RelGroupCatalogEntry>() : nullptr;
                auto ctx = c.getClientContext(); const auto before = snapshots(f, db);
                typedRefusal([&] {
                    if (rel) {
                        lbug::storage::IceDiskRelTable table(entry, node->getTableID(), node->getTableID(), db.getStorageManager(), lbug::storage::MemoryManager::Get(*ctx), ctx);
                    } else {
                        lbug::storage::IceDiskNodeTable table(db.getStorageManager(), node, lbug::storage::MemoryManager::Get(*ctx), ctx);
                    }
                }, "external Parquet storage");
                require(snapshots(f, db) == before, "direct storage constructor changed filesystem");
                query(other, "ROLLBACK"); f.unchanged();
        });
    }
    for (int reader = 0; reader < 3; ++reader) {
        test("rooted raw reader " + std::to_string(reader), [=] {
            helperRefusal(reader == 0 ? "Parquet scan" : "file scan", [&](auto& f, auto& c) {
                auto ctx = c.getClientContext();
                if (reader == 0) { ParquetReader raw(f.parquet.string(), {}, ctx); }
                else if (reader == 1) { SerialCSVReader raw(f.csv.string(), 0, CSVOption{}, CSVColumnInfo(1, {}, 0), ctx, nullptr); }
                else { CSVBoundaryScanner::planFixedChunkOverlap(f.csv.string(), 0, CSVOption{}, ctx); }
            });
        });
    }
    test("rooted manager extension load", [] {
        helperRefusal("extension load", [](auto&, auto& c) {
            ExtensionManager::Get(*c.getClientContext())->loadExtension(MAESTRO_REFUSAL_EXTENSION, c.getClientContext());
        });
    });
    test("unrooted manager extension load", [] {
        Database db(":memory:", config()); Connection c(&db); query(c, "BEGIN TRANSACTION");
        auto manager = ExtensionManager::Get(*c.getClientContext());
        manager->loadExtension(MAESTRO_REFUSAL_EXTENSION, c.getClientContext());
        require(manager->getLoadedExtensions().size() == 1, "unrooted manager failed dynamic load");
        query(c, "ROLLBACK");
    });
    test("rooted direct installer", [] {
        helperRefusal("extension install", [](auto&, auto& c) {
            InstallExtensionInfo info{"httpfs", "https://example.invalid", false};
            ExtensionInstaller installer(info, *c.getClientContext()); installer.install();
        });
    });
    test("rooted direct attached database", [] {
        helperRefusal("ATTACH", [](auto& f, auto& c) {
            AttachedLbugDatabase attached(f.attached.string(), "other", ATTACHED_LBUG_DB_TYPE, c.getClientContext());
        });
    });
    for (bool parquet : {false, true}) {
        test(std::string("rooted direct export ") + (parquet ? "Parquet" : "CSV truncate"), [=] {
            QueryFixture f; Database db(f.root, "db.lbdb", config()); Connection c(&db);
            f.owned("output.csv", "export sentinel");
            auto functions = parquet ? ExportParquetFunction::getFunctionSet() : ExportCSVFunction::getFunctionSet();
            auto func = functions[0]->template ptrCast<ExportFunction>();
            ExportFuncBindInput input{{"id"}, "output.csv", {}};
            auto data = func->bind(input);
            std::vector<LogicalType> types; types.push_back(LogicalType::INT64());
            data->setDataType(std::move(types));
            auto state = func->createSharedState();
            const auto before = snapshots(f, db);
            typedRefusal([&] { state->init(*c.getClientContext(), *data); }, "COPY TO");
            require(snapshots(f, db) == before && bytes(f.path / "output.csv") == "export sentinel", "direct export truncated sentinel");
            f.unchanged();
        });
    }
    for (const auto& row : functionCases) {
        test("rooted reader plan " + std::string(row.feature), [&] {
            helperRefusal(row.feature, [&](auto& f, auto& c) {
                auto functions = row.functions(); auto func = functions[0]->template ptrCast<TableFunction>();
                auto path = row.type == FileType::NPY ? f.array : row.type == FileType::PARQUET ? f.parquet : f.csv;
                Binder binder(c.getClientContext());
                std::vector<LogicalType> types; types.push_back(LogicalType::INT64());
                auto columns = binder.createVariables({"id"}, types);
                ScanFileBindData data(columns, 1, FileScanInfo{FileTypeInfo{row.type, row.format}, {path.string()}}, c.getClientContext());
                ExecutionContext context(nullptr, c.getClientContext(), 0);
                TableFuncInitSharedStateInput input(&data, &context);
                func->initSharedStateFunc(input);
            });
        });
    }
    const char* features[]{"extension uninstall", "ATTACH", "DETACH", "database import", "database export"};
    for (auto feature : features) {
        test("rooted operator " + std::string(feature), [=] {
            helperRefusal(feature, [&](auto& f, auto& c) {
                auto ctx = c.getClientContext();
                auto message = FactorizedTableUtils::getSingleStringColumnFTable(lbug::storage::MemoryManager::Get(*ctx));
                ExecutionContext context(nullptr, ctx, 0);
                if (std::string(feature) == "extension uninstall") {
                    UninstallExtension op("httpfs", message, 0, nullptr); op.executeInternal(&context);
                } else if (std::string(feature) == "ATTACH") {
                    AttachDatabase op(AttachInfo{f.attached.string(), "other", ATTACHED_LBUG_DB_TYPE, {}}, message, 0, nullptr); op.executeInternal(&context);
                } else if (std::string(feature) == "DETACH") {
                    DetachDatabase op("other", message, 0, nullptr); op.executeInternal(&context);
                } else if (std::string(feature) == "database import") {
                    ImportDB op("RETURN 1", "", message, 0, nullptr); op.executeInternal(&context);
                } else {
                    lbug::processor::ExportDB op(FileScanInfo{FileTypeInfo{FileType::CSV, "CSV"}, {f.imported.string()}}, true, message, 0, nullptr); op.executeInternal(&context);
                }
            });
        });
    }
}
#endif
