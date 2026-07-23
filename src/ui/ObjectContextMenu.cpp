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

ActionRequest DrawMenuItems(Scene& scene, int objectId, std::optional<int> curveEditObjectId) {
    ActionRequest request;
    request.objectId = objectId;

    const SceneObject* object = scene.FindById(objectId);
    const bool isCamera = object && object->IsCamera();
    const bool isCurve = object && object->IsCurve();
    const bool isBSpline = object && object->IsBSplineCurve();
    const bool editingThisCurve =
        curveEditObjectId.has_value() && *curveEditObjectId == objectId;

    if (isCurve) {
        if (ImGui::MenuItem("Edit")) {
            request.action = Action::Edit;
        }
        if (isBSpline) {
            ImGui::BeginDisabled(editingThisCurve);
            if (ImGui::MenuItem("Translate")) {
                request.action = Action::Translate;
            }
            if (ImGui::MenuItem("Scale")) {
                request.action = Action::Scale;
            }
            ImGui::EndDisabled();
            if (editingThisCurve) {
                ImGui::TextDisabled("Exit Edit to translate/scale");
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            request.action = Action::Delete;
        }
        return request;
    }

    if (isCamera) {
        if (ImGui::MenuItem("Go to View")) {
            request.action = Action::GoToView;
        }
        if (ImGui::MenuItem("Edit")) {
            request.action = Action::Edit;
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

ActionRequest Draw(Scene& scene, std::optional<int> curveEditObjectId) {
    ActionRequest request;

    if (g_OpenRequested) {
        ImGui::OpenPopup("##ObjectContextMenu");
        g_OpenRequested = false;
    }

    if (ImGui::BeginPopup("##ObjectContextMenu")) {
        if (g_TargetId.has_value()) {
            request = DrawMenuItems(scene, *g_TargetId, curveEditObjectId);
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
