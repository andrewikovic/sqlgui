#pragma once

#include <memory>
#include <stop_token>

#include "core/common/Error.hpp"
#include "core/common/Expected.hpp"
#include "core/models/ConnectionConfig.hpp"
#include "core/models/QueryModels.hpp"

namespace sqlgui::core {

class Database {
public:
    virtual ~Database() = default;

    [[nodiscard]] virtual Expected<void, DatabaseError> open(const ConnectionConfig& config) = 0;
    virtual void close() noexcept = 0;
    [[nodiscard]] virtual bool is_open() const noexcept = 0;
    [[nodiscard]] virtual const ConnectionConfig& config() const noexcept = 0;

    [[nodiscard]] virtual Expected<QueryResult, DatabaseError> execute(
        const QueryRequest& request,
        std::stop_token stop_token) = 0;

    [[nodiscard]] virtual Expected<void, DatabaseError> cancel_running_query() noexcept = 0;
    [[nodiscard]] virtual Expected<void, DatabaseError> set_autocommit(bool enabled) = 0;
    [[nodiscard]] virtual bool autocommit() const noexcept = 0;
    [[nodiscard]] virtual Expected<void, DatabaseError> begin_transaction() = 0;
    [[nodiscard]] virtual Expected<void, DatabaseError> commit() = 0;
    [[nodiscard]] virtual Expected<void, DatabaseError> rollback() = 0;
};

using DatabasePtr = std::shared_ptr<Database>;

}  // namespace sqlgui::core
