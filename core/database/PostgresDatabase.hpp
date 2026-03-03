#pragma once

#include <memory>

#include "core/database/Database.hpp"

namespace spdlog {
class logger;
}

namespace sqlgui::core {

class PostgresDatabase final : public Database {
public:
    struct ConnectionState;

    explicit PostgresDatabase(std::shared_ptr<spdlog::logger> logger);
    ~PostgresDatabase() override;

    [[nodiscard]] Expected<void, DatabaseError> open(const ConnectionConfig& config) override;
    void close() noexcept override;
    [[nodiscard]] bool is_open() const noexcept override;
    [[nodiscard]] const ConnectionConfig& config() const noexcept override;
    [[nodiscard]] Expected<QueryResult, DatabaseError> execute(
        const QueryRequest& request,
        std::stop_token stop_token) override;
    [[nodiscard]] Expected<void, DatabaseError> cancel_running_query() noexcept override;
    [[nodiscard]] Expected<void, DatabaseError> set_autocommit(bool enabled) override;
    [[nodiscard]] bool autocommit() const noexcept override;
    [[nodiscard]] Expected<void, DatabaseError> begin_transaction() override;
    [[nodiscard]] Expected<void, DatabaseError> commit() override;
    [[nodiscard]] Expected<void, DatabaseError> rollback() override;

private:
    std::shared_ptr<ConnectionState> state_;
    ConnectionConfig config_;
    std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace sqlgui::core
