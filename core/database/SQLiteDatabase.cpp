#include "core/database/SQLiteDatabase.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <spdlog/logger.h>
#include <sqlite3.h>

#include "core/common/SqlText.hpp"

namespace sqlgui::core {

using SqliteHandle = std::unique_ptr<sqlite3, decltype(&sqlite3_close_v2)>;
using SqliteStatement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

struct SQLiteDatabase::ConnectionState {
    SqliteHandle handle {nullptr, &sqlite3_close_v2};
    std::mutex mutex;
    bool autocommit_enabled {true};
    bool in_transaction {false};
};

namespace {

[[nodiscard]] std::string normalize_statement_text(std::string_view sql) {
    std::string normalized = trim_sql(sql);
    while (!normalized.empty() && normalized.back() == ';') {
        normalized.pop_back();
        normalized = trim_sql(normalized);
    }
    return normalized;
}

[[nodiscard]] DatabaseError sqlite_error(
    sqlite3* handle,
    ErrorCategory category,
    std::string message,
    std::optional<TextLocation> location = std::nullopt) {
    DatabaseError error {
        .category = category,
        .message = std::move(message),
        .details = handle != nullptr ? sqlite3_errmsg(handle) : std::string {},
    };
    if (location.has_value()) {
        error.line = location->line;
        error.column = location->column;
    }
    return error;
}

class SQLiteProgressGuard {
public:
    SQLiteProgressGuard(sqlite3* handle, std::stop_token stop_token)
        : handle_(handle) {
        if (stop_token.stop_possible()) {
            callback_.emplace(stop_token, [this] {
                cancelled_.store(true, std::memory_order_relaxed);
                sqlite3_interrupt(handle_);
            });
        }
        sqlite3_progress_handler(handle_, 1000, &SQLiteProgressGuard::progress, this);
    }

    ~SQLiteProgressGuard() {
        sqlite3_progress_handler(handle_, 0, nullptr, nullptr);
    }

private:
    static int progress(void* context) {
        const auto* guard = static_cast<SQLiteProgressGuard*>(context);
        return guard->cancelled_.load(std::memory_order_relaxed) ? 1 : 0;
    }

    sqlite3* handle_ {nullptr};
    std::atomic_bool cancelled_ {false};
    std::optional<std::stop_callback<std::function<void()>>> callback_;
};

[[nodiscard]] std::string cell_to_string(sqlite3_stmt* statement, int column) {
    const int type = sqlite3_column_type(statement, column);
    switch (type) {
        case SQLITE_INTEGER:
            return std::to_string(sqlite3_column_int64(statement, column));
        case SQLITE_FLOAT: {
            std::ostringstream builder;
            builder << sqlite3_column_double(statement, column);
            return builder.str();
        }
        case SQLITE_TEXT: {
            const auto* value = reinterpret_cast<const char*>(sqlite3_column_text(statement, column));
            return value != nullptr ? std::string(value) : std::string {};
        }
        case SQLITE_BLOB:
            return "<blob " + std::to_string(sqlite3_column_bytes(statement, column)) + " bytes>";
        case SQLITE_NULL:
        default:
            return {};
    }
}

[[nodiscard]] ResultRow read_row(sqlite3_stmt* statement) {
    ResultRow row;
    const int column_count = sqlite3_column_count(statement);
    row.reserve(static_cast<std::size_t>(column_count));
    for (int column = 0; column < column_count; ++column) {
        const bool is_null = sqlite3_column_type(statement, column) == SQLITE_NULL;
        row.push_back(ResultCell {.text = cell_to_string(statement, column), .is_null = is_null});
    }
    return row;
}

[[nodiscard]] Expected<void, DatabaseError> execute_no_result(sqlite3* handle, std::string_view sql, std::stop_token stop_token) {
    const std::string statement_text = normalize_statement_text(sql);
    sqlite3_stmt* raw_statement = nullptr;
    int rc = sqlite3_prepare_v3(
        handle,
        statement_text.c_str(),
        -1,
        SQLITE_PREPARE_PERSISTENT,
        &raw_statement,
        nullptr);
    SqliteStatement statement(raw_statement, &sqlite3_finalize);
    if (rc != SQLITE_OK) {
        return make_unexpected(sqlite_error(handle, ErrorCategory::Query, "Failed to prepare statement"));
    }

    SQLiteProgressGuard progress_guard(handle, stop_token);
    while ((rc = sqlite3_step(statement.get())) == SQLITE_ROW) {
    }
    if (rc == SQLITE_INTERRUPT || stop_token.stop_requested()) {
        return make_unexpected(DatabaseError::cancelled());
    }
    if (rc != SQLITE_DONE) {
        return make_unexpected(sqlite_error(handle, ErrorCategory::Query, "Statement execution failed"));
    }
    return {};
}

class SQLitePagedQueryCursor final : public QueryCursor {
public:
    SQLitePagedQueryCursor(
        std::shared_ptr<SQLiteDatabase::ConnectionState> state,
        std::shared_ptr<spdlog::logger> logger,
        std::string base_sql,
        std::vector<ColumnDefinition> columns,
        std::uint64_t total_row_count)
        : state_(std::move(state)),
          logger_(std::move(logger)),
          base_sql_(std::move(base_sql)),
          columns_(std::move(columns)),
          total_row_count_(total_row_count) {}

    [[nodiscard]] static Expected<std::unique_ptr<QueryCursor>, DatabaseError> create(
        const std::shared_ptr<SQLiteDatabase::ConnectionState>& state,
        const std::shared_ptr<spdlog::logger>& logger,
        const std::string& base_sql,
        std::stop_token stop_token) {
        std::vector<ColumnDefinition> columns;
        {
            sqlite3_stmt* raw_statement = nullptr;
            const int rc = sqlite3_prepare_v3(
                state->handle.get(),
                base_sql.c_str(),
                -1,
                SQLITE_PREPARE_PERSISTENT,
                &raw_statement,
                nullptr);
            SqliteStatement statement(raw_statement, &sqlite3_finalize);
            if (rc != SQLITE_OK) {
                return make_unexpected(sqlite_error(state->handle.get(), ErrorCategory::Query, "Failed to inspect result columns"));
            }
            const int column_count = sqlite3_column_count(statement.get());
            columns.reserve(static_cast<std::size_t>(column_count));
            for (int column = 0; column < column_count; ++column) {
                const auto* name = sqlite3_column_name(statement.get(), column);
                const auto* declared_type = sqlite3_column_decltype(statement.get(), column);
                columns.push_back(ColumnDefinition {
                    .name = name != nullptr ? std::string(name) : "column_" + std::to_string(column),
                    .declared_type = declared_type != nullptr ? std::string(declared_type) : std::string {},
                    .nullable = true,
                });
            }
        }

        std::ostringstream count_sql;
        count_sql << "SELECT COUNT(*) FROM (" << base_sql << ") AS _sqlgui_count";
        sqlite3_stmt* raw_count = nullptr;
        const int rc = sqlite3_prepare_v3(
            state->handle.get(),
            count_sql.str().c_str(),
            -1,
            SQLITE_PREPARE_PERSISTENT,
            &raw_count,
            nullptr);
        SqliteStatement count_statement(raw_count, &sqlite3_finalize);
        if (rc != SQLITE_OK) {
            return make_unexpected(sqlite_error(state->handle.get(), ErrorCategory::Query, "Failed to count result rows"));
        }

        SQLiteProgressGuard progress_guard(state->handle.get(), stop_token);
        const int step_rc = sqlite3_step(count_statement.get());
        if (step_rc == SQLITE_INTERRUPT || stop_token.stop_requested()) {
            return make_unexpected(DatabaseError::cancelled());
        }
        if (step_rc != SQLITE_ROW) {
            return make_unexpected(sqlite_error(state->handle.get(), ErrorCategory::Query, "Failed to read row count"));
        }

        const auto row_count = static_cast<std::uint64_t>(sqlite3_column_int64(count_statement.get(), 0));
        return std::unique_ptr<QueryCursor>(
            new SQLitePagedQueryCursor(state, logger, base_sql, std::move(columns), row_count));
    }

    [[nodiscard]] const std::vector<ColumnDefinition>& columns() const noexcept override {
        return columns_;
    }

    [[nodiscard]] std::optional<std::uint64_t> total_row_count() const noexcept override {
        return total_row_count_;
    }

    [[nodiscard]] Expected<ResultPage, DatabaseError> fetch_page(
        std::size_t offset,
        std::size_t limit,
        const std::optional<SortSpec>& sort,
        std::stop_token stop_token) override {
        std::scoped_lock lock(state_->mutex);

        std::ostringstream sql;
        sql << "SELECT * FROM (" << base_sql_ << ") AS _sqlgui_page";
        if (sort.has_value() && sort->column_index < columns_.size()) {
            sql << " ORDER BY " << quote_identifier(columns_[sort->column_index].name)
                << (sort->direction == SortDirection::Ascending ? " ASC" : " DESC");
        }
        sql << " LIMIT " << limit << " OFFSET " << offset;

        sqlite3_stmt* raw_statement = nullptr;
        const int rc = sqlite3_prepare_v3(
            state_->handle.get(),
            sql.str().c_str(),
            -1,
            SQLITE_PREPARE_PERSISTENT,
            &raw_statement,
            nullptr);
        SqliteStatement statement(raw_statement, &sqlite3_finalize);
        if (rc != SQLITE_OK) {
            return make_unexpected(sqlite_error(state_->handle.get(), ErrorCategory::Query, "Failed to prepare result page"));
        }

        ResultPage page;
        page.offset = offset;
        page.limit = limit;
        page.total_row_count = total_row_count_;

        SQLiteProgressGuard progress_guard(state_->handle.get(), stop_token);
        int step_rc = SQLITE_OK;
        while ((step_rc = sqlite3_step(statement.get())) == SQLITE_ROW) {
            page.rows.push_back(read_row(statement.get()));
        }

        if (step_rc == SQLITE_INTERRUPT || stop_token.stop_requested()) {
            return make_unexpected(DatabaseError::cancelled());
        }
        if (step_rc != SQLITE_DONE) {
            return make_unexpected(sqlite_error(state_->handle.get(), ErrorCategory::Query, "Failed to fetch result page"));
        }

        page.has_more = offset + page.rows.size() < total_row_count_;
        logger_->debug("Fetched SQLite page offset={} limit={}", offset, limit);
        return page;
    }

private:
    std::shared_ptr<SQLiteDatabase::ConnectionState> state_;
    std::shared_ptr<spdlog::logger> logger_;
    std::string base_sql_;
    std::vector<ColumnDefinition> columns_;
    std::uint64_t total_row_count_ {0};
};

}  // namespace

SQLiteDatabase::SQLiteDatabase(std::shared_ptr<spdlog::logger> logger)
    : state_(std::make_shared<ConnectionState>()),
      logger_(std::move(logger)) {}

SQLiteDatabase::~SQLiteDatabase() {
    close();
}

Expected<void, DatabaseError> SQLiteDatabase::open(const ConnectionConfig& config) {
    if (config.kind != DatabaseKind::SQLite) {
        return make_unexpected(DatabaseError::validation("SQLiteDatabase received a non-SQLite configuration"));
    }

    {
        std::scoped_lock lock(state_->mutex);
        sqlite3* raw_handle = nullptr;
        const int rc = sqlite3_open_v2(
            config.sqlite_path.string().c_str(),
            &raw_handle,
            SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX,
            nullptr);
        if (rc != SQLITE_OK) {
            DatabaseError error = sqlite_error(raw_handle, ErrorCategory::Connection, "Failed to open SQLite database");
            if (raw_handle != nullptr) {
                sqlite3_close_v2(raw_handle);
            }
            return make_unexpected(std::move(error));
        }

        state_->handle.reset(raw_handle);
        sqlite3_extended_result_codes(state_->handle.get(), 1);
        sqlite3_busy_timeout(state_->handle.get(), 5000);
        state_->autocommit_enabled = true;
        state_->in_transaction = false;
        config_ = config;
    }

    logger_->info("Opened SQLite database {}", config.sqlite_path.string());
    if (config.autocommit) {
        return {};
    }

    return set_autocommit(false);
}

void SQLiteDatabase::close() noexcept {
    std::scoped_lock lock(state_->mutex);
    state_->handle.reset();
    state_->in_transaction = false;
}

bool SQLiteDatabase::is_open() const noexcept {
    std::scoped_lock lock(state_->mutex);
    return state_->handle != nullptr;
}

const ConnectionConfig& SQLiteDatabase::config() const noexcept {
    return config_;
}

Expected<QueryResult, DatabaseError> SQLiteDatabase::execute(
    const QueryRequest& request,
    std::stop_token stop_token) {
    if (request.explain) {
        return make_unexpected(DatabaseError::validation("EXPLAIN analysis is implemented for PostgreSQL connections only"));
    }

    std::scoped_lock lock(state_->mutex);
    if (state_->handle == nullptr) {
        return make_unexpected(DatabaseError {
            .category = ErrorCategory::Connection,
            .message = "SQLite connection is not open",
        });
    }

    const auto started_at = std::chrono::steady_clock::now();
    QueryResult result;
    std::string row_query;
    std::string_view remaining = request.sql;
    std::size_t offset = 0;
    bool saw_statement = false;

    while (true) {
        const auto next_content = remaining.find_first_not_of(" \t\r\n");
        if (next_content == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(next_content);
        offset += next_content;

        const char* tail = nullptr;
        sqlite3_stmt* raw_statement = nullptr;
        const int rc = sqlite3_prepare_v3(
            state_->handle.get(),
            remaining.data(),
            static_cast<int>(remaining.size()),
            SQLITE_PREPARE_PERSISTENT,
            &raw_statement,
            &tail);
        SqliteStatement statement(raw_statement, &sqlite3_finalize);
        const std::size_t consumed = tail != nullptr
            ? static_cast<std::size_t>(tail - remaining.data())
            : remaining.size();
        if (rc != SQLITE_OK) {
            const auto location = location_from_offset(request.sql, offset);
            return make_unexpected(sqlite_error(
                state_->handle.get(),
                ErrorCategory::Query,
                "Failed to prepare SQL statement",
                location));
        }

        if (!statement) {
            offset += consumed;
            remaining.remove_prefix(consumed);
            continue;
        }

        saw_statement = true;
        const std::string current_sql = normalize_statement_text(std::string_view {remaining.data(), consumed});
        result.statement_kind = classify_statement(current_sql);

        if (sqlite3_column_count(statement.get()) > 0) {
            row_query = current_sql;
        } else {
            SQLiteProgressGuard progress_guard(state_->handle.get(), stop_token);
            int step_rc = SQLITE_OK;
            while ((step_rc = sqlite3_step(statement.get())) == SQLITE_ROW) {
            }
            if (step_rc == SQLITE_INTERRUPT || stop_token.stop_requested()) {
                return make_unexpected(DatabaseError::cancelled());
            }
            if (step_rc != SQLITE_DONE) {
                const auto location = location_from_offset(request.sql, offset);
                return make_unexpected(sqlite_error(
                    state_->handle.get(),
                    ErrorCategory::Query,
                    "Statement execution failed",
                    location));
            }

            const int changes = sqlite3_changes(state_->handle.get());
            if (changes > 0) {
                result.affected_rows += static_cast<std::uint64_t>(changes);
            }
        }

        offset += consumed;
        remaining.remove_prefix(consumed);
    }

    if (!saw_statement) {
        return make_unexpected(DatabaseError::validation("SQL text is empty"));
    }

    if (!row_query.empty()) {
        auto cursor = SQLitePagedQueryCursor::create(state_, logger_, row_query, stop_token);
        if (!cursor) {
            return make_unexpected(cursor.error());
        }
        result.cursor = std::move(*cursor);
    }

    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at);
    logger_->info("SQLite query completed in {} ms", result.execution_time.count());
    return result;
}

Expected<void, DatabaseError> SQLiteDatabase::cancel_running_query() noexcept {
    sqlite3* handle = nullptr;
    {
        std::scoped_lock lock(state_->mutex);
        handle = state_->handle.get();
    }
    if (handle != nullptr) {
        sqlite3_interrupt(handle);
    }
    return {};
}

Expected<void, DatabaseError> SQLiteDatabase::set_autocommit(bool enabled) {
    std::scoped_lock lock(state_->mutex);
    if (state_->handle == nullptr) {
        return make_unexpected(DatabaseError {
            .category = ErrorCategory::Connection,
            .message = "SQLite connection is not open",
        });
    }

    if (state_->autocommit_enabled == enabled) {
        return {};
    }

    if (enabled) {
        if (state_->in_transaction) {
            if (auto committed = execute_no_result(state_->handle.get(), "COMMIT", {}); !committed) {
                return committed;
            }
            state_->in_transaction = false;
        }
        state_->autocommit_enabled = true;
        return {};
    }

    if (auto begun = execute_no_result(state_->handle.get(), "BEGIN IMMEDIATE", {}); !begun) {
        return begun;
    }
    state_->autocommit_enabled = false;
    state_->in_transaction = true;
    return {};
}

bool SQLiteDatabase::autocommit() const noexcept {
    std::scoped_lock lock(state_->mutex);
    return state_->autocommit_enabled;
}

Expected<void, DatabaseError> SQLiteDatabase::begin_transaction() {
    std::scoped_lock lock(state_->mutex);
    if (state_->handle == nullptr) {
        return make_unexpected(DatabaseError {
            .category = ErrorCategory::Connection,
            .message = "SQLite connection is not open",
        });
    }
    if (state_->in_transaction) {
        return {};
    }
    if (auto begun = execute_no_result(state_->handle.get(), "BEGIN IMMEDIATE", {}); !begun) {
        return begun;
    }
    state_->in_transaction = true;
    return {};
}

Expected<void, DatabaseError> SQLiteDatabase::commit() {
    std::scoped_lock lock(state_->mutex);
    if (state_->handle == nullptr) {
        return make_unexpected(DatabaseError {
            .category = ErrorCategory::Connection,
            .message = "SQLite connection is not open",
        });
    }
    if (!state_->in_transaction) {
        return {};
    }
    if (auto committed = execute_no_result(state_->handle.get(), "COMMIT", {}); !committed) {
        return committed;
    }
    state_->in_transaction = false;
    if (!state_->autocommit_enabled) {
        if (auto begun = execute_no_result(state_->handle.get(), "BEGIN IMMEDIATE", {}); !begun) {
            return begun;
        }
        state_->in_transaction = true;
    }
    return {};
}

Expected<void, DatabaseError> SQLiteDatabase::rollback() {
    std::scoped_lock lock(state_->mutex);
    if (state_->handle == nullptr) {
        return make_unexpected(DatabaseError {
            .category = ErrorCategory::Connection,
            .message = "SQLite connection is not open",
        });
    }
    if (!state_->in_transaction) {
        return {};
    }
    if (auto rolled_back = execute_no_result(state_->handle.get(), "ROLLBACK", {}); !rolled_back) {
        return rolled_back;
    }
    state_->in_transaction = false;
    if (!state_->autocommit_enabled) {
        if (auto begun = execute_no_result(state_->handle.get(), "BEGIN IMMEDIATE", {}); !begun) {
            return begun;
        }
        state_->in_transaction = true;
    }
    return {};
}

}  // namespace sqlgui::core
