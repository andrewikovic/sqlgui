cmake_minimum_required(VERSION 3.27)

project(
    sqlgui
    VERSION 0.1.0
    DESCRIPTION "Async SQL client GUI built with Dear ImGui"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

option(SQLGUI_BUILD_POSTGRES "Enable PostgreSQL support when libpq is available" ON)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EXPECTED_BUILD_TESTS OFF CACHE BOOL "" FORCE)

find_package(OpenGL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(PostgreSQL QUIET)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)

FetchContent_Declare(
    tl_expected
    GIT_REPOSITORY https://github.com/TartanLlama/expected.git
    GIT_TAG v1.1.0
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(glfw imgui spdlog tl_expected nlohmann_json)

add_library(
    imgui_docking STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)

target_include_directories(
    imgui_docking
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imgui_SOURCE_DIR}/misc/cpp
)

target_link_libraries(imgui_docking PUBLIC glfw OpenGL::GL)

set(SQLGUI_HAS_POSTGRES 0)
if(SQLGUI_BUILD_POSTGRES AND PostgreSQL_FOUND)
    set(SQLGUI_HAS_POSTGRES 1)
endif()

add_executable(
    sqlgui
    app/main.cpp
    core/common/SqlText.cpp
    core/database/DatabaseFactory.cpp
    core/database/SQLiteDatabase.cpp
    core/database/PostgresDatabase.cpp
    core/schema/SQLiteSchemaProvider.cpp
    core/schema/PostgresSchemaProvider.cpp
    core/async/QueryExecutionService.cpp
    ui/QueryEditorPane.cpp
    ui/SchemaExplorerPanel.cpp
    ui/ResultGridPanel.cpp
    ui/MainWindow.cpp
)

target_include_directories(sqlgui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(
    sqlgui
    PRIVATE
        IMGUI_ENABLE_DOCKING
        SQLGUI_HAS_POSTGRES=${SQLGUI_HAS_POSTGRES}
)

if(APPLE)
    target_compile_definitions(sqlgui PRIVATE GL_SILENCE_DEPRECATION)
endif()

target_link_libraries(
    sqlgui
    PRIVATE
        imgui_docking
        SQLite::SQLite3
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        tl::expected
)

if(SQLGUI_HAS_POSTGRES)
    target_link_libraries(sqlgui PRIVATE PostgreSQL::PostgreSQL)
endif()

if(MSVC)
    target_compile_options(sqlgui PRIVATE /W4 /permissive-)
else()
    target_compile_options(sqlgui PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
