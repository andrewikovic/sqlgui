#pragma once

#include <memory>
#include <unordered_map>

#include "core/database/Database.hpp"
#include "core/schema/SchemaProvider.hpp"

namespace spdlog {
class logger;
}

namespace sqlgui::core {

class PostgresSchemaProvider final : public SchemaProvider {
public:
    PostgresSchemaProvider(std::shared_ptr<Database> database, std::shared_ptr<spdlog::logger> logger);

    [[nodiscard]] Expected<std::vector<TableSummary>, DatabaseError> list_tables(
        std::stop_token stop_token) override;

    [[nodiscard]] Expected<TableMetadata, DatabaseError> load_table_metadata(
        const TableSummary& table,
        std::stop_token stop_token) override;

    void invalidate() noexcept override;

private:
    [[nodiscard]] Expected<std::vector<ResultRow>, DatabaseError> run_rows(
        const std::string& sql,
        std::stop_token stop_token) const;

    static std::string cache_key(const TableSummary& table);

    std::shared_ptr<Database> database_;
    std::shared_ptr<spdlog::logger> logger_;
    std::unordered_map<std::string, TableMetadata> cache_;
};

}  // namespace sqlgui::core
