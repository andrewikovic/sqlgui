#pragma once

#include <optional>
#include <string>

namespace sqlgui::core {

enum class ErrorCategory {
    Connection,
    Query,
    Schema,
    Cancelled,
    Validation,
    Internal,
};

struct DatabaseError {
    ErrorCategory category {ErrorCategory::Internal};
    std::string message;
    std::string details;
    std::string sql_state;
    std::optional<int> line;
    std::optional<int> column;

    [[nodiscard]] static DatabaseError cancelled(std::string message = "Operation cancelled") {
        return DatabaseError {
            .category = ErrorCategory::Cancelled,
            .message = std::move(message),
        };
    }

    [[nodiscard]] static DatabaseError validation(std::string message) {
        return DatabaseError {
            .category = ErrorCategory::Validation,
            .message = std::move(message),
        };
    }
};

[[nodiscard]] inline bool is_cancelled(const DatabaseError& error) noexcept {
    return error.category == ErrorCategory::Cancelled;
}

}  // namespace sqlgui::core
