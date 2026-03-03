#include "ui/MainWindow.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <spdlog/logger.h>

#include "core/common/SqlText.hpp"
#include "core/database/DatabaseFactory.hpp"
#include "core/database/Database.hpp"
#include "core/schema/SchemaProvider.hpp"

namespace sqlgui::ui {

MainWindow::MainWindow(std::shared_ptr<spdlog::logger> logger)
    : logger_(std::move(logger)),
      query_executor_(logger_) {}

void MainWindow::render() {
    poll_query();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    render_connection_window();
    render_schema_window();
    render_editor_window();
    render_results_window();
    render_plan_window();
    render_delete_update_warning();
}

void MainWindow::render_connection_window() {
    ImGui::Begin("Connection");

    int database_kind = connection_config_.kind == sqlgui::core::DatabaseKind::SQLite ? 0 : 1;
    const char* kinds[] = {"SQLite", "PostgreSQL"};
    if (ImGui::Combo("Database", &database_kind, kinds, IM_ARRAYSIZE(kinds))) {
        connection_config_.kind = database_kind == 0
            ? sqlgui::core::DatabaseKind::SQLite
            : sqlgui::core::DatabaseKind::PostgreSQL;
    }

    ImGui::InputText("Connection Name", &connection_config_.name);
    if (connection_config_.kind == sqlgui::core::DatabaseKind::SQLite) {
        ImGui::InputText("SQLite Path", &sqlite_path_input_);
    } else {
        ImGui::InputText("Host", &connection_config_.host);
        int port = static_cast<int>(connection_config_.port);
        if (ImGui::InputInt("Port", &port)) {
            connection_config_.port = static_cast<std::uint16_t>(std::clamp(port, 1, 65535));
        }
        ImGui::InputText("Database", &connection_config_.database);
        ImGui::InputText("User", &connection_config_.user);
        ImGui::InputText("Password", &connection_config_.password, ImGuiInputTextFlags_Password);
    }

    if (ImGui::Button(database_ ? "Reconnect" : "Connect")) {
        connect();
    }

    if (database_ != nullptr) {
        ImGui::SameLine();
        bool autocommit = database_->autocommit();
        if (ImGui::Checkbox("Autocommit", &autocommit)) {
            apply_transaction_toggle(autocommit);
        }
        ImGui::SameLine();
        if (ImGui::Button("BEGIN")) {
            if (auto result = database_->begin_transaction(); !result) {
                last_error_ = result.error();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("COMMIT")) {
            if (auto result = database_->commit(); !result) {
                last_error_ = result.error();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("ROLLBACK")) {
            if (auto result = database_->rollback(); !result) {
                last_error_ = result.error();
            }
        }
    }

    if (active_query_.has_value()) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel Running Query")) {
            (void)query_executor_.cancel(active_query_->id);
        }
    }

    if (database_ != nullptr) {
        ImGui::Separator();
        ImGui::Text(
            "Connected to %s (%s)",
            connection_config_.name.c_str(),
            database_kind_label(connection_config_.kind));
    }
    if (last_error_.has_value()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.92F, 0.24F, 0.19F, 1.0F), "%s", last_error_->message.c_str());
    }

    ImGui::End();
}

void MainWindow::render_schema_window() {
    ImGui::Begin("Schema Explorer");
    schema_explorer_.render(schema_provider_.get());
    ImGui::End();
}

void MainWindow::render_editor_window() {
    ImGui::Begin("Query Editor");
    const auto action = editor_.render(
        active_query_.has_value(),
        last_execution_time_,
        last_error_,
        query_history_);
    ImGui::End();

    switch (action.type) {
        case QueryEditorAction::Type::RunAll:
        case QueryEditorAction::Type::RunSelection:
            submit_query(std::move(action.sql), false);
            break;
        case QueryEditorAction::Type::ExplainAll:
        case QueryEditorAction::Type::ExplainSelection:
            submit_query(std::move(action.sql), true);
            break;
        case QueryEditorAction::Type::None:
        default:
            break;
    }
}

void MainWindow::render_results_window() {
    ImGui::Begin("Results");
    result_grid_.render();
    ImGui::End();
}

void MainWindow::render_plan_window() {
    ImGui::Begin("Execution Plan");
    if (!explain_plan_.has_value()) {
        ImGui::TextDisabled("Run EXPLAIN on a PostgreSQL query to render the execution plan.");
    } else {
        render_plan_node(*explain_plan_);
    }
    ImGui::End();
}

void MainWindow::render_delete_update_warning() {
    if (open_dangerous_modal_) {
        ImGui::OpenPopup("Confirm Dangerous Write");
        open_dangerous_modal_ = false;
    }

    if (ImGui::BeginPopupModal("Confirm Dangerous Write", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("This statement looks like an UPDATE or DELETE without a WHERE clause.");
        ImGui::TextWrapped("Execute it anyway?");
        if (ImGui::Button("Execute")) {
            enqueue_query(pending_sql_, pending_explain_);
            pending_sql_.clear();
            pending_explain_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            pending_sql_.clear();
            pending_explain_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void MainWindow::poll_query() {
    if (!active_query_.has_value()) {
        return;
    }

    if (active_query_->future.wait_for(std::chrono::milliseconds {0}) != std::future_status::ready) {
        return;
    }

    auto outcome = active_query_->future.get();
    active_query_.reset();
    if (!outcome) {
        last_error_ = outcome.error();
        explain_plan_.reset();
        result_grid_.clear();
        return;
    }

    last_error_.reset();
    last_execution_time_ = outcome->execution_time;
    explain_plan_ = outcome->explain_plan;
    result_grid_.set_result(
        std::move(outcome->cursor),
        outcome->execution_time,
        outcome->affected_rows);
    logger_->info("Result applied to UI");
}

void MainWindow::connect() {
    if (connection_config_.kind == sqlgui::core::DatabaseKind::SQLite) {
        connection_config_.sqlite_path = sqlite_path_input_;
    }

    auto session = sqlgui::core::DatabaseFactory::create_session(connection_config_, logger_);
    if (!session) {
        last_error_ = session.error();
        return;
    }

    database_ = session->database;
    schema_provider_ = std::move(session->schema_provider);
    schema_explorer_.reset();
    result_grid_.clear();
    explain_plan_.reset();
    last_error_.reset();
    active_query_.reset();
}

void MainWindow::enqueue_query(std::string sql, bool explain) {
    if (database_ == nullptr) {
        last_error_ = sqlgui::core::DatabaseError {
            .category = sqlgui::core::ErrorCategory::Validation,
            .message = "Connect to a database before executing queries",
        };
        return;
    }

    const std::string trimmed = sqlgui::core::trim_sql(sql);
    if (trimmed.empty()) {
        last_error_ = sqlgui::core::DatabaseError::validation("Query text is empty");
        return;
    }

    if (query_history_.empty() || query_history_.front() != trimmed) {
        query_history_.insert(query_history_.begin(), trimmed);
        if (query_history_.size() > 50) {
            query_history_.resize(50);
        }
    }

    result_grid_.clear();
    explain_plan_.reset();
    last_error_.reset();
    active_query_ = query_executor_.submit(
        database_,
        sqlgui::core::QueryRequest {.sql = trimmed, .explain = explain});
}

void MainWindow::submit_query(std::string sql, bool explain) {
    if (!explain && sqlgui::core::is_dangerous_write_without_where(sql)) {
        pending_sql_ = std::move(sql);
        pending_explain_ = false;
        open_dangerous_modal_ = true;
        return;
    }
    enqueue_query(std::move(sql), explain);
}

void MainWindow::apply_transaction_toggle(bool enabled) {
    if (database_ == nullptr) {
        return;
    }
    if (auto result = database_->set_autocommit(enabled); !result) {
        last_error_ = result.error();
    }
}

const char* MainWindow::database_kind_label(sqlgui::core::DatabaseKind kind) {
    return kind == sqlgui::core::DatabaseKind::SQLite ? "SQLite" : "PostgreSQL";
}

void MainWindow::render_plan_node(const sqlgui::core::ExplainNode& node) {
    std::string label = node.node_type;
    if (!node.relation_name.empty()) {
        label += " [" + node.relation_name + "]";
    }

    if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (node.startup_cost.has_value() && node.total_cost.has_value()) {
            ImGui::Text("Cost: %.2f -> %.2f", *node.startup_cost, *node.total_cost);
        }
        if (node.rows.has_value()) {
            ImGui::Text("Estimated rows: %.0f", *node.rows);
        }
        for (const auto& child : node.children) {
            render_plan_node(child);
        }
        ImGui::TreePop();
    }
}

}  // namespace sqlgui::ui
