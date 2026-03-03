#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

#include "core/common/Error.hpp"
#include "core/common/Expected.hpp"

namespace sqlgui::core {

enum class StatementKind {
    Select,
    Insert,
    Update,
    Delete,
    Begin,
    Commit,
    Rollback,
    Explain,
    Ddl,
    Other,
};

enum class SortDirection {
    Ascending,
    Descending,
};

struct SortSpec {
    std::size_t column_index {0};
    SortDirection direction {SortDirection::Ascending};
};

struct ColumnDefinition {
    std::string name;
    std::string declared_type;
    bool nullable {true};
};

struct ResultCell {
    std::string text;
    bool is_null {false};
};

using ResultRow = std::vector<ResultCell>;

struct ResultPage {
    std::vector<ResultRow> rows;
    std::size_t offset {0};
    std::size_t limit {0};
    std::optional<std::uint64_t> total_row_count;
    bool has_more {false};
};

struct ExplainNode {
    std::string node_type;
    std::string relation_name;
    std::optional<double> startup_cost;
    std::optional<double> total_cost;
    std::optional<double> rows;
    std::vector<ExplainNode> children;
};

class QueryCursor {
public:
    virtual ~QueryCursor() = default;

    [[nodiscard]] virtual const std::vector<ColumnDefinition>& columns() const noexcept = 0;
    [[nodiscard]] virtual std::optional<std::uint64_t> total_row_count() const noexcept = 0;
    [[nodiscard]] virtual Expected<ResultPage, DatabaseError> fetch_page(
        std::size_t offset,
        std::size_t limit,
        const std::optional<SortSpec>& sort,
        std::stop_token stop_token) = 0;
};

struct QueryRequest {
    std::string sql;
    bool explain {false};
    std::size_t page_size {250};
};

struct QueryResult {
    StatementKind statement_kind {StatementKind::Other};
    std::unique_ptr<QueryCursor> cursor;
    std::uint64_t affected_rows {0};
    std::chrono::milliseconds execution_time {0};
    std::optional<ExplainNode> explain_plan;
};

}  // namespace sqlgui::core
