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
#include "rendering/LoopBlinnCubic.h"
#include "rendering/LoopBlinnBSpline.h"
#include "scene/Scene.h"
#include "scene/CameraFactory.h"
#include "scene/MeshFactory.h"
#include "scene/CurveFactory.h"
#include "scene/BSplineFit.h"
#include "scene/Picking.h"
#include "tools/TransformTool.h"
#include "tools/CurveEditTool.h"
#include "tools/CameraEditTool.h"
#include "ui/MainMenu.h"
#include "ui/ObjectPanel.h"
#include "ui/ObjectContextMenu.h"

#include <cmath>
#include <cstdio>
#include <array>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static Scene* g_Scene = nullptr;

static glm::vec2 ScreenNormToPixel(const glm::vec2& n, int width, int height) {
    return {n.x * static_cast<float>(width), n.y * static_cast<float>(height)};
}

static bool BuildScreenCurveStroke(
    const SceneObject& object,
    LoopBlinnCubicStroke& cubicOut,
    LoopBlinnBSplineStroke& bsplineOut,
    int width,
    int height) {
    cubicOut.Destroy();
    bsplineOut.Destroy();
    if (width <= 0 || height <= 0) {
        return false;
    }

    if (object.IsBezierCurve()) {
        if (object.controlPoints.size() < 4) {
            return false;
        }
        std::array<glm::vec2, 4> cps{};
        for (int i = 0; i < 4; ++i) {
            cps[i] = ScreenNormToPixel(
                {object.controlPoints[static_cast<size_t>(i)].x,
                 object.controlPoints[static_cast<size_t>(i)].y},
                width,
                height);
        }
        return cubicOut.Build(cps, 0.0f);
    }

    if (object.IsBSplineCurve()) {
        if (object.controlPoints.size() < 4) {
            return false;
        }
        std::vector<glm::vec2> cps;
        cps.reserve(object.controlPoints.size());
        for (const glm::vec3& cp : object.controlPoints) {
            cps.push_back(ScreenNormToPixel({cp.x, cp.y}, width, height));
        }
        return bsplineOut.Build(cps, 0.0f);
    }

    return false;
}

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

static std::vector<glm::vec3> MakeHandleQuadVertices(float halfExtent) {
    // Axis-aligned quad in the screen XY plane (z=0) so it survives pixel ortho near/far.
    const float h = halfExtent;
    return {
        {-h, -h, 0.0f}, {h, -h, 0.0f}, {h, h, 0.0f},
        {-h, -h, 0.0f}, {h, h, 0.0f}, {-h, h, 0.0f},
    };
}

static void HandleObjectAction(
    const ObjectContextMenu::ActionRequest& request,
    Scene& scene,
    TransformTool& transformTool,
    CurveEditTool& curveEditTool,
    CameraEditTool& cameraEditTool) {
    if (request.action == ObjectContextMenu::Action::None) {
        return;
    }

    if (transformTool.IsActive()) {
        transformTool.Cancel(scene);
    }
    if (curveEditTool.IsActive()) {
        curveEditTool.End();
    }
    if (cameraEditTool.IsActive()) {
        cameraEditTool.End();
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
        if (const SceneObject* object = scene.FindById(request.objectId);
            object && object->IsBSplineCurve()) {
            transformTool.BeginScale(request.objectId, scene);
        } else {
            std::cout << "Scale tool is not implemented yet." << std::endl;
        }
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
    case ObjectContextMenu::Action::Edit:
        if (const SceneObject* object = scene.FindById(request.objectId)) {
            if (object->IsCamera()) {
                cameraEditTool.Begin(request.objectId, scene);
            } else if (object->IsCurve()) {
                curveEditTool.Begin(request.objectId, scene);
            }
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
    const std::string cubicVertPath = ResolveShaderPath("cubic_bezier.vert");
    const std::string cubicFragPath = ResolveShaderPath("cubic_bezier.frag");
    Shader cubicBezierShader(cubicVertPath, cubicFragPath);
    CameraBackgroundRenderer backgroundRenderer;
    Grid grid(10, 1.0f);
    TransformTool transformTool;
    CurveEditTool curveEditTool;
    CameraEditTool cameraEditTool;

    Mesh controlPointHandle;
    controlPointHandle.Upload(MakeHandleQuadVertices(7.0f)); // ~14px square in screen ortho

    // Scratch strokes for screen-space scene curve annotations.
    LoopBlinnCubicStroke screenCubicStroke;
    LoopBlinnBSplineStroke screenBSplineStroke;

    Scene scene;
    g_Scene = &scene;

    SceneObject& defaultCamera = scene.AddObject(CameraFactory::Create(CameraFactory::Preset::Free, 3.0f));
    scene.SetActiveViewCameraId(defaultCamera.id);

    scene.AddObject(MeshFactory::CreateTriangle());

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
    bool wasKeyTab = false;
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
        const bool keyTab = glfwGetKey(glfwWindow, GLFW_KEY_TAB) == GLFW_PRESS;
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
                    transformTool.Update(
                        scene,
                        viewBasis,
                        dx,
                        dy,
                        shiftDown,
                        window.GetWidth(),
                        window.GetHeight());
                }

                if (leftDown && !wasLeftDown) {
                    transformTool.Confirm();
                }

                if (rightDown && !wasRightDown) {
                    transformTool.Cancel(scene);
                }
            }
        } else if (curveEditTool.IsActive()) {
            if (!ImGui::GetIO().WantTextInput) {
                if (keyBackspace && !wasKeyBackspace) {
                    curveEditTool.BackspaceNumeric(scene);
                }
                if (keyMinus && !wasKeyMinus) {
                    curveEditTool.AppendNumericChar('-', scene);
                }
                if (keyPeriod && !wasKeyPeriod) {
                    curveEditTool.AppendNumericChar('.', scene);
                }
                for (int digit = 0; digit <= 9; ++digit) {
                    if ((digitDown[digit] && !wasDigitKey[digit]) ||
                        (kpDigitDown[digit] && !wasKpDigitKey[digit])) {
                        curveEditTool.AppendNumericChar(static_cast<char>('0' + digit), scene);
                    }
                }

                if (keyX && !wasKeyX) {
                    curveEditTool.ToggleAxisConstraint(AxisConstraint::X, scene);
                }
                if (keyY && !wasKeyY) {
                    curveEditTool.ToggleAxisConstraint(AxisConstraint::Y, scene);
                }
                if (keyTab && !wasKeyTab) {
                    curveEditTool.ToggleHandleMode(scene);
                }
                if (keyEnter && !wasKeyEnter) {
                    if (curveEditTool.IsTranslating()) {
                        curveEditTool.ConfirmTranslate();
                    }
                }
                if (keyEscape && !wasKeyEscape) {
                    if (curveEditTool.IsTranslating()) {
                        curveEditTool.CancelTranslate(scene);
                    } else {
                        curveEditTool.End();
                    }
                }
            }

            if (!MainMenu::WantCaptureMouse()) {
                if (middleDown) {
                    NavigateActiveView(scene, dx, dy, true, false);
                } else if (curveEditTool.IsTranslating()) {
                    curveEditTool.Update(
                        scene,
                        dx,
                        dy,
                        window.GetWidth(),
                        window.GetHeight(),
                        shiftDown);
                }

                if (leftDown && !wasLeftDown) {
                    if (curveEditTool.IsTranslating()) {
                        curveEditTool.ConfirmTranslate();
                    } else if (curveEditTool.GetTargetId().has_value()) {
                        if (const SceneObject* curve = scene.FindById(*curveEditTool.GetTargetId())) {
                            const std::optional<CurvePointKind> kindFilter =
                                curve->IsBSplineCurve()
                                    ? std::optional<CurvePointKind>(curveEditTool.GetHandleMode())
                                    : std::optional<CurvePointKind>(CurvePointKind::Control);
                            if (const auto handle = Picking::PickCurveHandle(
                                    *curve,
                                    static_cast<float>(mouseX),
                                    static_cast<float>(mouseY),
                                    window.GetWidth(),
                                    window.GetHeight(),
                                    12.0f,
                                    kindFilter)) {
                                curveEditTool.BeginTranslatePoint(handle->kind, handle->index, scene);
                            }
                        }
                    }
                }

                if (rightDown && !wasRightDown) {
                    if (curveEditTool.IsTranslating()) {
                        curveEditTool.CancelTranslate(scene);
                    } else {
                        // Allow opening context menu on the curve while editing.
                        rightDragDistance = 0.0f;
                        rightOrbitActive = false;
                        rightClickTarget = pickAtCursor();
                        if (rightClickTarget.has_value()) {
                            scene.SetSelectedId(rightClickTarget);
                        }
                    }
                }

                if (rightDown && !curveEditTool.IsTranslating()) {
                    rightDragDistance += std::fabs(dx) + std::fabs(dy);
                    if (rightDragDistance > 4.0f) {
                        rightOrbitActive = true;
                        NavigateActiveView(scene, dx, dy, false, true);
                    }
                }

                if (!rightDown && wasRightDown && !curveEditTool.IsTranslating()) {
                    if (!rightOrbitActive && rightClickTarget.has_value()) {
                        ObjectContextMenu::Open(*rightClickTarget);
                    }
                    rightClickTarget.reset();
                    rightOrbitActive = false;
                    rightDragDistance = 0.0f;
                }
            }
        } else if (cameraEditTool.IsActive()) {
            if (!ImGui::GetIO().WantTextInput) {
                if (keyEscape && !wasKeyEscape) {
                    cameraEditTool.End();
                }
            }

            if (!MainMenu::WantCaptureMouse()) {
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
                    rightDragDistance += std::fabs(dx) + std::fabs(dy);
                    if (rightDragDistance > 4.0f) {
                        rightOrbitActive = true;
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
        wasKeyTab = keyTab;
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
            if (object.IsCurve()) {
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

        // Screen-space curve annotations (normalized CPs → pixels + ortho overlay).
        const int fbWidth = window.GetWidth();
        const int fbHeight = window.GetHeight();
        {
            const glm::mat4 screenOrtho = glm::ortho(
                0.0f,
                static_cast<float>(fbWidth),
                static_cast<float>(fbHeight),
                0.0f,
                -1.0f,
                1.0f);

            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            cubicBezierShader.Use();
            cubicBezierShader.SetMat4("uMVP", screenOrtho);
            cubicBezierShader.SetFloat("uHalfWidth", 1.25f);

            for (const SceneObject& object : scene.GetObjects()) {
                if (!object.IsCurve()) {
                    continue;
                }
                if (!BuildScreenCurveStroke(object, screenCubicStroke, screenBSplineStroke, fbWidth, fbHeight)) {
                    continue;
                }

                glm::vec3 drawColor = object.color;
                if (scene.IsSelected(object.id)) {
                    drawColor = {1.0f, 0.85f, 0.2f};
                }
                cubicBezierShader.SetVec3("uColor", drawColor);
                if (object.IsBezierCurve() && screenCubicStroke.IsValid()) {
                    screenCubicStroke.Draw();
                } else if (object.IsBSplineCurve() && screenBSplineStroke.IsValid()) {
                    screenBSplineStroke.Draw();
                }
            }

            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
        }

        MainMenu::Draw(scene);
        ObjectContextMenu::ActionRequest action = ObjectPanel::Draw(
            scene,
            curveEditTool.IsActive() ? curveEditTool.GetTargetId() : std::nullopt);
        if (action.action == ObjectContextMenu::Action::None) {
            action = ObjectContextMenu::Draw(
                scene,
                curveEditTool.IsActive() ? curveEditTool.GetTargetId() : std::nullopt);
        } else {
            ObjectContextMenu::Draw(
                scene,
                curveEditTool.IsActive() ? curveEditTool.GetTargetId() : std::nullopt);
        }
        HandleObjectAction(action, scene, transformTool, curveEditTool, cameraEditTool);

        // Screen-space handles (drawn after Edit is applied so they appear immediately).
        if (curveEditTool.IsActive() && curveEditTool.GetTargetId().has_value()) {
            if (const SceneObject* curve = scene.FindById(*curveEditTool.GetTargetId())) {
                const int w = window.GetWidth();
                const int h = window.GetHeight();
                const glm::mat4 screenOrtho = glm::ortho(
                    0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, -1.0f, 1.0f);
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_BLEND);
                shader.Use();

                const auto selectedKind = curveEditTool.GetSelectedPointKind();
                const auto selectedIndex = curveEditTool.GetSelectedPoint();
                const CurvePointKind handleMode = curveEditTool.GetHandleMode();

                // Draw only the active handle kind (control vs fit).
                if (curve->IsBSplineCurve() && handleMode == CurvePointKind::Fit) {
                    const std::vector<glm::vec2> fitPoints =
                        BSplineFit::FitPointsFromControls(curve->controlPoints);
                    for (size_t i = 0; i < fitPoints.size(); ++i) {
                        const bool selectedPoint =
                            selectedKind.has_value() &&
                            *selectedKind == CurvePointKind::Fit &&
                            selectedIndex.has_value() &&
                            *selectedIndex == static_cast<int>(i);
                        const glm::vec3 handleColor =
                            selectedPoint ? glm::vec3(1.0f, 0.35f, 0.35f)
                                          : glm::vec3(0.25f, 0.95f, 0.95f);
                        const glm::vec2 pixel = ScreenNormToPixel(fitPoints[i], w, h);
                        const glm::mat4 handleModel =
                            glm::translate(glm::mat4(1.0f), glm::vec3(pixel.x, pixel.y, 0.0f)) *
                            glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)) *
                            glm::scale(glm::mat4(1.0f), glm::vec3(0.85f, 0.85f, 1.0f));
                        shader.SetMat4("uMVP", screenOrtho * handleModel);
                        shader.SetVec3("uColor", handleColor);
                        controlPointHandle.Draw();
                    }
                } else {
                    for (size_t i = 0; i < curve->controlPoints.size(); ++i) {
                        const bool selectedPoint =
                            selectedKind.has_value() &&
                            *selectedKind == CurvePointKind::Control &&
                            selectedIndex.has_value() &&
                            *selectedIndex == static_cast<int>(i);
                        const glm::vec3 handleColor =
                            selectedPoint ? glm::vec3(1.0f, 0.35f, 0.35f) : glm::vec3(1.0f, 1.0f, 1.0f);
                        const glm::vec2 pixel = ScreenNormToPixel(
                            {curve->controlPoints[i].x, curve->controlPoints[i].y}, w, h);
                        const glm::mat4 handleModel =
                            glm::translate(glm::mat4(1.0f), glm::vec3(pixel.x, pixel.y, 0.0f));
                        shader.SetMat4("uMVP", screenOrtho * handleModel);
                        shader.SetVec3("uColor", handleColor);
                        controlPointHandle.Draw();
                    }
                }
                glEnable(GL_DEPTH_TEST);
            }
        }

        transformTool.DrawStatusUi();
        curveEditTool.DrawStatusUi(scene);
        cameraEditTool.DrawStatusUi(scene);

        if (const SceneObject* activeCamera = scene.GetActiveViewCamera()) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 12.0f, viewport->WorkPos.y + 8.0f), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.65f);
            if (ImGui::Begin("##ActiveCameraView", nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav)) {
                ImGui::Text("View: %s  |  FOV: %.1f deg",
                            activeCamera->name.c_str(),
                            activeCamera->cameraFovDegrees);
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
