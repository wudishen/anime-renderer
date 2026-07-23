cmake_minimum_required(VERSION 3.22)
project(AnimeRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC glfw)

add_executable(AnimeRenderer
    src/main.cpp
    src/engine/Window.cpp
    src/rendering/Shader.cpp
    src/rendering/Mesh.cpp
    src/rendering/Grid.cpp
    src/rendering/CameraBackground.cpp
    src/rendering/CameraBackgroundRenderer.cpp
    src/rendering/LoopBlinnCubic.cpp
    src/rendering/LoopBlinnBSpline.cpp
    src/scene/Scene.cpp
    src/scene/Picking.cpp
    src/scene/CameraFactory.cpp
    src/scene/MeshFactory.cpp
    src/scene/CurveFactory.cpp
    src/scene/BSplineFit.cpp
    src/io/FileDialog.cpp
    src/io/ObjLoader.cpp
    src/io/SceneFile.cpp
    src/ui/MainMenu.cpp
    src/ui/ObjectPanel.cpp
    src/ui/ObjectContextMenu.cpp
    src/tools/TransformTool.cpp
    src/tools/CurveEditTool.cpp
    src/tools/CameraEditTool.cpp
    src/glad.c
)

target_link_libraries(AnimeRenderer PRIVATE
    glfw
    glm::glm
    imgui
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(AnimeRenderer PRIVATE comdlg32)
endif()

target_include_directories(AnimeRenderer PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include     # ← Manual GLAD
)

# CMake/VS debugger defaults cwd to build/Debug; shaders use repo-relative paths.
set_property(TARGET AnimeRenderer PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# Copy shaders next to the exe so double-clicking from build/Debug still works.
add_custom_command(TARGET AnimeRenderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:AnimeRenderer>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/src/rendering/shaders/basic.vert
            ${CMAKE_SOURCE_DIR}/src/rendering/shaders/basic.frag
            ${CMAKE_SOURCE_DIR}/src/rendering/shaders/bg.vert
            ${CMAKE_SOURCE_DIR}/src/rendering/shaders/bg.frag
            ${CMAKE_SOURCE_DIR}/src/rendering/shaders/cubic_bezier.vert
            ${CMAKE_SOURCE_DIR}/src/rendering/shaders/cubic_bezier.frag
            $<TARGET_FILE_DIR:AnimeRenderer>/shaders
)