#include "core/schema/SQLiteSchemaProvider.hpp"

#include <string>
#include <utility>

#include <spdlog/logger.h>

#include "core/common/SqlText.hpp"

namespace sqlgui::core {

SQLiteSchemaProvider::SQLiteSchemaProvider(
    std::shared_ptr<Database> database,
    std::shared_ptr<spdlog::logger> logger)
    : database_(std::move(database)),
      logger_(std::move(logger)) {}

Expected<std::vector<ResultRow>, DatabaseError> SQLiteSchemaProvider::run_rows(
    const std::string& sql,
    std::stop_token stop_token) const {
    auto result = database_->execute(QueryRequest {.sql = sql, .page_size = 2048}, stop_token);
    if (!result) {
        return make_unexpected(result.error());
    }
    if (!result->cursor) {
        return std::vector<ResultRow> {};
    }
    const auto row_count = result->cursor->total_row_count().value_or(2048);
    auto page = result->cursor->fetch_page(0, static_cast<std::size_t>(row_count), std::nullopt, stop_token);
    if (!page) {
        return make_unexpected(page.error());
    }
    return std::move(page->rows);
}

Expected<std::vector<TableSummary>, DatabaseError> SQLiteSchemaProvider::list_tables(std::stop_token stop_token) {
    const std::string sql =
        "SELECT name "
        "FROM sqlite_master "
        "WHERE type = 'table' AND name NOT LIKE 'sqlite_%' "
        "ORDER BY name";
    auto rows = run_rows(sql, stop_token);
    if (!rows) {
        return make_unexpected(rows.error());
    }

    std::vector<TableSummary> tables;
    tables.reserve(rows->size());
    for (const auto& row : *rows) {
        if (!row.empty()) {
            tables.push_back(TableSummary {.schema = {}, .name = row[0].text});
        }
    }
    logger_->debug("Loaded {} SQLite tables", tables.size());
    return tables;
}

Expected<TableMetadata, DatabaseError> SQLiteSchemaProvider::load_table_metadata(
    const TableSummary& table,
    std::stop_token stop_token) {
    if (const auto it = cache_.find(table.name); it != cache_.end()) {
        return it->second;
    }

    TableMetadata metadata;

    auto columns = run_rows("PRAGMA table_info(" + quote_identifier(table.name) + ")", stop_token);
    if (!columns) {
        return make_unexpected(columns.error());
    }
    for (const auto& row : *columns) {
        if (row.size() < 6) {
            continue;
        }
        metadata.columns.push_back(ColumnInfo {
            .name = row[1].text,
            .type = row[2].text,
            .nullable = row[3].text != "1",
            .primary_key = row[5].text == "1",
        });
    }

    auto indexes = run_rows("PRAGMA index_list(" + quote_identifier(table.name) + ")", stop_token);
    if (!indexes) {
        return make_unexpected(indexes.error());
    }
    for (const auto& row : *indexes) {
        if (row.size() < 3) {
            continue;
        }
        IndexInfo index {
            .name = row[1].text,
            .unique = row[2].text == "1",
        };

        auto index_columns = run_rows("PRAGMA index_info(" + quote_identifier(index.name) + ")", stop_token);
        if (!index_columns) {
            return make_unexpected(index_columns.error());
        }
        for (const auto& index_row : *index_columns) {
            if (index_row.size() >= 3) {
                index.columns.push_back(index_row[2].text);
            }
        }
        metadata.indexes.push_back(std::move(index));
    }

    auto foreign_keys = run_rows("PRAGMA foreign_key_list(" + quote_identifier(table.name) + ")", stop_token);
    if (!foreign_keys) {
        return make_unexpected(foreign_keys.error());
    }
    for (const auto& row : *foreign_keys) {
        if (row.size() < 8) {
            continue;
        }
        metadata.foreign_keys.push_back(ForeignKeyInfo {
            .name = "fk_" + row[0].text,
            .from_column = row[3].text,
            .target_schema = {},
            .target_table = row[2].text,
            .target_column = row[4].text,
            .on_update = row[5].text,
            .on_delete = row[6].text,
        });
    }

    cache_[table.name] = metadata;
    return metadata;
}

void SQLiteSchemaProvider::invalidate() noexcept {
    cache_.clear();
}

}  // namespace sqlgui::core
