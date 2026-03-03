#pragma once

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "core/async/QueryExecutionService.hpp"
#include "core/common/Error.hpp"
#include "core/models/ConnectionConfig.hpp"
#include "core/models/QueryModels.hpp"
#include "core/schema/SchemaProvider.hpp"
#include "ui/QueryEditorPane.hpp"
#include "ui/ResultGridPanel.hpp"
#include "ui/SchemaExplorerPanel.hpp"

namespace spdlog {
class logger;
}

namespace sqlgui::core {
class Database;
}

namespace sqlgui::ui {

class MainWindow {
public:
    MainWindow(std::shared_ptr<spdlog::logger> logger, ImFont* mono_font);

    void render();

private:
    void render_connection_window();
    void render_schema_window();
    void render_editor_window();
    void render_results_window();
    void render_plan_window();
    void ensure_initial_dock_layout(ImGuiID dockspace_id);
    void render_delete_update_warning();
    void poll_query();
    void connect();
    void enqueue_query(std::string sql, bool explain);
    void submit_query(std::string sql, bool explain);
    void apply_transaction_toggle(bool enabled);
    [[nodiscard]] static const char* database_kind_label(sqlgui::core::DatabaseKind kind);
    static void render_plan_node(const sqlgui::core::ExplainNode& node);

    std::shared_ptr<spdlog::logger> logger_;
    ImFont* mono_font_ {nullptr};
    sqlgui::core::QueryExecutionService query_executor_;
    sqlgui::core::ConnectionConfig connection_config_;
    std::shared_ptr<sqlgui::core::Database> database_;
    std::unique_ptr<sqlgui::core::SchemaProvider> schema_provider_;
    QueryEditorPane editor_;
    SchemaExplorerPanel schema_explorer_;
    ResultGridPanel result_grid_;
    std::vector<std::string> query_history_;
    std::optional<sqlgui::core::DatabaseError> last_error_;
    std::optional<sqlgui::core::ExplainNode> explain_plan_;
    std::optional<sqlgui::core::QueryHandle> active_query_;
    std::chrono::milliseconds last_execution_time_ {0};
    std::string sqlite_path_input_ {"data/comprehensive_test.db"};
    std::string pending_sql_;
    bool pending_explain_ {false};
    bool dock_layout_initialized_ {false};
    bool open_dangerous_modal_ {false};
};

}  // namespace sqlgui::ui
