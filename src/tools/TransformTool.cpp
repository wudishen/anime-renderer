#include "TransformTool.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <charconv>

void TransformTool::BeginTranslate(int objectId, Scene& scene) {
    SceneObject* object = scene.FindById(objectId);
    if (!object) {
        return;
    }
    m_Mode = TransformMode::Translate;
    m_Axis = AxisConstraint::None;
    m_TargetId = objectId;
    m_OriginalTransform = object->transform;
    ClearNumericInput();
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
    ClearNumericInput();
    scene.SetSelectedId(objectId);
}

void TransformTool::Update(Scene& scene, const ViewBasis& view, float mouseDx, float mouseDy, bool precision) {
    if (!IsActive() || !m_TargetId.has_value()) {
        return;
    }

    // Typed numeric offset replaces mouse movement until the buffer is cleared.
    if (IsNumericInputActive()) {
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
    ClearNumericInput();
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
    ClearNumericInput();
}

void TransformTool::ToggleAxisConstraint(AxisConstraint axis, Scene& scene) {
    if (!IsActive()) {
        return;
    }

    // Changing or clearing the axis drops typed input and restores the start transform.
    if (IsNumericInputActive()) {
        if (SceneObject* object = scene.FindById(*m_TargetId)) {
            object->transform = m_OriginalTransform;
        }
        ClearNumericInput();
    }

    m_Axis = (m_Axis == axis) ? AxisConstraint::None : axis;
}

bool TransformTool::CanAppendNumericChar(char c) const {
    if (c == '-') {
        return m_NumericInput.empty();
    }
    if (c == '.') {
        return m_NumericInput.find('.') == std::string::npos;
    }
    return c >= '0' && c <= '9';
}

bool TransformTool::AppendNumericChar(char c, Scene& scene) {
    if (m_Mode != TransformMode::Translate || m_Axis == AxisConstraint::None || !m_TargetId.has_value()) {
        return false;
    }
    if (!CanAppendNumericChar(c)) {
        return true; // key is for numeric entry, just reject the character
    }

    m_NumericInput.push_back(c);
    ApplyNumericOffset(scene);
    return true;
}

bool TransformTool::BackspaceNumeric(Scene& scene) {
    if (m_Mode != TransformMode::Translate || !IsNumericInputActive()) {
        return false;
    }

    m_NumericInput.pop_back();
    ApplyNumericOffset(scene);
    return true;
}

void TransformTool::ClearNumericInput() {
    m_NumericInput.clear();
}

void TransformTool::ApplyNumericOffset(Scene& scene) {
    if (!m_TargetId.has_value()) {
        return;
    }

    SceneObject* object = scene.FindById(*m_TargetId);
    if (!object) {
        return;
    }

    object->transform = m_OriginalTransform;

    if (m_NumericInput.empty() || m_NumericInput == "-" || m_NumericInput == "." || m_NumericInput == "-.") {
        return;
    }

    float value = 0.0f;
    const char* begin = m_NumericInput.data();
    const char* end = begin + m_NumericInput.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        // Incomplete mid-entry (should be rare); leave at original until valid.
        return;
    }

    glm::vec3 delta(0.0f);
    switch (m_Axis) {
    case AxisConstraint::X:
        delta.x = value;
        break;
    case AxisConstraint::Y:
        delta.y = value;
        break;
    case AxisConstraint::Z:
        delta.z = value;
        break;
    case AxisConstraint::None:
    default:
        return;
    }

    object->AddTranslation(delta);
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

    const bool showNumericHint =
        m_Mode == TransformMode::Translate && m_Axis != AxisConstraint::None;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float statusHeight = showNumericHint ? 96.0f : 72.0f;
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + 12.0f, viewport->WorkPos.y + viewport->WorkSize.y - statusHeight),
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
        if (showNumericHint) {
            if (IsNumericInputActive()) {
                ImGui::Text("Offset: %s", m_NumericInput.c_str());
            } else {
                ImGui::TextUnformatted("Type a number for exact offset");
            }
        }
        ImGui::TextUnformatted("LMB/Enter: confirm   Esc/RMB: cancel   X/Y/Z: constrain");
    }
    ImGui::End();
}
