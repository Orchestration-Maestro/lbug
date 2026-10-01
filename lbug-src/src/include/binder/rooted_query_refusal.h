#pragma once

#include <string_view>
#include <variant>

#include "common/enums/scan_source_type.h"
#include "common/enums/statement_type.h"
#include "common/enums/storage_format.h"
#include "extension/extension_action.h"

namespace lbug::main { class ClientContext; }

namespace lbug::binder {
using RootedQueryKind = std::variant<common::StatementType, extension::ExtensionAction,
    common::ScanSourceType, common::StorageFormat, std::string_view>;

// Structural preflight: binders can access files before a BoundStatement exists.
// Recovery and contextual helper entry points share the same refusal table.
void refuseRootedQuery(const main::ClientContext& context, const RootedQueryKind& kind);
} // namespace lbug::binder
