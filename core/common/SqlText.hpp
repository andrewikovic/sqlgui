#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "core/models/QueryModels.hpp"

namespace sqlgui::core {

struct TextLocation {
    int line {1};
    int column {1};
};

[[nodiscard]] std::string trim_sql(std::string_view sql);
[[nodiscard]] std::string normalize_sql_for_analysis(std::string_view sql);
[[nodiscard]] StatementKind classify_statement(std::string_view sql);
[[nodiscard]] bool is_dangerous_write_without_where(std::string_view sql);
[[nodiscard]] TextLocation location_from_offset(std::string_view sql, std::size_t offset);
[[nodiscard]] std::string quote_identifier(std::string_view identifier);
[[nodiscard]] std::string quote_literal(std::string_view literal);

}  // namespace sqlgui::core
