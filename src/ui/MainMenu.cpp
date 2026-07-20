#include "MainMenu.h"
#include "io/FileDialog.h"
#include "io/ObjLoader.h"
#include "io/SceneFile.h"
#include "scene/CameraFactory.h"
#include "scene/MeshFactory.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <string>

namespace MainMenu {
namespace {

std::string g_ScenePath;
std::string g_MessageTitle = "Message";
std::string g_MessageBody;
bool g_OpenMessagePopup = false;

void ShowMessage(const char* title, const std::string& body) {
    g_MessageTitle = title;
    g_MessageBody = body;
    g_OpenMessagePopup = true;
}

void ResetToDefaultScene(Scene& scene) {
    scene.Clear();
    SceneObject& camera = scene.AddObject(CameraFactory::Create(CameraFactory::Preset::Free, 3.0f));
    scene.SetActiveViewCameraId(camera.id);

    scene.AddObject(MeshFactory::CreateTriangle());

    SceneObject sphere = MeshFactory::CreateSphere();
    sphere.SetTranslation({2.0f, 0.5f, 0.0f});
    scene.AddObject(std::move(sphere));

    SceneObject cone = MeshFactory::CreateCone();
    cone.SetTranslation({-2.0f, 0.0f, 0.0f});
    scene.AddObject(std::move(cone));
}

void OnFileNew(Scene& scene) {
    ResetToDefaultScene(scene);
    g_ScenePath.clear();
}

bool SaveToPath(Scene& scene, const std::string& path) {
    const SceneFile::Result result = SceneFile::Save(scene, path);
    if (!result.ok) {
        ShowMessage("Save Failed", result.error);
        return false;
    }
    g_ScenePath = path;
    return true;
}

void OnFileSave(Scene& scene) {
    if (!g_ScenePath.empty()) {
        SaveToPath(scene, g_ScenePath);
        return;
    }

    std::string path;
    if (!FileDialog::SaveSceneFile(path)) {
        return;
    }
    SaveToPath(scene, path);
}

void OnFileSaveAs(Scene& scene) {
    std::string path = g_ScenePath;
    if (!FileDialog::SaveSceneFile(path)) {
        return;
    }
    SaveToPath(scene, path);
}

void OnFileOpen(Scene& scene) {
    std::string path = g_ScenePath;
    if (!FileDialog::OpenSceneFile(path)) {
        return;
    }

    const SceneFile::Result result = SceneFile::Load(scene, path);
    if (!result.ok) {
        ShowMessage("Open Failed", result.error);
        return;
    }

    g_ScenePath = path;
    if (!result.warning.empty()) {
        ShowMessage("Opened with Warnings", result.warning);
    }
}

void OnFileImport(Scene& scene) {
    std::string path;
    if (!FileDialog::OpenObjFile(path)) {
        return;
    }

    ObjLoader::LoadResult result = ObjLoader::Load(path);
    if (result.object.has_value()) {
        SceneObject& added = scene.AddObject(std::move(*result.object));
        scene.SetSelectedId(added.id);
    } else {
        ShowMessage("Import Failed", result.error.empty() ? "Import failed." : result.error);
    }
}

void OnFileExport() {
    std::cout << "File > Export" << std::endl;
}

void AddCamera(Scene& scene, CameraFactory::Preset preset) {
    SceneObject& added = scene.AddObject(CameraFactory::Create(preset));
    scene.SetSelectedId(added.id);
}

void AddMesh(Scene& scene, SceneObject object) {
    SceneObject& added = scene.AddObject(std::move(object));
    scene.SetSelectedId(added.id);
}

void DrawMessagePopup() {
    if (g_OpenMessagePopup) {
        ImGui::OpenPopup("##AppMessage");
        g_OpenMessagePopup = false;
    }

    if (ImGui::BeginPopupModal("##AppMessage", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(g_MessageTitle.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("%s", g_MessageBody.c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            g_MessageBody.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace

void Init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");
}

void Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void BeginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Draw(Scene& scene) {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New", "Ctrl+N")) {
                OnFileNew(scene);
            }
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                OnFileOpen(scene);
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                OnFileSave(scene);
            }
            if (ImGui::MenuItem("Save As...")) {
                OnFileSaveAs(scene);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Import")) {
                OnFileImport(scene);
            }
            if (ImGui::MenuItem("Export")) {
                OnFileExport();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Add")) {
            if (ImGui::BeginMenu("Mesh")) {
                if (ImGui::MenuItem("Triangle")) {
                    AddMesh(scene, MeshFactory::CreateTriangle());
                }
                if (ImGui::MenuItem("Sphere")) {
                    AddMesh(scene, MeshFactory::CreateSphere());
                }
                if (ImGui::MenuItem("Cone")) {
                    AddMesh(scene, MeshFactory::CreateCone());
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Camera")) {
                if (ImGui::MenuItem("Camera")) {
                    AddCamera(scene, CameraFactory::Preset::Free);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Top Camera")) {
                    AddCamera(scene, CameraFactory::Preset::Top);
                }
                if (ImGui::MenuItem("Bottom Camera")) {
                    AddCamera(scene, CameraFactory::Preset::Bottom);
                }
                if (ImGui::MenuItem("Left Camera")) {
                    AddCamera(scene, CameraFactory::Preset::Left);
                }
                if (ImGui::MenuItem("Right Camera")) {
                    AddCamera(scene, CameraFactory::Preset::Right);
                }
                if (ImGui::MenuItem("Front Camera")) {
                    AddCamera(scene, CameraFactory::Preset::Front);
                }
                if (ImGui::MenuItem("Back Camera")) {
                    AddCamera(scene, CameraFactory::Preset::Back);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Layout")) {
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    DrawMessagePopup();
}

void EndFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool WantCaptureMouse() {
    return ImGui::GetIO().WantCaptureMouse;
}

bool WantCaptureKeyboard() {
    return ImGui::GetIO().WantCaptureKeyboard;
}

} // namespace MainMenu
