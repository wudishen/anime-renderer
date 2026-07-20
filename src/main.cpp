#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include "engine/Window.h"
#include "rendering/Shader.h"
#include "rendering/Grid.h"
#include "rendering/CameraBackgroundRenderer.h"
#include "scene/Scene.h"
#include "scene/CameraFactory.h"
#include "scene/Picking.h"
#include "tools/TransformTool.h"
#include "ui/MainMenu.h"
#include "ui/ObjectPanel.h"
#include "ui/ObjectContextMenu.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace fs = std::filesystem;

static Scene* g_Scene = nullptr;

static fs::path ExecutableDir() {
#ifdef _WIN32
    char buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return fs::current_path();
    }
    return fs::path(buffer).parent_path();
#else
    return fs::current_path();
#endif
}

static std::string ResolveShaderPath(const char* fileName) {
    const fs::path exeDir = ExecutableDir();
    const fs::path candidates[] = {
        exeDir / "shaders" / fileName,
        exeDir / ".." / ".." / "src" / "rendering" / "shaders" / fileName,
        fs::current_path() / "src" / "rendering" / "shaders" / fileName,
        fs::current_path() / "shaders" / fileName,
    };

    for (const fs::path& candidate : candidates) {
        std::error_code ec;
        if (fs::exists(candidate, ec)) {
            return candidate.lexically_normal().string();
        }
    }

    std::cerr << "Could not find shader: " << fileName << std::endl;
    return fileName;
}

// Cameras are saved viewports — there is always one active view camera.
static void EnsureActiveViewCamera(Scene& scene) {
    if (scene.GetActiveViewCamera()) {
        return;
    }

    for (const SceneObject& object : scene.GetObjects()) {
        if (object.IsCamera()) {
            scene.SetActiveViewCameraId(object.id);
            return;
        }
    }

    SceneObject& camera = scene.AddObject(CameraFactory::Create(CameraFactory::Preset::Free, 3.0f));
    scene.SetActiveViewCameraId(camera.id);
}

static ViewBasis GetActiveViewBasis(Scene& scene) {
    EnsureActiveViewCamera(scene);
    const SceneObject* viewCamera = scene.GetActiveViewCamera();
    ViewBasis basis;
    basis.right = viewCamera->GetRight();
    basis.up = viewCamera->GetUp();
    basis.distance = std::max(glm::length(viewCamera->GetTranslation()), 1.0f);
    return basis;
}

static glm::mat4 GetActiveViewMatrix(Scene& scene) {
    EnsureActiveViewCamera(scene);
    return scene.GetActiveViewCamera()->GetViewMatrix();
}

static glm::mat4 GetActiveProjection(Scene& scene, float aspect) {
    EnsureActiveViewCamera(scene);
    return glm::perspective(
        glm::radians(scene.GetActiveViewCamera()->cameraFovDegrees),
        aspect,
        0.1f,
        100.0f);
}

static glm::vec3 GetActiveViewPosition(Scene& scene) {
    EnsureActiveViewCamera(scene);
    return scene.GetActiveViewCamera()->GetTranslation();
}

static void NavigateActiveView(Scene& scene, float dx, float dy, bool pan, bool orbit) {
    EnsureActiveViewCamera(scene);
    SceneObject* viewCamera = scene.GetActiveViewCamera();

    const float distance = std::max(glm::length(viewCamera->GetTranslation()), 1.0f);
    if (pan) {
        const float sensitivity = 0.0025f * distance;
        viewCamera->AddTranslation(
            -viewCamera->GetRight() * (dx * sensitivity) +
            viewCamera->GetUp() * (dy * sensitivity));
        return;
    }

    if (orbit) {
        const float sensitivity = 0.25f;
        const glm::vec3 pos = viewCamera->GetTranslation();
        const glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), -pos);
        const glm::mat4 fromOrigin = glm::translate(glm::mat4(1.0f), pos);
        const glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), glm::radians(-dx * sensitivity),
                                          glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), glm::radians(-dy * sensitivity),
                                            viewCamera->GetRight());
        viewCamera->transform = fromOrigin * yaw * pitch * toOrigin * viewCamera->transform;
    }
}

static void ScrollCallback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset) {
    if (MainMenu::WantCaptureMouse() || !g_Scene) {
        return;
    }

    EnsureActiveViewCamera(*g_Scene);
    if (SceneObject* viewCamera = g_Scene->GetActiveViewCamera()) {
        viewCamera->AddTranslation(viewCamera->GetForward() * static_cast<float>(yoffset) * 0.25f);
    }
}

static void HandleObjectAction(
    const ObjectContextMenu::ActionRequest& request,
    Scene& scene,
    TransformTool& transformTool) {
    if (request.action == ObjectContextMenu::Action::None) {
        return;
    }

    if (transformTool.IsActive()) {
        transformTool.Cancel(scene);
    }

    switch (request.action) {
    case ObjectContextMenu::Action::Delete:
        scene.RemoveObject(request.objectId);
        EnsureActiveViewCamera(scene);
        break;
    case ObjectContextMenu::Action::Translate:
        transformTool.BeginTranslate(request.objectId, scene);
        break;
    case ObjectContextMenu::Action::Scale:
        std::cout << "Scale tool is not implemented yet." << std::endl;
        break;
    case ObjectContextMenu::Action::Rotate:
        transformTool.BeginRotate(request.objectId, scene);
        break;
    case ObjectContextMenu::Action::GoToView:
        if (const SceneObject* camera = scene.FindById(request.objectId); camera && camera->IsCamera()) {
            scene.SetActiveViewCameraId(request.objectId);
            scene.SetSelectedId(request.objectId);
        }
        break;
    case ObjectContextMenu::Action::None:
    default:
        break;
    }
}

#ifdef _WIN32
static void AttachConsoleIfNeeded() {
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        FILE* unused = nullptr;
        freopen_s(&unused, "CONOUT$", "w", stdout);
        freopen_s(&unused, "CONOUT$", "w", stderr);
    }
}
#endif

int main() {
#ifdef _WIN32
    AttachConsoleIfNeeded();
#endif

    Window window("Anime Renderer", 1280, 720);

    if (!window.getGLFWwindow()) {
        return 1;
    }

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return 1;
    }

    glEnable(GL_DEPTH_TEST);

    GLFWwindow* glfwWindow = window.getGLFWwindow();

    // Register before ImGui so its backend chains this callback.
    glfwSetScrollCallback(glfwWindow, ScrollCallback);
    MainMenu::Init(glfwWindow);

    const std::string vertPath = ResolveShaderPath("basic.vert");
    const std::string fragPath = ResolveShaderPath("basic.frag");
    std::cout << "Vertex shader: " << vertPath << std::endl;
    std::cout << "Fragment shader: " << fragPath << std::endl;

    Shader shader(vertPath, fragPath);
    const std::string bgVertPath = ResolveShaderPath("bg.vert");
    const std::string bgFragPath = ResolveShaderPath("bg.frag");
    Shader backgroundShader(bgVertPath, bgFragPath);
    CameraBackgroundRenderer backgroundRenderer;
    Grid grid(10, 1.0f);
    TransformTool transformTool;

    Scene scene;
    g_Scene = &scene;

    SceneObject& defaultCamera = scene.AddObject(CameraFactory::Create(CameraFactory::Preset::Free, 3.0f));
    scene.SetActiveViewCameraId(defaultCamera.id);

    SceneObject triangle;
    triangle.name = "triangle";
    triangle.vertices = {
        {-0.5f, -0.5f, 0.0f},
        { 0.5f, -0.5f, 0.0f},
        { 0.0f,  0.5f, 0.0f},
    };
    triangle.color = {1.0f, 0.5f, 0.2f};
    triangle.UploadMesh();
    scene.AddObject(std::move(triangle));

    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    glfwGetCursorPos(glfwWindow, &lastMouseX, &lastMouseY);

    bool wasLeftDown = false;
    bool wasRightDown = false;
    bool rightOrbitActive = false;
    float rightDragDistance = 0.0f;
    std::optional<int> rightClickTarget;

    bool wasKeyX = false;
    bool wasKeyY = false;
    bool wasKeyZ = false;
    bool wasKeyEnter = false;
    bool wasKeyEscape = false;
    bool wasKeyBackspace = false;
    bool wasKeyMinus = false;
    bool wasKeyPeriod = false;
    bool wasDigitKey[10] = {};
    bool wasKpDigitKey[10] = {};

    while (!window.ShouldClose()) {
        window.PollEvents();
        MainMenu::BeginFrame();
        EnsureActiveViewCamera(scene);

        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(glfwWindow, &mouseX, &mouseY);
        const float dx = static_cast<float>(mouseX - lastMouseX);
        const float dy = static_cast<float>(mouseY - lastMouseY);
        lastMouseX = mouseX;
        lastMouseY = mouseY;

        const bool leftDown = glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        const bool middleDown = glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
        const bool rightDown = glfwGetMouseButton(glfwWindow, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        const bool shiftDown =
            glfwGetKey(glfwWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(glfwWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

        const bool keyX = glfwGetKey(glfwWindow, GLFW_KEY_X) == GLFW_PRESS;
        const bool keyY = glfwGetKey(glfwWindow, GLFW_KEY_Y) == GLFW_PRESS;
        const bool keyZ = glfwGetKey(glfwWindow, GLFW_KEY_Z) == GLFW_PRESS;
        const bool keyEnter =
            glfwGetKey(glfwWindow, GLFW_KEY_ENTER) == GLFW_PRESS ||
            glfwGetKey(glfwWindow, GLFW_KEY_KP_ENTER) == GLFW_PRESS;
        const bool keyEscape = glfwGetKey(glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        const bool keyBackspace = glfwGetKey(glfwWindow, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
        const bool keyMinus =
            glfwGetKey(glfwWindow, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(glfwWindow, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
        const bool keyPeriod =
            glfwGetKey(glfwWindow, GLFW_KEY_PERIOD) == GLFW_PRESS ||
            glfwGetKey(glfwWindow, GLFW_KEY_KP_DECIMAL) == GLFW_PRESS;
        bool digitDown[10] = {};
        bool kpDigitDown[10] = {};
        for (int digit = 0; digit <= 9; ++digit) {
            digitDown[digit] = glfwGetKey(glfwWindow, GLFW_KEY_0 + digit) == GLFW_PRESS;
            kpDigitDown[digit] = glfwGetKey(glfwWindow, GLFW_KEY_KP_0 + digit) == GLFW_PRESS;
        }

        const float aspect = window.GetAspect();
        const ViewBasis viewBasis = GetActiveViewBasis(scene);
        const glm::mat4 viewMatrix = GetActiveViewMatrix(scene);
        const glm::mat4 projectionMatrix = GetActiveProjection(scene, aspect);
        const glm::vec3 viewPosition = GetActiveViewPosition(scene);

        auto pickAtCursor = [&]() {
            return Picking::PickObject(
                scene,
                static_cast<float>(mouseX),
                static_cast<float>(mouseY),
                window.GetWidth(),
                window.GetHeight(),
                viewMatrix,
                projectionMatrix,
                viewPosition);
        };

        if (transformTool.IsActive()) {
            // Tool mode owns the keyboard so axis/numeric input works for cameras too
            // (Objects panel focus would otherwise set WantCaptureKeyboard).
            // Still yield while ImGui is actively editing a text/number field.
            if (!ImGui::GetIO().WantTextInput) {
                if (keyBackspace && !wasKeyBackspace) {
                    transformTool.BackspaceNumeric(scene);
                }
                if (keyMinus && !wasKeyMinus) {
                    transformTool.AppendNumericChar('-', scene);
                }
                if (keyPeriod && !wasKeyPeriod) {
                    transformTool.AppendNumericChar('.', scene);
                }
                for (int digit = 0; digit <= 9; ++digit) {
                    if ((digitDown[digit] && !wasDigitKey[digit]) ||
                        (kpDigitDown[digit] && !wasKpDigitKey[digit])) {
                        transformTool.AppendNumericChar(static_cast<char>('0' + digit), scene);
                    }
                }

                if (keyX && !wasKeyX) {
                    transformTool.ToggleAxisConstraint(AxisConstraint::X, scene);
                }
                if (keyY && !wasKeyY) {
                    transformTool.ToggleAxisConstraint(AxisConstraint::Y, scene);
                }
                if (keyZ && !wasKeyZ) {
                    transformTool.ToggleAxisConstraint(AxisConstraint::Z, scene);
                }
                if (keyEnter && !wasKeyEnter) {
                    transformTool.Confirm();
                }
                if (keyEscape && !wasKeyEscape) {
                    transformTool.Cancel(scene);
                }
            }

            if (!MainMenu::WantCaptureMouse()) {
                if (middleDown) {
                    NavigateActiveView(scene, dx, dy, true, false);
                } else {
                    transformTool.Update(scene, viewBasis, dx, dy, shiftDown);
                }

                if (leftDown && !wasLeftDown) {
                    transformTool.Confirm();
                }

                if (rightDown && !wasRightDown) {
                    transformTool.Cancel(scene);
                }
            }
        } else if (!MainMenu::WantCaptureMouse()) {
            if (middleDown) {
                NavigateActiveView(scene, dx, dy, true, false);
            }

            if (rightDown && !wasRightDown) {
                rightDragDistance = 0.0f;
                rightOrbitActive = false;
                rightClickTarget = pickAtCursor();
                if (rightClickTarget.has_value()) {
                    scene.SetSelectedId(rightClickTarget);
                }
            }

            if (rightDown) {
                rightDragDistance += std::sqrt(dx * dx + dy * dy);
                constexpr float kRightDragThreshold = 4.0f;
                if (rightDragDistance >= kRightDragThreshold) {
                    rightOrbitActive = true;
                }
                if (rightOrbitActive) {
                    NavigateActiveView(scene, dx, dy, false, true);
                }
            }

            if (!rightDown && wasRightDown) {
                if (!rightOrbitActive && rightClickTarget.has_value()) {
                    ObjectContextMenu::Open(*rightClickTarget);
                }
                rightClickTarget.reset();
                rightOrbitActive = false;
                rightDragDistance = 0.0f;
            }

            if (leftDown && !wasLeftDown) {
                scene.SetSelectedId(pickAtCursor());
            }
        }

        wasLeftDown = leftDown;
        wasRightDown = rightDown;
        wasKeyX = keyX;
        wasKeyY = keyY;
        wasKeyZ = keyZ;
        wasKeyEnter = keyEnter;
        wasKeyEscape = keyEscape;
        wasKeyBackspace = keyBackspace;
        wasKeyMinus = keyMinus;
        wasKeyPeriod = keyPeriod;
        for (int digit = 0; digit <= 9; ++digit) {
            wasDigitKey[digit] = digitDown[digit];
            wasKpDigitKey[digit] = kpDigitDown[digit];
        }

        glViewport(0, 0, window.GetWidth(), window.GetHeight());
        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (const SceneObject* activeCamera = scene.GetActiveViewCamera()) {
            backgroundRenderer.Draw(activeCamera->background, aspect, backgroundShader);
        }

        const glm::mat4 viewProjection = projectionMatrix * GetActiveViewMatrix(scene);
        grid.Draw(shader, viewProjection);

        const std::optional<int> activeViewId = scene.GetActiveViewCameraId();
        shader.Use();
        for (const SceneObject& object : scene.GetObjects()) {
            // Don't draw the camera you're currently looking through.
            if (object.IsCamera() && activeViewId.has_value() && object.id == *activeViewId) {
                continue;
            }

            glm::vec3 drawColor = object.color;
            if (scene.IsSelected(object.id)) {
                drawColor = {1.0f, 0.85f, 0.2f};
            }

            shader.SetMat4("uMVP", viewProjection * object.transform);
            shader.SetVec3("uColor", drawColor);
            object.mesh.Draw();
        }

        MainMenu::Draw(scene);
        ObjectContextMenu::ActionRequest action = ObjectPanel::Draw(scene);
        if (action.action == ObjectContextMenu::Action::None) {
            action = ObjectContextMenu::Draw(scene);
        } else {
            ObjectContextMenu::Draw(scene);
        }
        HandleObjectAction(action, scene, transformTool);
        transformTool.DrawStatusUi();

        if (const SceneObject* activeCamera = scene.GetActiveViewCamera()) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 12.0f, viewport->WorkPos.y + 8.0f), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.65f);
            if (ImGui::Begin("##ActiveCameraView", nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav)) {
                ImGui::Text("View: %s", activeCamera->name.c_str());
            }
            ImGui::End();
        }

        MainMenu::EndFrame();

        window.SwapBuffers();
    }

    g_Scene = nullptr;
    MainMenu::Shutdown();
    return 0;
}
