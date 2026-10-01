#include "binder/rooted_query_refusal.h"

#include "common/exception/binder.h"
#include "common/file_system/virtual_file_system.h"
#include "processor/operator/persistent/reader/csv/parallel_csv_reader.h"
#include "processor/operator/persistent/reader/csv/serial_csv_reader.h"
#include "processor/operator/persistent/reader/npy/npy_reader.h"
#include "processor/operator/persistent/reader/parquet/parquet_reader.h"

namespace lbug::binder {
namespace {
struct RootedRefusal {
    RootedQueryKind kind;
    std::string_view feature;
};
using common::StatementType;
using extension::ExtensionAction;
constexpr RootedRefusal rootedRefusals[]{
    {ExtensionAction::INSTALL, "extension install"},
    {ExtensionAction::LOAD, "extension load"},
    {ExtensionAction::UNINSTALL, "extension uninstall"},
    {StatementType::COPY_FROM, "COPY FROM"},
    {StatementType::COPY_TO, "COPY TO"},
    {StatementType::IMPORT_DATABASE, "database import"},
    {StatementType::EXPORT_DATABASE, "database export"},
    {StatementType::ATTACH_DATABASE, "ATTACH"},
    {StatementType::DETACH_DATABASE, "DETACH"},
    {common::ScanSourceType::FILE, "file scan"},
    {std::string_view{processor::NpyScanFunction::name}, "NPY mapping"},
    {std::string_view{processor::ParallelCSVScan::name}, "CSV parallel scan"},
    {std::string_view{processor::SerialCSVScan::name}, "CSV serial scan"},
    {std::string_view{processor::ParquetScanFunction::name}, "Parquet scan"},
    {common::StorageFormat::ICEBUG_DISK, "external Parquet storage"},
};
} // namespace

void refuseRootedQuery(const main::ClientContext& context, const RootedQueryKind& kind) {
    if (!common::VirtualFileSystem::GetUnsafe(context)->isRestricted()) { return; }
    for (const auto& refusal : rootedRefusals) {
        if (refusal.kind == kind) {
            throw common::BinderException("Rooted mode refuses " + std::string(refusal.feature) + ".");
        }
    }
}
} // namespace lbug::binder
