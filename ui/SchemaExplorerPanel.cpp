#include "ui/SchemaExplorerPanel.hpp"

#include <imgui.h>

#include "core/schema/SchemaProvider.hpp"

namespace sqlgui::ui {

namespace {

void render_table_metadata(const sqlgui::core::TableMetadata& metadata) {
    if (ImGui::TreeNodeEx("Columns", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& column : metadata.columns) {
            ImGui::BulletText(
                "%s : %s%s",
                column.name.c_str(),
                column.type.c_str(),
                column.primary_key ? " [PK]" : "");
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Indexes", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& index : metadata.indexes) {
            const std::string joined = index.columns.empty()
                ? std::string {}
                : (std::string(" (") + [&index] {
                      std::string text;
                      for (std::size_t i = 0; i < index.columns.size(); ++i) {
                          if (i > 0) {
                              text += ", ";
                          }
                          text += index.columns[i];
                      }
                      return text;
                  }() + ")");
            ImGui::BulletText(
                "%s%s%s",
                index.name.c_str(),
                index.unique ? " [unique]" : "",
                joined.c_str());
        }
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Foreign Keys", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& foreign_key : metadata.foreign_keys) {
            ImGui::BulletText(
                "%s: %s -> %s.%s(%s)",
                foreign_key.name.c_str(),
                foreign_key.from_column.c_str(),
                foreign_key.target_schema.empty() ? "<default>" : foreign_key.target_schema.c_str(),
                foreign_key.target_table.c_str(),
                foreign_key.target_column.c_str());
        }
        ImGui::TreePop();
    }
}

}  // namespace

void SchemaExplorerPanel::reset() noexcept {
    loaded_ = false;
    tables_.clear();
    metadata_.clear();
    error_.reset();
}

void SchemaExplorerPanel::render(sqlgui::core::SchemaProvider* provider) {
    if (provider == nullptr) {
        ImGui::TextDisabled("Connect to a database to browse schema.");
        return;
    }

    if (ImGui::Button("Refresh Schema")) {
        provider->invalidate();
        reset();
    }

    if (!loaded_) {
        const auto tables = provider->list_tables({});
        if (!tables) {
            error_ = tables.error();
        } else {
            tables_ = *tables;
            loaded_ = true;
            error_.reset();
        }
    }

    if (error_.has_value()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.92F, 0.24F, 0.19F, 1.0F), "%s", error_->message.c_str());
        return;
    }

    ImGui::Separator();
    for (const auto& table : tables_) {
        const std::string label = table.schema.empty() ? table.name : (table.schema + "." + table.name);
        const bool open = ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
        if (!open) {
            continue;
        }

        const auto key = cache_key(table);
        if (metadata_.find(key) == metadata_.end()) {
            const auto metadata = provider->load_table_metadata(table, {});
            if (!metadata) {
                error_ = metadata.error();
                ImGui::TreePop();
                break;
            }
            metadata_[key] = *metadata;
        }

        render_table_metadata(metadata_.at(key));
        ImGui::TreePop();
    }
}

std::string SchemaExplorerPanel::cache_key(const sqlgui::core::TableSummary& table) {
    return table.schema + "." + table.name;
}

}  // namespace sqlgui::ui
