#include "ObjectPanel.h"

#include <imgui.h>
#include <string>

namespace ObjectPanel {

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
    }
    ImGui::End();

    return request;
}

} // namespace ObjectPanel
