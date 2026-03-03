#pragma once

#include <imgui.h>

namespace sqlgui::ui {

struct ThemeResources {
    ImVec4 clear_color;
    ImFont* body_font {nullptr};
    ImFont* mono_font {nullptr};
};

[[nodiscard]] ThemeResources apply_app_theme(ImGuiIO& io, float dpi_scale);

}  // namespace sqlgui::ui
