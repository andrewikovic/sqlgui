#include "ui/AppTheme.hpp"

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <string_view>

namespace sqlgui::ui {

namespace {

[[nodiscard]] ImFont* load_first_available_font(
    ImGuiIO& io,
    float size_pixels,
    std::initializer_list<std::string_view> candidates,
    float rasterizer_multiply) {
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 2;
    config.RasterizerMultiply = rasterizer_multiply;

    for (const auto candidate : candidates) {
        const std::filesystem::path path(candidate);
        if (!std::filesystem::exists(path)) {
            continue;
        }

        if (ImFont* font = io.Fonts->AddFontFromFileTTF(path.string().c_str(), size_pixels, &config);
            font != nullptr) {
            return font;
        }
    }

    return nullptr;
}

[[nodiscard]] ImVec4 rgba(float red, float green, float blue, float alpha = 1.0F) {
    return ImVec4(red, green, blue, alpha);
}

void apply_palette(ImGuiStyle& style) {
    const ImVec4 paper = rgba(0.95F, 0.94F, 0.90F);
    const ImVec4 paper_soft = rgba(0.91F, 0.90F, 0.85F);
    const ImVec4 surface = rgba(0.99F, 0.98F, 0.95F);
    const ImVec4 surface_alt = rgba(0.94F, 0.93F, 0.89F);
    const ImVec4 line = rgba(0.78F, 0.76F, 0.70F);
    const ImVec4 ink = rgba(0.15F, 0.17F, 0.19F);
    const ImVec4 ink_soft = rgba(0.38F, 0.40F, 0.41F);
    const ImVec4 accent = rgba(0.10F, 0.50F, 0.49F);
    const ImVec4 accent_hover = rgba(0.14F, 0.58F, 0.56F);
    const ImVec4 accent_active = rgba(0.08F, 0.42F, 0.41F);
    const ImVec4 selection = rgba(0.88F, 0.64F, 0.31F, 0.28F);

    style.WindowPadding = ImVec2(14.0F, 12.0F);
    style.FramePadding = ImVec2(10.0F, 7.0F);
    style.CellPadding = ImVec2(10.0F, 6.0F);
    style.ItemSpacing = ImVec2(10.0F, 8.0F);
    style.ItemInnerSpacing = ImVec2(7.0F, 6.0F);
    style.TouchExtraPadding = ImVec2(0.0F, 0.0F);
    style.IndentSpacing = 18.0F;
    style.ScrollbarSize = 14.0F;
    style.GrabMinSize = 10.0F;

    style.WindowBorderSize = 1.0F;
    style.ChildBorderSize = 1.0F;
    style.PopupBorderSize = 1.0F;
    style.FrameBorderSize = 0.0F;
    style.TabBorderSize = 0.0F;

    style.WindowRounding = 14.0F;
    style.ChildRounding = 12.0F;
    style.FrameRounding = 10.0F;
    style.PopupRounding = 12.0F;
    style.ScrollbarRounding = 999.0F;
    style.GrabRounding = 999.0F;
    style.TabRounding = 10.0F;

    style.WindowTitleAlign = ImVec2(0.03F, 0.5F);
    style.ButtonTextAlign = ImVec2(0.5F, 0.5F);
    style.SelectableTextAlign = ImVec2(0.0F, 0.5F);

    auto& colors = style.Colors;
    colors[ImGuiCol_Text] = ink;
    colors[ImGuiCol_TextDisabled] = ink_soft;
    colors[ImGuiCol_WindowBg] = paper;
    colors[ImGuiCol_ChildBg] = rgba(surface.x, surface.y, surface.z, 0.88F);
    colors[ImGuiCol_PopupBg] = surface;
    colors[ImGuiCol_Border] = line;
    colors[ImGuiCol_BorderShadow] = rgba(0.0F, 0.0F, 0.0F, 0.0F);
    colors[ImGuiCol_FrameBg] = surface;
    colors[ImGuiCol_FrameBgHovered] = rgba(accent_hover.x, accent_hover.y, accent_hover.z, 0.14F);
    colors[ImGuiCol_FrameBgActive] = rgba(accent.x, accent.y, accent.z, 0.18F);
    colors[ImGuiCol_TitleBg] = paper_soft;
    colors[ImGuiCol_TitleBgActive] = surface_alt;
    colors[ImGuiCol_TitleBgCollapsed] = paper_soft;
    colors[ImGuiCol_MenuBarBg] = surface;
    colors[ImGuiCol_ScrollbarBg] = rgba(surface_alt.x, surface_alt.y, surface_alt.z, 0.75F);
    colors[ImGuiCol_ScrollbarGrab] = rgba(0.52F, 0.56F, 0.54F, 0.75F);
    colors[ImGuiCol_ScrollbarGrabHovered] = rgba(0.42F, 0.47F, 0.45F, 0.86F);
    colors[ImGuiCol_ScrollbarGrabActive] = rgba(0.33F, 0.37F, 0.36F, 0.94F);
    colors[ImGuiCol_CheckMark] = accent_active;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = accent_active;
    colors[ImGuiCol_Button] = accent;
    colors[ImGuiCol_ButtonHovered] = accent_hover;
    colors[ImGuiCol_ButtonActive] = accent_active;
    colors[ImGuiCol_Header] = rgba(accent.x, accent.y, accent.z, 0.12F);
    colors[ImGuiCol_HeaderHovered] = rgba(accent_hover.x, accent_hover.y, accent_hover.z, 0.20F);
    colors[ImGuiCol_HeaderActive] = rgba(accent.x, accent.y, accent.z, 0.28F);
    colors[ImGuiCol_Separator] = line;
    colors[ImGuiCol_SeparatorHovered] = accent_hover;
    colors[ImGuiCol_SeparatorActive] = accent_active;
    colors[ImGuiCol_ResizeGrip] = rgba(accent.x, accent.y, accent.z, 0.25F);
    colors[ImGuiCol_ResizeGripHovered] = rgba(accent_hover.x, accent_hover.y, accent_hover.z, 0.54F);
    colors[ImGuiCol_ResizeGripActive] = rgba(accent_active.x, accent_active.y, accent_active.z, 0.82F);
    colors[ImGuiCol_Tab] = rgba(surface_alt.x, surface_alt.y, surface_alt.z, 0.96F);
    colors[ImGuiCol_TabHovered] = rgba(accent_hover.x, accent_hover.y, accent_hover.z, 0.24F);
    colors[ImGuiCol_TabActive] = rgba(accent.x, accent.y, accent.z, 0.18F);
    colors[ImGuiCol_TabUnfocused] = rgba(surface.x, surface.y, surface.z, 0.94F);
    colors[ImGuiCol_TabUnfocusedActive] = rgba(accent.x, accent.y, accent.z, 0.12F);
    colors[ImGuiCol_DockingPreview] = rgba(accent.x, accent.y, accent.z, 0.30F);
    colors[ImGuiCol_DockingEmptyBg] = rgba(0.90F, 0.89F, 0.84F, 1.0F);
    colors[ImGuiCol_PlotLines] = accent;
    colors[ImGuiCol_PlotLinesHovered] = accent_hover;
    colors[ImGuiCol_PlotHistogram] = rgba(0.82F, 0.58F, 0.22F, 1.0F);
    colors[ImGuiCol_PlotHistogramHovered] = rgba(0.91F, 0.67F, 0.28F, 1.0F);
    colors[ImGuiCol_TableHeaderBg] = rgba(surface_alt.x, surface_alt.y, surface_alt.z, 0.98F);
    colors[ImGuiCol_TableBorderStrong] = rgba(0.71F, 0.69F, 0.63F, 1.0F);
    colors[ImGuiCol_TableBorderLight] = rgba(0.83F, 0.81F, 0.75F, 1.0F);
    colors[ImGuiCol_TableRowBg] = rgba(surface.x, surface.y, surface.z, 0.90F);
    colors[ImGuiCol_TableRowBgAlt] = rgba(surface_alt.x, surface_alt.y, surface_alt.z, 0.65F);
    colors[ImGuiCol_TextSelectedBg] = selection;
    colors[ImGuiCol_DragDropTarget] = rgba(0.86F, 0.56F, 0.18F, 0.90F);
    colors[ImGuiCol_NavHighlight] = rgba(accent.x, accent.y, accent.z, 0.68F);
    colors[ImGuiCol_NavWindowingHighlight] = rgba(0.15F, 0.17F, 0.19F, 0.72F);
}

}  // namespace

ThemeResources apply_app_theme(ImGuiIO& io, float dpi_scale) {
    io.Fonts->Clear();

    // Sanitize DPI scale to prevent invalid or zero values.
    // If dpi_scale <= 0, default to 1.0 (no scaling).
    const float safe_scale = dpi_scale > 0.0F ? dpi_scale : 1.0F;


    // Apply a very subtle font scaling when DPI > 1.0.
    // Only 8% of the extra DPI scaling is applied, capped at +8% total.
    // This keeps fonts from becoming excessively large on high-DPI displays.
    const float subtle_scale = safe_scale > 1.0F
        ? std::min(1.08F, 1.0F + ((safe_scale - 1.0F) * 0.08F))
        : 1.0F;

    // Compute final font sizes using the restrained scaling factor.
    const float body_size = 16.0F * subtle_scale;
    const float mono_size = 14.5F * subtle_scale;

    ImFont* body_font = load_first_available_font(
        io,
        body_size,
        {
            "/System/Library/Fonts/SFNS.ttf",
            "/System/Library/Fonts/Avenir Next.ttc",
            "/System/Library/Fonts/Avenir.ttc",
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "C:/Windows/Fonts/segoeui.ttf",
            "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        },
        1.1F);

    ImFont* mono_font = load_first_available_font(
        io,
        mono_size,
        {
            "assets/fonts/IBMPlexMono-Regular.ttf",
            "../assets/fonts/IBMPlexMono-Regular.ttf",
            "../../assets/fonts/IBMPlexMono-Regular.ttf",
            "/Users/ikovic/Library/Fonts/IBMPlexMono-Regular.otf",
            "/System/Library/Fonts/SFNSMono.ttf",
            "/System/Library/Fonts/Menlo.ttc",
            "/System/Library/Fonts/Monaco.ttf",
            "C:/Windows/Fonts/consola.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf",
        },
        1.0F);

    if (body_font == nullptr) {
        body_font = io.Fonts->AddFontDefault();
    }
    if (mono_font == nullptr) {
        mono_font = body_font;
    }

    io.FontDefault = body_font;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle {};
    apply_palette(style);
    style.ScaleAllSizes(subtle_scale);

    return ThemeResources {
        .clear_color = rgba(0.93F, 0.92F, 0.88F, 1.0F),
        .body_font = body_font,
        .mono_font = mono_font,
    };
}

}  // namespace sqlgui::ui
