#include "ui/QueryEditorPane.hpp"

#include <algorithm>
#include <string_view>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace sqlgui::ui {

QueryEditorPane::QueryEditorPane()
    : buffer_("SELECT name\nFROM sqlite_master\nORDER BY name;") {}

QueryEditorAction QueryEditorPane::render(
    ImFont* mono_font,
    bool query_running,
    std::chrono::milliseconds last_execution_time,
    const std::optional<sqlgui::core::DatabaseError>& last_error,
    const std::vector<std::string>& history) {
    QueryEditorAction action;

    if (ImGui::BeginCombo("History", history.empty() ? "No history yet" : "Load from history")) {
        for (const auto& item : history) {
            const bool selected = item == buffer_;
            if (ImGui::Selectable(item.c_str(), selected)) {
                buffer_ = item;
                selection_start_ = 0;
                selection_end_ = 0;
            }
        }
        ImGui::EndCombo();
    }

    if (query_running) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Run")) {
        action = QueryEditorAction {.type = QueryEditorAction::Type::RunAll, .sql = buffer_};
    }
    ImGui::SameLine();
    const bool selection_available = has_selection();
    if (!selection_available) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Run Selection")) {
        action = QueryEditorAction {.type = QueryEditorAction::Type::RunSelection, .sql = selected_text()};
    }
    if (!selection_available) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (ImGui::Button("EXPLAIN")) {
        action = QueryEditorAction {.type = QueryEditorAction::Type::ExplainAll, .sql = buffer_};
    }
    ImGui::SameLine();
    if (!selection_available) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("EXPLAIN Selection")) {
        action = QueryEditorAction {.type = QueryEditorAction::Type::ExplainSelection, .sql = selected_text()};
    }
    if (!selection_available) {
        ImGui::EndDisabled();
    }
    if (query_running) {
        ImGui::EndDisabled();
    }

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackAlways;
    if (mono_font != nullptr) {
        ImGui::PushFont(mono_font);
    }
    ImGui::InputTextMultiline(
        "##query_editor",
        &buffer_,
        ImVec2(-FLT_MIN, -FLT_MIN),
        flags,
        &QueryEditorPane::input_callback,
        this);
    if (mono_font != nullptr) {
        ImGui::PopFont();
    }

    if (last_error.has_value()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.92F, 0.24F, 0.19F, 1.0F), "%s", last_error->message.c_str());
        if (last_error->line.has_value()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(line %d", *last_error->line);
            if (last_error->column.has_value()) {
                ImGui::SameLine();
                ImGui::TextDisabled(", col %d)", *last_error->column);
            } else {
                ImGui::SameLine();
                ImGui::TextDisabled(")");
            }
        }
    } else {
        ImGui::Separator();
        ImGui::Text("Last execution: %lld ms", static_cast<long long>(last_execution_time.count()));
    }

    return action;
}

const std::string& QueryEditorPane::text() const noexcept {
    return buffer_;
}

void QueryEditorPane::set_text(std::string text) {
    buffer_ = std::move(text);
    selection_start_ = 0;
    selection_end_ = 0;
}

bool QueryEditorPane::has_selection() const noexcept {
    return selection_start_ != selection_end_;
}

std::string QueryEditorPane::selected_text() const {
    if (!has_selection()) {
        return {};
    }
    const auto [first, last] = std::minmax(selection_start_, selection_end_);
    const auto safe_first = std::clamp(first, 0, static_cast<int>(buffer_.size()));
    const auto safe_last = std::clamp(last, 0, static_cast<int>(buffer_.size()));
    return buffer_.substr(
        static_cast<std::size_t>(safe_first),
        static_cast<std::size_t>(safe_last - safe_first));
}

int QueryEditorPane::input_callback(ImGuiInputTextCallbackData* data) {
    auto* pane = static_cast<QueryEditorPane*>(data->UserData);
    pane->selection_start_ = data->SelectionStart;
    pane->selection_end_ = data->SelectionEnd;
    return 0;
}

}  // namespace sqlgui::ui
