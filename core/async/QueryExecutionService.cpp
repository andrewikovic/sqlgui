#include "core/async/QueryExecutionService.hpp"

#include <exception>
#include <utility>

#include <spdlog/logger.h>

namespace sqlgui::core {

QueryExecutionService::QueryExecutionService(std::shared_ptr<spdlog::logger> logger)
    : logger_(std::move(logger)),
      worker_([this](std::stop_token stop_token) { worker_loop(stop_token); }) {}

QueryExecutionService::~QueryExecutionService() {
    worker_.request_stop();
}

QueryHandle QueryExecutionService::submit(std::shared_ptr<Database> database, QueryRequest request) {
    QueryTask task;
    task.id = next_id_.fetch_add(1, std::memory_order_relaxed);
    task.database = std::move(database);
    task.request = std::move(request);
    task.control = std::make_shared<TaskControl>();
    task.control->id = task.id;
    task.control->database = task.database;

    auto future = task.promise.get_future();
    const QueryId query_id = task.id;
    {
        std::scoped_lock lock(mutex_);
        tasks_[task.id] = task.control;
    }
    queue_.push(std::move(task));

    logger_->info("Queued query {}", query_id);
    return QueryHandle {.id = query_id, .future = std::move(future)};
}

bool QueryExecutionService::cancel(QueryId query_id) {
    std::shared_ptr<TaskControl> control;
    {
        std::scoped_lock lock(mutex_);
        const auto it = tasks_.find(query_id);
        if (it == tasks_.end()) {
            return false;
        }
        control = it->second;
    }

    control->stop_source.request_stop();
    const auto cancel_result = control->database->cancel_running_query();
    if (!cancel_result) {
        logger_->warn("Cancellation request for query {} returned: {}", query_id, cancel_result.error().message);
    }
    logger_->info("Cancellation requested for query {}", query_id);
    return true;
}

void QueryExecutionService::worker_loop(std::stop_token stop_token) {
    while (auto queued_task = queue_.wait_pop(stop_token)) {
        auto task = std::move(*queued_task);
        {
            std::scoped_lock lock(mutex_);
            running_ = task.control;
        }

        try {
            QueryOutcome outcome = task.control->stop_source.stop_requested()
                ? QueryOutcome {make_unexpected(DatabaseError::cancelled())}
                : task.database->execute(task.request, task.control->stop_source.get_token());
            task.promise.set_value(std::move(outcome));
        } catch (const std::exception& exception) {
            task.promise.set_value(QueryOutcome {make_unexpected(DatabaseError {
                .category = ErrorCategory::Internal,
                .message = "Unhandled exception in query worker",
                .details = exception.what(),
            })});
        }

        {
            std::scoped_lock lock(mutex_);
            tasks_.erase(task.id);
            if (const auto running = running_.lock(); running && running->id == task.id) {
                running_.reset();
            }
        }
    }
}

}  // namespace sqlgui::core
