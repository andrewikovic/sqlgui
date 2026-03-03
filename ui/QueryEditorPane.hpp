#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "core/common/Error.hpp"

namespace sqlgui::ui {

struct QueryEditorAction {
    enum class Type {
        None,
        RunAll,
        RunSelection,
        ExplainAll,
        ExplainSelection,
    };

    Type type {Type::None};
    std::string sql;
};

class QueryEditorPane {
public:
    QueryEditorPane();

    [[nodiscard]] QueryEditorAction render(
        ImFont* mono_font,
        bool query_running,
        std::chrono::milliseconds last_execution_time,
        const std::optional<sqlgui::core::DatabaseError>& last_error,
        const std::vector<std::string>& history);

    [[nodiscard]] const std::string& text() const noexcept;
    void set_text(std::string text);

private:
    [[nodiscard]] bool has_selection() const noexcept;
    [[nodiscard]] std::string selected_text() const;
    static int input_callback(ImGuiInputTextCallbackData* data);

    std::string buffer_;
    int selection_start_ {0};
    int selection_end_ {0};
};

}  // namespace sqlgui::ui
