#include "core/common/SqlText.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace sqlgui::core {

namespace {

[[nodiscard]] std::string to_lower(std::string_view input) {
    std::string lowered;
    lowered.reserve(input.size());
    for (const char ch : input) {
        lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return lowered;
}

[[nodiscard]] std::string_view strip_leading_noise(std::string_view sql) {
    std::size_t cursor = 0;
    while (cursor < sql.size()) {
        if (std::isspace(static_cast<unsigned char>(sql[cursor])) != 0) {
            ++cursor;
            continue;
        }
        if (cursor + 1 < sql.size() && sql[cursor] == '-' && sql[cursor + 1] == '-') {
            cursor += 2;
            while (cursor < sql.size() && sql[cursor] != '\n') {
                ++cursor;
            }
            continue;
        }
        if (cursor + 1 < sql.size() && sql[cursor] == '/' && sql[cursor + 1] == '*') {
            cursor += 2;
            while (cursor + 1 < sql.size() && !(sql[cursor] == '*' && sql[cursor + 1] == '/')) {
                ++cursor;
            }
            cursor = std::min(cursor + 2, sql.size());
            continue;
        }
        break;
    }
    return sql.substr(cursor);
}

}  // namespace

std::string trim_sql(std::string_view sql) {
    const auto begin = sql.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) {
        return {};
    }
    const auto end = sql.find_last_not_of(" \t\r\n");
    return std::string(sql.substr(begin, end - begin + 1));
}

std::string normalize_sql_for_analysis(std::string_view sql) {
    std::string normalized;
    normalized.reserve(sql.size());

    bool in_single_quote = false;
    bool in_double_quote = false;
    bool in_line_comment = false;
    bool in_block_comment = false;

    for (std::size_t i = 0; i < sql.size(); ++i) {
        const char ch = sql[i];
        const char next = i + 1 < sql.size() ? sql[i + 1] : '\0';

        if (in_line_comment) {
            if (ch == '\n') {
                in_line_comment = false;
                normalized.push_back(' ');
            }
            continue;
        }

        if (in_block_comment) {
            if (ch == '*' && next == '/') {
                in_block_comment = false;
                ++i;
            }
            continue;
        }

        if (!in_single_quote && !in_double_quote) {
            if (ch == '-' && next == '-') {
                in_line_comment = true;
                ++i;
                continue;
            }
            if (ch == '/' && next == '*') {
                in_block_comment = true;
                ++i;
                continue;
            }
        }

        if (!in_double_quote && ch == '\'' && !in_line_comment && !in_block_comment) {
            in_single_quote = !in_single_quote;
            normalized.push_back(' ');
            continue;
        }
        if (!in_single_quote && ch == '"' && !in_line_comment && !in_block_comment) {
            in_double_quote = !in_double_quote;
            normalized.push_back(' ');
            continue;
        }

        if (in_single_quote || in_double_quote) {
            normalized.push_back(' ');
            continue;
        }

        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }

    return normalized;
}

StatementKind classify_statement(std::string_view sql) {
    const auto stripped = strip_leading_noise(sql);
    const auto lowered = to_lower(stripped.substr(0, stripped.find_first_of(" \t\r\n(")));

    if (lowered == "select" || lowered == "with" || lowered == "pragma" || lowered == "show"
        || lowered == "values" || lowered == "table") {
        return StatementKind::Select;
    }
    if (lowered == "insert") {
        return StatementKind::Insert;
    }
    if (lowered == "update") {
        return StatementKind::Update;
    }
    if (lowered == "delete") {
        return StatementKind::Delete;
    }
    if (lowered == "begin") {
        return StatementKind::Begin;
    }
    if (lowered == "commit") {
        return StatementKind::Commit;
    }
    if (lowered == "rollback") {
        return StatementKind::Rollback;
    }
    if (lowered == "explain") {
        return StatementKind::Explain;
    }
    if (lowered == "create" || lowered == "alter" || lowered == "drop") {
        return StatementKind::Ddl;
    }
    return StatementKind::Other;
}

bool is_dangerous_write_without_where(std::string_view sql) {
    const auto normalized = normalize_sql_for_analysis(sql);
    const auto trimmed = trim_sql(normalized);

    if (trimmed.starts_with("delete")) {
        return trimmed.find("where") == std::string::npos;
    }
    if (trimmed.starts_with("update")) {
        return trimmed.find("where") == std::string::npos;
    }
    return false;
}

TextLocation location_from_offset(std::string_view sql, std::size_t offset) {
    TextLocation location;
    const std::size_t safe_offset = std::min(offset, sql.size());
    for (std::size_t i = 0; i < safe_offset; ++i) {
        if (sql[i] == '\n') {
            ++location.line;
            location.column = 1;
            continue;
        }
        ++location.column;
    }
    return location;
}

std::string quote_identifier(std::string_view identifier) {
    std::ostringstream builder;
    builder << '"';
    for (const char ch : identifier) {
        if (ch == '"') {
            builder << "\"\"";
            continue;
        }
        builder << ch;
    }
    builder << '"';
    return builder.str();
}

std::string quote_literal(std::string_view literal) {
    std::ostringstream builder;
    builder << '\'';
    for (const char ch : literal) {
        if (ch == '\'') {
            builder << "''";
            continue;
        }
        builder << ch;
    }
    builder << '\'';
    return builder.str();
}

}  // namespace sqlgui::core
