#include "ObjectContextMenu.h"

#include <imgui.h>
#include <optional>

namespace ObjectContextMenu {
namespace {

std::optional<int> g_TargetId;
bool g_OpenRequested = false;

} // namespace

void Open(int objectId) {
    g_TargetId = objectId;
    g_OpenRequested = true;
}

ActionRequest DrawMenuItems(Scene& scene, int objectId) {
    ActionRequest request;
    request.objectId = objectId;

    const SceneObject* object = scene.FindById(objectId);
    const bool isCamera = object && object->IsCamera();

    if (isCamera) {
        if (ImGui::MenuItem("Go to View")) {
            request.action = Action::GoToView;
        }
        ImGui::Separator();
    }

    if (ImGui::MenuItem("Translate")) {
        request.action = Action::Translate;
    }
    if (!isCamera && ImGui::MenuItem("Scale")) {
        request.action = Action::Scale;
    }
    if (ImGui::MenuItem("Rotate")) {
        request.action = Action::Rotate;
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Delete")) {
        request.action = Action::Delete;
    }

    return request;
}

ActionRequest Draw(Scene& scene) {
    ActionRequest request;

    if (g_OpenRequested) {
        ImGui::OpenPopup("##ObjectContextMenu");
        g_OpenRequested = false;
    }

    if (ImGui::BeginPopup("##ObjectContextMenu")) {
        if (g_TargetId.has_value()) {
            request = DrawMenuItems(scene, *g_TargetId);
            if (request.action != Action::None) {
                ImGui::CloseCurrentPopup();
                g_TargetId.reset();
            }
        }
        ImGui::EndPopup();
    }

    return request;
}

} // namespace ObjectContextMenu
