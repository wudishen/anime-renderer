#include "ObjectPanel.h"
#include "io/FileDialog.h"

#include <imgui.h>
#include <filesystem>
#include <string>

namespace ObjectPanel {
namespace {

void DrawCameraBackgroundControls(SceneObject& camera) {
    ImGui::Separator();
    ImGui::TextUnformatted("Background Image");
    ImGui::Spacing();

    if (ImGui::Button("Upload Image...")) {
        std::string path;
        if (FileDialog::OpenImageFile(path)) {
            if (!camera.background.LoadFromFile(path)) {
                // Keep previous image if load fails.
            }
        }
    }

    if (camera.background.HasImage()) {
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            camera.background.Clear();
        }

        const std::string fileName = std::filesystem::path(camera.background.imagePath).filename().string();
        ImGui::TextWrapped("%s", fileName.c_str());
        ImGui::TextDisabled("%dx%d", camera.background.width, camera.background.height);

        ImGui::Checkbox("Enabled", &camera.background.enabled);
        ImGui::SliderFloat("Opacity", &camera.background.opacity, 0.0f, 1.0f, "%.2f");
        ImGui::DragFloat2("Position", &camera.background.offset.x, 0.01f, -2.0f, 2.0f, "%.2f");
        ImGui::DragFloat("Scale", &camera.background.scale, 0.01f, 0.05f, 5.0f, "%.2f");
    } else {
        ImGui::TextDisabled("No background image");
    }
}

} // namespace

ObjectContextMenu::ActionRequest Draw(Scene& scene) {
    constexpr float kPanelWidth = 260.0f;
    ObjectContextMenu::ActionRequest request;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 panelPos(viewport->WorkPos.x + viewport->WorkSize.x - kPanelWidth, viewport->WorkPos.y);
    const ImVec2 panelSize(kPanelWidth, viewport->WorkSize.y);

    ImGui::SetNextWindowPos(panelPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(panelSize, ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("Objects", nullptr, flags)) {
        ImGui::TextUnformatted("Scene");
        ImGui::Separator();

        if (scene.GetObjects().empty()) {
            ImGui::TextDisabled("No objects");
        } else {
            for (const SceneObject& object : scene.GetObjects()) {
                const bool selected = scene.IsSelected(object.id);
                const bool isActiveView =
                    object.IsCamera() && scene.GetActiveViewCameraId().has_value() &&
                    *scene.GetActiveViewCameraId() == object.id;

                ImGui::PushID(object.id);
                std::string label = object.name;
                if (object.IsCamera()) {
                    label = std::string(isActiveView ? "[View] " : "[Cam] ") + label;
                }

                if (ImGui::Selectable(label.c_str(), selected)) {
                    scene.SetSelectedId(object.id);
                }

                if (ImGui::BeginPopupContextItem("object_item_context")) {
                    scene.SetSelectedId(object.id);
                    const ObjectContextMenu::ActionRequest itemRequest =
                        ObjectContextMenu::DrawMenuItems(scene, object.id);
                    if (itemRequest.action != ObjectContextMenu::Action::None) {
                        request = itemRequest;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }

        if (scene.GetSelectedId().has_value()) {
            if (SceneObject* selected = scene.FindById(*scene.GetSelectedId());
                selected && selected->IsCamera()) {
                DrawCameraBackgroundControls(*selected);
            }
        }
    }
    ImGui::End();

    return request;
}

} // namespace ObjectPanel
