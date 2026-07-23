#include "CameraEditTool.h"

#include <imgui.h>
#include <algorithm>

void CameraEditTool::Begin(int objectId, Scene& scene) {
    SceneObject* object = scene.FindById(objectId);
    if (!object || !object->IsCamera()) {
        return;
    }

    m_Active = true;
    m_TargetId = objectId;
    scene.SetSelectedId(objectId);
}

void CameraEditTool::End() {
    m_Active = false;
    m_TargetId.reset();
}

void CameraEditTool::DrawStatusUi(Scene& scene) {
    if (!m_Active || !m_TargetId.has_value()) {
        return;
    }

    SceneObject* camera = scene.FindById(*m_TargetId);
    if (!camera || !camera->IsCamera()) {
        End();
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + 12.0f, viewport->WorkPos.y + viewport->WorkSize.y - 96.0f),
        ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.7f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("##CameraEditStatus", nullptr, flags)) {
        ImGui::Text("Edit camera: %s", camera->name.c_str());
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::SliderFloat("FOV (deg)", &camera->cameraFovDegrees, 1.0f, 179.0f, "%.1f")) {
            camera->cameraFovDegrees = std::clamp(camera->cameraFovDegrees, 1.0f, 179.0f);
        }
        ImGui::TextUnformatted("Esc: exit edit mode");
    }
    ImGui::End();
}
