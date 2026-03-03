#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/common/Error.hpp"
#include "core/models/SchemaModels.hpp"

namespace sqlgui::core {
class SchemaProvider;
}

namespace sqlgui::ui {

class SchemaExplorerPanel {
public:
    void reset() noexcept;
    void render(sqlgui::core::SchemaProvider* provider);

private:
    [[nodiscard]] static std::string cache_key(const sqlgui::core::TableSummary& table);

    bool loaded_ {false};
    std::vector<sqlgui::core::TableSummary> tables_;
    std::unordered_map<std::string, sqlgui::core::TableMetadata> metadata_;
    std::optional<sqlgui::core::DatabaseError> error_;
};

}  // namespace sqlgui::ui
