#include "core/schema/PostgresSchemaProvider.hpp"

#include <sstream>
#include <string>
#include <utility>

#include <spdlog/logger.h>

#include "core/common/SqlText.hpp"

namespace sqlgui::core {

namespace {

[[nodiscard]] std::vector<std::string> split_columns(const std::string& raw_columns) {
    std::vector<std::string> columns;
    std::string current;
    int depth = 0;
    for (const char ch : raw_columns) {
        if (ch == '(') {
            ++depth;
        } else if (ch == ')') {
            --depth;
        }

        if (ch == ',' && depth == 0) {
            columns.push_back(trim_sql(current));
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!trim_sql(current).empty()) {
        columns.push_back(trim_sql(current));
    }
    return columns;
}

}  // namespace

PostgresSchemaProvider::PostgresSchemaProvider(
    std::shared_ptr<Database> database,
    std::shared_ptr<spdlog::logger> logger)
    : database_(std::move(database)),
      logger_(std::move(logger)) {}

Expected<std::vector<ResultRow>, DatabaseError> PostgresSchemaProvider::run_rows(
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

std::string PostgresSchemaProvider::cache_key(const TableSummary& table) {
    return table.schema + "." + table.name;
}

Expected<std::vector<TableSummary>, DatabaseError> PostgresSchemaProvider::list_tables(std::stop_token stop_token) {
    const std::string sql =
        "SELECT table_schema, table_name "
        "FROM information_schema.tables "
        "WHERE table_type = 'BASE TABLE' "
        "AND table_schema NOT IN ('pg_catalog', 'information_schema') "
        "ORDER BY table_schema, table_name";

    auto rows = run_rows(sql, stop_token);
    if (!rows) {
        return make_unexpected(rows.error());
    }

    std::vector<TableSummary> tables;
    tables.reserve(rows->size());
    for (const auto& row : *rows) {
        if (row.size() >= 2) {
            tables.push_back(TableSummary {.schema = row[0].text, .name = row[1].text});
        }
    }
    logger_->debug("Loaded {} PostgreSQL tables", tables.size());
    return tables;
}

Expected<TableMetadata, DatabaseError> PostgresSchemaProvider::load_table_metadata(
    const TableSummary& table,
    std::stop_token stop_token) {
    const auto key = cache_key(table);
    if (const auto it = cache_.find(key); it != cache_.end()) {
        return it->second;
    }

    TableMetadata metadata;
    const std::string schema = quote_literal(table.schema);
    const std::string name = quote_literal(table.name);

    auto columns = run_rows(
        "SELECT column_name, data_type, is_nullable, "
        "       CASE WHEN column_name IN ("
        "           SELECT kcu.column_name "
        "           FROM information_schema.table_constraints tc "
        "           JOIN information_schema.key_column_usage kcu "
        "             ON tc.constraint_name = kcu.constraint_name "
        "            AND tc.table_schema = kcu.table_schema "
        "          WHERE tc.constraint_type = 'PRIMARY KEY' "
        "            AND tc.table_schema = " + schema +
        "            AND tc.table_name = " + name +
        "       ) THEN '1' ELSE '0' END AS is_primary "
        "FROM information_schema.columns "
        "WHERE table_schema = " + schema + " AND table_name = " + name +
        " ORDER BY ordinal_position",
        stop_token);
    if (!columns) {
        return make_unexpected(columns.error());
    }
    for (const auto& row : *columns) {
        if (row.size() < 4) {
            continue;
        }
        metadata.columns.push_back(ColumnInfo {
            .name = row[0].text,
            .type = row[1].text,
            .nullable = row[2].text == "YES",
            .primary_key = row[3].text == "1",
        });
    }

    auto indexes = run_rows(
        "SELECT indexname, indexdef "
        "FROM pg_indexes "
        "WHERE schemaname = " + schema + " AND tablename = " + name +
        " ORDER BY indexname",
        stop_token);
    if (!indexes) {
        return make_unexpected(indexes.error());
    }
    for (const auto& row : *indexes) {
        if (row.size() < 2) {
            continue;
        }
        const auto open = row[1].text.find('(');
        const auto close = row[1].text.rfind(')');
        const auto raw_columns = (open != std::string::npos && close != std::string::npos && close > open)
            ? row[1].text.substr(open + 1, close - open - 1)
            : std::string {};

        metadata.indexes.push_back(IndexInfo {
            .name = row[0].text,
            .unique = row[1].text.find("UNIQUE INDEX") != std::string::npos,
            .columns = split_columns(raw_columns),
        });
    }

    auto foreign_keys = run_rows(
        "SELECT tc.constraint_name, kcu.column_name, ccu.table_schema, ccu.table_name, ccu.column_name, "
        "       rc.update_rule, rc.delete_rule "
        "FROM information_schema.table_constraints tc "
        "JOIN information_schema.key_column_usage kcu "
        "  ON tc.constraint_name = kcu.constraint_name "
        " AND tc.table_schema = kcu.table_schema "
        "JOIN information_schema.constraint_column_usage ccu "
        "  ON ccu.constraint_name = tc.constraint_name "
        " AND ccu.constraint_schema = tc.table_schema "
        "JOIN information_schema.referential_constraints rc "
        "  ON rc.constraint_name = tc.constraint_name "
        " AND rc.constraint_schema = tc.table_schema "
        "WHERE tc.constraint_type = 'FOREIGN KEY' "
        "  AND tc.table_schema = " + schema +
        "  AND tc.table_name = " + name +
        " ORDER BY tc.constraint_name, kcu.ordinal_position",
        stop_token);
    if (!foreign_keys) {
        return make_unexpected(foreign_keys.error());
    }
    for (const auto& row : *foreign_keys) {
        if (row.size() < 7) {
            continue;
        }
        metadata.foreign_keys.push_back(ForeignKeyInfo {
            .name = row[0].text,
            .from_column = row[1].text,
            .target_schema = row[2].text,
            .target_table = row[3].text,
            .target_column = row[4].text,
            .on_update = row[5].text,
            .on_delete = row[6].text,
        });
    }

    cache_[key] = metadata;
    return metadata;
}

void PostgresSchemaProvider::invalidate() noexcept {
    cache_.clear();
}

}  // namespace sqlgui::core
