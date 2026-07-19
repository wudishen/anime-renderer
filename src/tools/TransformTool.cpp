#include "TransformTool.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void TransformTool::BeginTranslate(int objectId, Scene& scene) {
    SceneObject* object = scene.FindById(objectId);
    if (!object) {
        return;
    }
    m_Mode = TransformMode::Translate;
    m_Axis = AxisConstraint::None;
    m_TargetId = objectId;
    m_OriginalTransform = object->transform;
    scene.SetSelectedId(objectId);
}

void TransformTool::BeginRotate(int objectId, Scene& scene) {
    SceneObject* object = scene.FindById(objectId);
    if (!object) {
        return;
    }
    m_Mode = TransformMode::Rotate;
    m_Axis = AxisConstraint::None;
    m_TargetId = objectId;
    m_OriginalTransform = object->transform;
    scene.SetSelectedId(objectId);
}

void TransformTool::Update(Scene& scene, const ViewBasis& view, float mouseDx, float mouseDy, bool precision) {
    if (!IsActive() || !m_TargetId.has_value()) {
        return;
    }

    SceneObject* object = scene.FindById(*m_TargetId);
    if (!object) {
        Cancel(scene);
        return;
    }

    if (std::fabs(mouseDx) < 1e-6f && std::fabs(mouseDy) < 1e-6f) {
        return;
    }

    if (m_Mode == TransformMode::Translate) {
        const float sensitivity = (precision ? 0.0004f : 0.004f) * view.distance;
        glm::vec3 delta = view.right * (mouseDx * sensitivity) + view.up * (-mouseDy * sensitivity);

        switch (m_Axis) {
        case AxisConstraint::X:
            delta = glm::vec3(glm::dot(delta, glm::vec3(1.0f, 0.0f, 0.0f)), 0.0f, 0.0f);
            break;
        case AxisConstraint::Y:
            delta = glm::vec3(0.0f, glm::dot(delta, glm::vec3(0.0f, 1.0f, 0.0f)), 0.0f);
            break;
        case AxisConstraint::Z:
            delta = glm::vec3(0.0f, 0.0f, glm::dot(delta, glm::vec3(0.0f, 0.0f, 1.0f)));
            break;
        case AxisConstraint::None:
        default:
            break;
        }

        object->AddTranslation(delta);
        return;
    }

    if (m_Mode == TransformMode::Rotate) {
        const float sensitivity = precision ? 0.05f : 0.25f; // degrees per pixel
        const glm::vec3 pos = object->GetTranslation();
        const glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), -pos);
        const glm::mat4 fromOrigin = glm::translate(glm::mat4(1.0f), pos);

        glm::mat4 rotation(1.0f);
        switch (m_Axis) {
        case AxisConstraint::X:
            rotation = glm::rotate(glm::mat4(1.0f), glm::radians((-mouseDx - mouseDy) * sensitivity),
                                   glm::vec3(1.0f, 0.0f, 0.0f));
            break;
        case AxisConstraint::Y:
            rotation = glm::rotate(glm::mat4(1.0f), glm::radians(-mouseDx * sensitivity),
                                   glm::vec3(0.0f, 1.0f, 0.0f));
            break;
        case AxisConstraint::Z:
            rotation = glm::rotate(glm::mat4(1.0f), glm::radians(-mouseDx * sensitivity),
                                   glm::vec3(0.0f, 0.0f, 1.0f));
            break;
        case AxisConstraint::None:
        default: {
            const glm::mat4 yaw = glm::rotate(glm::mat4(1.0f), glm::radians(-mouseDx * sensitivity),
                                              glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 pitch = glm::rotate(glm::mat4(1.0f), glm::radians(-mouseDy * sensitivity), view.right);
            rotation = yaw * pitch;
            break;
        }
        }

        object->transform = fromOrigin * rotation * toOrigin * object->transform;
    }
}

void TransformTool::Confirm() {
    m_Mode = TransformMode::None;
    m_Axis = AxisConstraint::None;
    m_TargetId.reset();
}

void TransformTool::Cancel(Scene& scene) {
    if (IsActive() && m_TargetId.has_value()) {
        if (SceneObject* object = scene.FindById(*m_TargetId)) {
            object->transform = m_OriginalTransform;
        }
    }

    m_Mode = TransformMode::None;
    m_Axis = AxisConstraint::None;
    m_TargetId.reset();
}

void TransformTool::ToggleAxisConstraint(AxisConstraint axis) {
    if (!IsActive()) {
        return;
    }
    m_Axis = (m_Axis == axis) ? AxisConstraint::None : axis;
}

void TransformTool::DrawStatusUi() const {
    if (!IsActive()) {
        return;
    }

    const char* modeLabel = (m_Mode == TransformMode::Rotate) ? "Rotate mode" : "Translate mode";
    const char* axisLabel = "Free";
    switch (m_Axis) {
    case AxisConstraint::X: axisLabel = "X"; break;
    case AxisConstraint::Y: axisLabel = "Y"; break;
    case AxisConstraint::Z: axisLabel = "Z"; break;
    case AxisConstraint::None: default: break;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + 12.0f, viewport->WorkPos.y + viewport->WorkSize.y - 72.0f),
        ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.7f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("##TransformStatus", nullptr, flags)) {
        ImGui::TextUnformatted(modeLabel);
        ImGui::Text("Axis: %s  |  Shift: precision", axisLabel);
        ImGui::TextUnformatted("LMB/Enter: confirm   Esc/RMB: cancel   X/Y/Z: constrain");
    }
    ImGui::End();
}
