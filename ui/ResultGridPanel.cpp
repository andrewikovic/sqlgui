#include "ui/ResultGridPanel.hpp"

#include <algorithm>
#include <limits>
#include <string>

#include <imgui.h>

namespace sqlgui::ui {

void ResultGridPanel::clear() {
    cursor_.reset();
    pages_.clear();
    sort_.reset();
    page_error_.reset();
    execution_time_ = std::chrono::milliseconds {0};
    affected_rows_ = 0;
    touch_counter_ = 0;
}

void ResultGridPanel::set_result(
    std::unique_ptr<sqlgui::core::QueryCursor> cursor,
    std::chrono::milliseconds execution_time,
    std::uint64_t affected_rows) {
    cursor_ = std::move(cursor);
    pages_.clear();
    sort_.reset();
    page_error_.reset();
    execution_time_ = execution_time;
    affected_rows_ = affected_rows;
    touch_counter_ = 0;
}

void ResultGridPanel::render(ImFont* mono_font) {
    if (!cursor_) {
        ImGui::Text("Execution time: %lld ms", static_cast<long long>(execution_time_.count()));
        ImGui::Text("Affected rows: %llu", static_cast<unsigned long long>(affected_rows_));
        ImGui::TextDisabled("Run a SELECT query to populate the result grid.");
        return;
    }

    const auto row_count = cursor_->total_row_count().value_or(0);
    const auto& columns = cursor_->columns();
    ImGui::Text(
        "Rows: %llu    Execution time: %lld ms",
        static_cast<unsigned long long>(row_count),
        static_cast<long long>(execution_time_.count()));

    ImGuiTableFlags table_flags = ImGuiTableFlags_Borders
        | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_ScrollX
        | ImGuiTableFlags_ScrollY
        | ImGuiTableFlags_Sortable;

    if (mono_font != nullptr) {
        ImGui::PushFont(mono_font);
    }
    if (ImGui::BeginTable("result_grid", static_cast<int>(columns.size()), table_flags, ImVec2(-FLT_MIN, -FLT_MIN))) {
        for (std::size_t index = 0; index < columns.size(); ++index) {
            ImGui::TableSetupColumn(
                columns[index].name.c_str(),
                ImGuiTableColumnFlags_DefaultSort,
                0.0F,
                static_cast<ImGuiID>(index));
        }
        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs();
            specs != nullptr && specs->SpecsCount > 0 && specs->SpecsDirty) {
            const ImGuiTableColumnSortSpecs& sort_spec = specs->Specs[0];
            sort_ = sqlgui::core::SortSpec {
                .column_index = static_cast<std::size_t>(sort_spec.ColumnUserID),
                .direction = sort_spec.SortDirection == ImGuiSortDirection_Descending
                    ? sqlgui::core::SortDirection::Descending
                    : sqlgui::core::SortDirection::Ascending,
            };
            pages_.clear();
            page_error_.reset();
            specs->SpecsDirty = false;
        }

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(std::min<std::uint64_t>(
            row_count,
            static_cast<std::uint64_t>(std::numeric_limits<int>::max()))));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const auto* result_row = row_at(static_cast<std::size_t>(row));
                if (result_row == nullptr) {
                    continue;
                }
                ImGui::TableNextRow();
                for (std::size_t column = 0; column < result_row->size(); ++column) {
                    ImGui::TableSetColumnIndex(static_cast<int>(column));
                    const auto& cell = (*result_row)[column];
                    if (cell.is_null) {
                        ImGui::TextDisabled("NULL");
                    } else {
                        ImGui::TextUnformatted(cell.text.c_str());
                    }
                }
            }
        }
        ImGui::EndTable();
    }
    if (mono_font != nullptr) {
        ImGui::PopFont();
    }

    if (page_error_.has_value()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.92F, 0.24F, 0.19F, 1.0F), "%s", page_error_->message.c_str());
    }
}

bool ResultGridPanel::ensure_page(std::size_t page_index) {
    if (pages_.contains(page_index)) {
        pages_.at(page_index).touch = ++touch_counter_;
        return true;
    }

    const auto page = cursor_->fetch_page(page_index * page_size_, page_size_, sort_, {});
    if (!page) {
        page_error_ = page.error();
        return false;
    }

    pages_[page_index] = CachedPage {.page = *page, .touch = ++touch_counter_};
    trim_cache();
    return true;
}

const sqlgui::core::ResultRow* ResultGridPanel::row_at(std::size_t row_index) {
    const std::size_t page_index = row_index / page_size_;
    const std::size_t page_offset = row_index % page_size_;
    if (!ensure_page(page_index)) {
        return nullptr;
    }

    auto& cached_page = pages_.at(page_index);
    if (page_offset >= cached_page.page.rows.size()) {
        return nullptr;
    }
    return &cached_page.page.rows[page_offset];
}

void ResultGridPanel::trim_cache() {
    constexpr std::size_t max_cached_pages = 8;
    if (pages_.size() <= max_cached_pages) {
        return;
    }

    auto oldest = pages_.begin();
    for (auto it = pages_.begin(); it != pages_.end(); ++it) {
        if (it->second.touch < oldest->second.touch) {
            oldest = it;
        }
    }
    pages_.erase(oldest);
}

}  // namespace sqlgui::ui
