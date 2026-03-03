#pragma once

#include <stop_token>
#include <vector>

#include "core/common/Error.hpp"
#include "core/common/Expected.hpp"
#include "core/models/SchemaModels.hpp"

namespace sqlgui::core {

class SchemaProvider {
public:
    virtual ~SchemaProvider() = default;

    [[nodiscard]] virtual Expected<std::vector<TableSummary>, DatabaseError> list_tables(
        std::stop_token stop_token) = 0;

    [[nodiscard]] virtual Expected<TableMetadata, DatabaseError> load_table_metadata(
        const TableSummary& table,
        std::stop_token stop_token) = 0;

    virtual void invalidate() noexcept = 0;
};

}  // namespace sqlgui::core
