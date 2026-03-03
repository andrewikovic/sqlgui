#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/common/Error.hpp"
#include "core/models/QueryModels.hpp"

namespace sqlgui::ui {

class ResultGridPanel {
public:
    void clear();
    void set_result(
        std::unique_ptr<sqlgui::core::QueryCursor> cursor,
        std::chrono::milliseconds execution_time,
        std::uint64_t affected_rows);
    void render();

private:
    struct CachedPage {
        sqlgui::core::ResultPage page;
        std::uint64_t touch {0};
    };

    [[nodiscard]] bool ensure_page(std::size_t page_index);
    [[nodiscard]] const sqlgui::core::ResultRow* row_at(std::size_t row_index);
    void trim_cache();

    std::unique_ptr<sqlgui::core::QueryCursor> cursor_;
    std::unordered_map<std::size_t, CachedPage> pages_;
    std::optional<sqlgui::core::SortSpec> sort_;
    std::optional<sqlgui::core::DatabaseError> page_error_;
    std::chrono::milliseconds execution_time_ {0};
    std::uint64_t affected_rows_ {0};
    std::size_t page_size_ {250};
    std::uint64_t touch_counter_ {0};
};

}  // namespace sqlgui::ui
