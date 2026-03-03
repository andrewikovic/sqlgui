#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "core/common/Error.hpp"
#include "core/common/Expected.hpp"
#include "core/common/ThreadSafeQueue.hpp"
#include "core/database/Database.hpp"
#include "core/models/QueryModels.hpp"

namespace spdlog {
class logger;
}

namespace sqlgui::core {

using QueryOutcome = Expected<QueryResult, DatabaseError>;
using QueryId = std::uint64_t;

struct QueryHandle {
    QueryId id {0};
    std::future<QueryOutcome> future;
};

class QueryExecutionService {
public:
    explicit QueryExecutionService(std::shared_ptr<spdlog::logger> logger);
    ~QueryExecutionService();

    QueryExecutionService(const QueryExecutionService&) = delete;
    QueryExecutionService& operator=(const QueryExecutionService&) = delete;

    [[nodiscard]] QueryHandle submit(std::shared_ptr<Database> database, QueryRequest request);
    [[nodiscard]] bool cancel(QueryId query_id);

private:
    struct TaskControl {
        QueryId id {0};
        std::shared_ptr<Database> database;
        std::stop_source stop_source;
    };

    struct QueryTask {
        QueryId id {0};
        std::shared_ptr<Database> database;
        QueryRequest request;
        std::promise<QueryOutcome> promise;
        std::shared_ptr<TaskControl> control;
    };

    void worker_loop(std::stop_token stop_token);

    std::shared_ptr<spdlog::logger> logger_;
    std::atomic<QueryId> next_id_ {1};
    ThreadSafeQueue<QueryTask> queue_;
    std::jthread worker_;
    std::mutex mutex_;
    std::unordered_map<QueryId, std::shared_ptr<TaskControl>> tasks_;
    std::weak_ptr<TaskControl> running_;
};

}  // namespace sqlgui::core
