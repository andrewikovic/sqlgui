#include "core/database/PostgresDatabase.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/logger.h>

#include "core/common/SqlText.hpp"

#if SQLGUI_HAS_POSTGRES
#include <libpq-fe.h>
#endif

namespace sqlgui::core {

#if SQLGUI_HAS_POSTGRES

using PgHandle = std::unique_ptr<PGconn, decltype(&PQfinish)>;
using PgResult = std::unique_ptr<PGresult, decltype(&PQclear)>;

struct PostgresDatabase::ConnectionState {
    PgHandle handle {nullptr, &PQfinish};
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

[[nodiscard]] DatabaseError postgres_error(
    PGconn* handle,
    ErrorCategory category,
    std::string message,
    PGresult* result = nullptr,
    std::optional<TextLocation> location = std::nullopt) {
    DatabaseError error {
        .category = category,
        .message = std::move(message),
        .details = result != nullptr ? PQresultErrorMessage(result) : PQerrorMessage(handle),
    };
    if (result != nullptr) {
        if (const char* state = PQresultErrorField(result, PG_DIAG_SQLSTATE); state != nullptr) {
            error.sql_state = state;
        }
    }
    if (location.has_value()) {
        error.line = location->line;
        error.column = location->column;
    }
    return error;
}

[[nodiscard]] std::string make_connection_string(const ConnectionConfig& config) {
    std::ostringstream builder;
    builder << "host=" << quote_literal(config.host)
            << " port=" << config.port
            << " dbname=" << quote_literal(config.database)
            << " user=" << quote_literal(config.user)
            << " password=" << quote_literal(config.password);
    return builder.str();
}

[[nodiscard]] std::string cell_to_string(PGresult* result, int row, int column) {
    return PQgetvalue(result, row, column);
}

[[nodiscard]] ResultRow read_row(PGresult* result, int row_index) {
    ResultRow row;
    const int columns = PQnfields(result);
    row.reserve(static_cast<std::size_t>(columns));
    for (int column = 0; column < columns; ++column) {
        const bool is_null = PQgetisnull(result, row_index, column) != 0;
        row.push_back(ResultCell {
            .text = is_null ? std::string {} : cell_to_string(result, row_index, column),
            .is_null = is_null,
        });
    }
    return row;
}

class CancellationBridge {
public:
    CancellationBridge(PGconn* handle, std::stop_token stop_token)
        : handle_(handle) {
        if (stop_token.stop_possible()) {
            callback_.emplace(stop_token, [this] { request_cancel(); });
        }
    }

private:
    void request_cancel() {
        if (PGcancel* cancel = PQgetCancel(handle_); cancel != nullptr) {
            char error_buffer[256] {};
            PQcancel(cancel, error_buffer, static_cast<int>(sizeof(error_buffer)));
            PQfreeCancel(cancel);
        }
    }

    PGconn* handle_ {nullptr};
    std::optional<std::stop_callback<std::function<void()>>> callback_;
};

class PostgresPagedQueryCursor final : public QueryCursor {
public:
    PostgresPagedQueryCursor(
        std::shared_ptr<PostgresDatabase::ConnectionState> state,
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
        const std::shared_ptr<PostgresDatabase::ConnectionState>& state,
        const std::shared_ptr<spdlog::logger>& logger,
        const std::string& base_sql,
        std::stop_token stop_token) {
        std::vector<ColumnDefinition> columns;
        {
            const std::string describe_sql = "SELECT * FROM (" + base_sql + ") AS _sqlgui_meta LIMIT 0";
            CancellationBridge cancellation(state->handle.get(), stop_token);
            PgResult describe_result(PQexec(state->handle.get(), describe_sql.c_str()), &PQclear);
            if (describe_result == nullptr) {
                return make_unexpected(DatabaseError {
                    .category = ErrorCategory::Query,
                    .message = "Failed to inspect PostgreSQL result columns",
                    .details = PQerrorMessage(state->handle.get()),
                });
            }
            if (PQresultStatus(describe_result.get()) != PGRES_TUPLES_OK) {
                return make_unexpected(postgres_error(
                    state->handle.get(),
                    ErrorCategory::Query,
                    "Failed to inspect PostgreSQL result columns",
                    describe_result.get()));
            }
            const int column_count = PQnfields(describe_result.get());
            columns.reserve(static_cast<std::size_t>(column_count));
            for (int column = 0; column < column_count; ++column) {
                columns.push_back(ColumnDefinition {
                    .name = PQfname(describe_result.get(), column),
                    .declared_type = std::to_string(PQftype(describe_result.get(), column)),
                    .nullable = true,
                });
            }
        }

        const std::string count_sql = "SELECT COUNT(*) FROM (" + base_sql + ") AS _sqlgui_count";
        CancellationBridge cancellation(state->handle.get(), stop_token);
        PgResult count_result(PQexec(state->handle.get(), count_sql.c_str()), &PQclear);
        if (count_result == nullptr) {
            return make_unexpected(DatabaseError {
                .category = ErrorCategory::Query,
                .message = "Failed to count PostgreSQL result rows",
                .details = PQerrorMessage(state->handle.get()),
            });
        }
        if (PQresultStatus(count_result.get()) != PGRES_TUPLES_OK) {
            return make_unexpected(postgres_error(
                state->handle.get(),
                ErrorCategory::Query,
                "Failed to count PostgreSQL result rows",
                count_result.get()));
        }

        const auto row_count = static_cast<std::uint64_t>(
            std::stoull(PQgetvalue(count_result.get(), 0, 0)));
        return std::unique_ptr<QueryCursor>(
            new PostgresPagedQueryCursor(state, logger, base_sql, std::move(columns), row_count));
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

        CancellationBridge cancellation(state_->handle.get(), stop_token);
        PgResult result(PQexec(state_->handle.get(), sql.str().c_str()), &PQclear);
        if (result == nullptr) {
            return make_unexpected(DatabaseError {
                .category = ErrorCategory::Query,
                .message = "Failed to fetch PostgreSQL result page",
                .details = PQerrorMessage(state_->handle.get()),
            });
        }
        if (PQresultStatus(result.get()) != PGRES_TUPLES_OK) {
            if (const char* sql_state = PQresultErrorField(result.get(), PG_DIAG_SQLSTATE);
                sql_state != nullptr && std::string_view(sql_state) == "57014") {
                return make_unexpected(DatabaseError::cancelled());
            }
            return make_unexpected(postgres_error(
                state_->handle.get(),
                ErrorCategory::Query,
                "Failed to fetch PostgreSQL result page",
                result.get()));
        }

        ResultPage page;
        page.offset = offset;
        page.limit = limit;
        page.total_row_count = total_row_count_;
        const int row_count = PQntuples(result.get());
        page.rows.reserve(static_cast<std::size_t>(row_count));
        for (int row = 0; row < row_count; ++row) {
            page.rows.push_back(read_row(result.get(), row));
        }
        page.has_more = offset + page.rows.size() < total_row_count_;
        logger_->debug("Fetched PostgreSQL page offset={} limit={}", offset, limit);
        return page;
    }

private:
    std::shared_ptr<PostgresDatabase::ConnectionState> state_;
    std::shared_ptr<spdlog::logger> logger_;
    std::string base_sql_;
    std::vector<ColumnDefinition> columns_;
    std::uint64_t total_row_count_ {0};
};

[[nodiscard]] ExplainNode parse_plan_node(const nlohmann::json& node) {
    ExplainNode plan {
        .node_type = node.value("Node Type", ""),
        .relation_name = node.value("Relation Name", ""),
        .startup_cost = node.contains("Startup Cost") ? std::optional<double>(node["Startup Cost"].get<double>()) : std::nullopt,
        .total_cost = node.contains("Total Cost") ? std::optional<double>(node["Total Cost"].get<double>()) : std::nullopt,
        .rows = node.contains("Plan Rows") ? std::optional<double>(node["Plan Rows"].get<double>()) : std::nullopt,
    };
    if (node.contains("Plans")) {
        for (const auto& child : node["Plans"]) {
            plan.children.push_back(parse_plan_node(child));
        }
    }
    return plan;
}

[[nodiscard]] Expected<void, DatabaseError> execute_no_result(PGconn* handle, std::string_view sql) {
    PgResult result(PQexec(handle, std::string(sql).c_str()), &PQclear);
    if (result == nullptr) {
        return make_unexpected(DatabaseError {
            .category = ErrorCategory::Query,
            .message = "Failed to execute PostgreSQL transaction statement",
            .details = PQerrorMessage(handle),
        });
    }
    if (PQresultStatus(result.get()) != PGRES_COMMAND_OK) {
        return make_unexpected(postgres_error(
            handle,
            ErrorCategory::Query,
            "Failed to execute PostgreSQL transaction statement",
            result.get()));
    }
    return {};
}

}  // namespace

PostgresDatabase::PostgresDatabase(std::shared_ptr<spdlog::logger> logger)
    : state_(std::make_shared<ConnectionState>()),
      logger_(std::move(logger)) {}

PostgresDatabase::~PostgresDatabase() {
    close();
}

Expected<void, DatabaseError> PostgresDatabase::open(const ConnectionConfig& config) {
    if (config.kind != DatabaseKind::PostgreSQL) {
        return make_unexpected(DatabaseError::validation("PostgresDatabase received a non-PostgreSQL configuration"));
    }

    {
        std::scoped_lock lock(state_->mutex);
        PGconn* raw_handle = PQconnectdb(make_connection_string(config).c_str());
        if (raw_handle == nullptr || PQstatus(raw_handle) != CONNECTION_OK) {
            DatabaseError error {
                .category = ErrorCategory::Connection,
                .message = "Failed to connect to PostgreSQL",
                .details = raw_handle != nullptr ? PQerrorMessage(raw_handle) : std::string {"PQconnectdb returned null"},
            };
            if (raw_handle != nullptr) {
                PQfinish(raw_handle);
            }
            return make_unexpected(std::move(error));
        }

        state_->handle.reset(raw_handle);
        state_->autocommit_enabled = true;
        state_->in_transaction = false;
        config_ = config;
    }
    logger_->info("Connected to PostgreSQL {}:{}", config.host, config.port);

    if (config.autocommit) {
        return {};
    }

    return set_autocommit(false);
}

void PostgresDatabase::close() noexcept {
    std::scoped_lock lock(state_->mutex);
    state_->handle.reset();
    state_->in_transaction = false;
}

bool PostgresDatabase::is_open() const noexcept {
    std::scoped_lock lock(state_->mutex);
    return state_->handle != nullptr;
}

const ConnectionConfig& PostgresDatabase::config() const noexcept {
    return config_;
}

Expected<QueryResult, DatabaseError> PostgresDatabase::execute(
    const QueryRequest& request,
    std::stop_token stop_token) {
    std::scoped_lock lock(state_->mutex);
    if (state_->handle == nullptr) {
        return make_unexpected(DatabaseError {
            .category = ErrorCategory::Connection,
            .message = "PostgreSQL connection is not open",
        });
    }

    const auto started_at = std::chrono::steady_clock::now();
    QueryResult result;
    const std::string sql = normalize_statement_text(request.sql);

    if (request.explain) {
        const std::string explain_sql = "EXPLAIN (FORMAT JSON) " + sql;
        CancellationBridge cancellation(state_->handle.get(), stop_token);
        PgResult explain_result(PQexec(state_->handle.get(), explain_sql.c_str()), &PQclear);
        if (explain_result == nullptr) {
            return make_unexpected(DatabaseError {
                .category = ErrorCategory::Query,
                .message = "Failed to execute EXPLAIN",
                .details = PQerrorMessage(state_->handle.get()),
            });
        }
        if (PQresultStatus(explain_result.get()) != PGRES_TUPLES_OK) {
            std::optional<TextLocation> location;
            if (const char* position = PQresultErrorField(explain_result.get(), PG_DIAG_STATEMENT_POSITION);
                position != nullptr) {
                const auto pos = static_cast<std::size_t>(std::max(1, std::atoi(position))) - 1;
                location = location_from_offset(sql, pos);
            }
            return make_unexpected(postgres_error(
                state_->handle.get(),
                ErrorCategory::Query,
                "EXPLAIN failed",
                explain_result.get(),
                location));
        }

        const auto explain_json = nlohmann::json::parse(PQgetvalue(explain_result.get(), 0, 0));
        result.statement_kind = StatementKind::Explain;
        result.explain_plan = parse_plan_node(explain_json.at(0).at("Plan"));
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_at);
        return result;
    }

    result.statement_kind = classify_statement(sql);
    if (result.statement_kind == StatementKind::Select) {
        auto cursor = PostgresPagedQueryCursor::create(state_, logger_, sql, stop_token);
        if (!cursor) {
            return make_unexpected(cursor.error());
        }
        result.cursor = std::move(*cursor);
        result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_at);
        return result;
    }

    CancellationBridge cancellation(state_->handle.get(), stop_token);
    PgResult command_result(PQexec(state_->handle.get(), sql.c_str()), &PQclear);
    if (command_result == nullptr) {
        return make_unexpected(DatabaseError {
            .category = ErrorCategory::Query,
            .message = "Failed to execute PostgreSQL statement",
            .details = PQerrorMessage(state_->handle.get()),
        });
    }
    if (PQresultStatus(command_result.get()) != PGRES_COMMAND_OK) {
        std::optional<TextLocation> location;
        if (const char* position = PQresultErrorField(command_result.get(), PG_DIAG_STATEMENT_POSITION);
            position != nullptr) {
            const auto pos = static_cast<std::size_t>(std::max(1, std::atoi(position))) - 1;
            location = location_from_offset(sql, pos);
        }
        if (const char* sql_state = PQresultErrorField(command_result.get(), PG_DIAG_SQLSTATE);
            sql_state != nullptr && std::string_view(sql_state) == "57014") {
            return make_unexpected(DatabaseError::cancelled());
        }
        return make_unexpected(postgres_error(
            state_->handle.get(),
            ErrorCategory::Query,
            "Failed to execute PostgreSQL statement",
            command_result.get(),
            location));
    }

    if (const char* tuples = PQcmdTuples(command_result.get()); tuples != nullptr && tuples[0] != '\0') {
        result.affected_rows = static_cast<std::uint64_t>(std::stoull(tuples));
    }
    result.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started_at);
    logger_->info("PostgreSQL query completed in {} ms", result.execution_time.count());
    return result;
}

Expected<void, DatabaseError> PostgresDatabase::cancel_running_query() noexcept {
    PGconn* handle = nullptr;
    {
        std::scoped_lock lock(state_->mutex);
        handle = state_->handle.get();
    }
    if (handle == nullptr) {
        return {};
    }
    if (PGcancel* cancel = PQgetCancel(handle); cancel != nullptr) {
        char error_buffer[256] {};
        if (PQcancel(cancel, error_buffer, static_cast<int>(sizeof(error_buffer))) == 0) {
            PQfreeCancel(cancel);
            return make_unexpected(DatabaseError {
                .category = ErrorCategory::Cancelled,
                .message = "PostgreSQL cancellation failed",
                .details = error_buffer,
            });
        }
        PQfreeCancel(cancel);
    }
    return {};
}

Expected<void, DatabaseError> PostgresDatabase::set_autocommit(bool enabled) {
    std::scoped_lock lock(state_->mutex);
    if (state_->handle == nullptr) {
        return make_unexpected(DatabaseError {
            .category = ErrorCategory::Connection,
            .message = "PostgreSQL connection is not open",
        });
    }
    if (state_->autocommit_enabled == enabled) {
        return {};
    }

    if (enabled) {
        if (state_->in_transaction) {
            if (auto committed = execute_no_result(state_->handle.get(), "COMMIT"); !committed) {
                return committed;
            }
            state_->in_transaction = false;
        }
        state_->autocommit_enabled = true;
        return {};
    }

    if (auto begun = execute_no_result(state_->handle.get(), "BEGIN"); !begun) {
        return begun;
    }
    state_->autocommit_enabled = false;
    state_->in_transaction = true;
    return {};
}

bool PostgresDatabase::autocommit() const noexcept {
    std::scoped_lock lock(state_->mutex);
    return state_->autocommit_enabled;
}

Expected<void, DatabaseError> PostgresDatabase::begin_transaction() {
    std::scoped_lock lock(state_->mutex);
    if (state_->handle == nullptr) {
        return make_unexpected(DatabaseError {
            .category = ErrorCategory::Connection,
            .message = "PostgreSQL connection is not open",
        });
    }
    if (state_->in_transaction) {
        return {};
    }
    if (auto begun = execute_no_result(state_->handle.get(), "BEGIN"); !begun) {
        return begun;
    }
    state_->in_transaction = true;
    return {};
}

Expected<void, DatabaseError> PostgresDatabase::commit() {
    std::scoped_lock lock(state_->mutex);
    if (state_->handle == nullptr) {
        return make_unexpected(DatabaseError {
            .category = ErrorCategory::Connection,
            .message = "PostgreSQL connection is not open",
        });
    }
    if (!state_->in_transaction) {
        return {};
    }
    if (auto committed = execute_no_result(state_->handle.get(), "COMMIT"); !committed) {
        return committed;
    }
    state_->in_transaction = false;
    if (!state_->autocommit_enabled) {
        if (auto begun = execute_no_result(state_->handle.get(), "BEGIN"); !begun) {
            return begun;
        }
        state_->in_transaction = true;
    }
    return {};
}

Expected<void, DatabaseError> PostgresDatabase::rollback() {
    std::scoped_lock lock(state_->mutex);
    if (state_->handle == nullptr) {
        return make_unexpected(DatabaseError {
            .category = ErrorCategory::Connection,
            .message = "PostgreSQL connection is not open",
        });
    }
    if (!state_->in_transaction) {
        return {};
    }
    if (auto rolled_back = execute_no_result(state_->handle.get(), "ROLLBACK"); !rolled_back) {
        return rolled_back;
    }
    state_->in_transaction = false;
    if (!state_->autocommit_enabled) {
        if (auto begun = execute_no_result(state_->handle.get(), "BEGIN"); !begun) {
            return begun;
        }
        state_->in_transaction = true;
    }
    return {};
}

#else

struct PostgresDatabase::ConnectionState {
    bool autocommit_enabled {true};
};

PostgresDatabase::PostgresDatabase(std::shared_ptr<spdlog::logger> logger)
    : state_(std::make_shared<ConnectionState>()),
      logger_(std::move(logger)) {}

PostgresDatabase::~PostgresDatabase() = default;

Expected<void, DatabaseError> PostgresDatabase::open(const ConnectionConfig& config) {
    config_ = config;
    return make_unexpected(DatabaseError {
        .category = ErrorCategory::Validation,
        .message = "PostgreSQL support is not compiled into this build",
    });
}

void PostgresDatabase::close() noexcept {}

bool PostgresDatabase::is_open() const noexcept {
    return false;
}

const ConnectionConfig& PostgresDatabase::config() const noexcept {
    return config_;
}

Expected<QueryResult, DatabaseError> PostgresDatabase::execute(const QueryRequest&, std::stop_token) {
    return make_unexpected(DatabaseError {
        .category = ErrorCategory::Validation,
        .message = "PostgreSQL support is not compiled into this build",
    });
}

Expected<void, DatabaseError> PostgresDatabase::cancel_running_query() noexcept {
    return make_unexpected(DatabaseError {
        .category = ErrorCategory::Validation,
        .message = "PostgreSQL support is not compiled into this build",
    });
}

Expected<void, DatabaseError> PostgresDatabase::set_autocommit(bool enabled) {
    state_->autocommit_enabled = enabled;
    return make_unexpected(DatabaseError {
        .category = ErrorCategory::Validation,
        .message = "PostgreSQL support is not compiled into this build",
    });
}

bool PostgresDatabase::autocommit() const noexcept {
    return state_->autocommit_enabled;
}

Expected<void, DatabaseError> PostgresDatabase::begin_transaction() {
    return make_unexpected(DatabaseError {
        .category = ErrorCategory::Validation,
        .message = "PostgreSQL support is not compiled into this build",
    });
}

Expected<void, DatabaseError> PostgresDatabase::commit() {
    return make_unexpected(DatabaseError {
        .category = ErrorCategory::Validation,
        .message = "PostgreSQL support is not compiled into this build",
    });
}

Expected<void, DatabaseError> PostgresDatabase::rollback() {
    return make_unexpected(DatabaseError {
        .category = ErrorCategory::Validation,
        .message = "PostgreSQL support is not compiled into this build",
    });
}

#endif

}  // namespace sqlgui::core
