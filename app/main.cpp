#include <cstdio>
#include <memory>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "core/common/Logger.hpp"
#include "ui/MainWindow.hpp"

namespace {

[[nodiscard]] bool init_glfw_window(GLFWwindow** window, const char** glsl_version) {
    if (!glfwInit()) {
        return false;
    }

#if defined(__APPLE__)
    *glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    *window = glfwCreateWindow(1600, 960, "sqlgui", nullptr, nullptr);
    if (*window == nullptr) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(*window);
    glfwSwapInterval(1);
    return true;
}

}  // namespace

int main() {
    GLFWwindow* window = nullptr;
    const char* glsl_version = nullptr;
    if (!init_glfw_window(&window, &glsl_version)) {
        std::fprintf(stderr, "Failed to initialize GLFW/OpenGL window\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsLight();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    sqlgui::core::AppLogger logger;
    sqlgui::ui::MainWindow main_window(logger.shared());

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        main_window.render();

        ImGui::Render();
        int display_width = 0;
        int display_height = 0;
        glfwGetFramebufferSize(window, &display_width, &display_height);
        glViewport(0, 0, display_width, display_height);
        glClearColor(0.94F, 0.94F, 0.93F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
