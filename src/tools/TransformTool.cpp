#include "TransformTool.h"

#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <charconv>

namespace {

glm::vec2 ControlPointCentroid(const std::vector<glm::vec3>& points) {
    if (points.empty()) {
        return {0.0f, 0.0f};
    }
    glm::vec2 sum(0.0f);
    for (const glm::vec3& p : points) {
        sum += glm::vec2(p.x, p.y);
    }
    return sum / static_cast<float>(points.size());
}

} // namespace

bool TransformTool::BeginScreenCurve(int objectId, Scene& scene, TransformMode mode) {
    SceneObject* object = scene.FindById(objectId);
    if (!object || !object->IsBSplineCurve() || object->controlPoints.empty()) {
        return false;
    }

    m_Mode = mode;
    m_Axis = AxisConstraint::None;
    m_TargetId = objectId;
    m_ScreenCurve = true;
    m_OriginalControlPoints = object->controlPoints;
    m_OriginalTransform = object->transform;
    m_AccumulatedTranslation = {0.0f, 0.0f};
    m_ScaleCenter = ControlPointCentroid(m_OriginalControlPoints);
    m_ScaleFactor = 1.0f;
    ClearNumericInput();
    scene.SetSelectedId(objectId);
    return true;
}

void TransformTool::BeginTranslate(int objectId, Scene& scene) {
    SceneObject* object = scene.FindById(objectId);
    if (!object) {
        return;
    }

    if (object->IsBSplineCurve()) {
        BeginScreenCurve(objectId, scene, TransformMode::Translate);
        return;
    }

    m_Mode = TransformMode::Translate;
    m_Axis = AxisConstraint::None;
    m_TargetId = objectId;
    m_ScreenCurve = false;
    m_OriginalControlPoints.clear();
    m_OriginalTransform = object->transform;
    ClearNumericInput();
    scene.SetSelectedId(objectId);
}

void TransformTool::BeginRotate(int objectId, Scene& scene) {
    SceneObject* object = scene.FindById(objectId);
    if (!object || object->IsCurve()) {
        return;
    }
    m_Mode = TransformMode::Rotate;
    m_Axis = AxisConstraint::None;
    m_TargetId = objectId;
    m_ScreenCurve = false;
    m_OriginalControlPoints.clear();
    m_OriginalTransform = object->transform;
    ClearNumericInput();
    scene.SetSelectedId(objectId);
}

void TransformTool::BeginScale(int objectId, Scene& scene) {
    // Screen-space B-spline only for now (mesh scale still unimplemented).
    BeginScreenCurve(objectId, scene, TransformMode::Scale);
}

void TransformTool::ApplyScreenCurveTranslation(SceneObject& object) const {
    object.controlPoints = m_OriginalControlPoints;
    for (glm::vec3& p : object.controlPoints) {
        p.x += m_AccumulatedTranslation.x;
        p.y += m_AccumulatedTranslation.y;
        p.z = 0.0f;
    }
}

void TransformTool::ApplyScreenCurveScale(SceneObject& object) const {
    object.controlPoints = m_OriginalControlPoints;
    for (glm::vec3& p : object.controlPoints) {
        const glm::vec2 offset = (glm::vec2(p.x, p.y) - m_ScaleCenter) * m_ScaleFactor;
        p.x = m_ScaleCenter.x + offset.x;
        p.y = m_ScaleCenter.y + offset.y;
        p.z = 0.0f;
    }
}

void TransformTool::Update(
    Scene& scene,
    const ViewBasis& view,
    float mouseDx,
    float mouseDy,
    bool precision,
    int viewportWidth,
    int viewportHeight) {
    if (!IsActive() || !m_TargetId.has_value()) {
        return;
    }

    if (IsNumericInputActive()) {
        return;
    }

    SceneObject* object = scene.FindById(*m_TargetId);
    if (!object) {
        Cancel(scene);
        return;
    }

    if (viewportWidth > 0) {
        m_LastWidth = viewportWidth;
    }
    if (viewportHeight > 0) {
        m_LastHeight = viewportHeight;
    }

    if (std::fabs(mouseDx) < 1e-6f && std::fabs(mouseDy) < 1e-6f) {
        return;
    }

    if (m_ScreenCurve) {
        const float w = static_cast<float>(std::max(m_LastWidth, 1));
        const float h = static_cast<float>(std::max(m_LastHeight, 1));

        if (m_Mode == TransformMode::Translate) {
            const float scale = precision ? 0.1f : 1.0f;
            float dxNorm = (mouseDx * scale) / w;
            float dyNorm = (mouseDy * scale) / h;
            switch (m_Axis) {
            case AxisConstraint::X:
                dyNorm = 0.0f;
                break;
            case AxisConstraint::Y:
                dxNorm = 0.0f;
                break;
            case AxisConstraint::Z:
                return;
            case AxisConstraint::None:
            default:
                break;
            }
            m_AccumulatedTranslation.x += dxNorm;
            m_AccumulatedTranslation.y += dyNorm;
            ApplyScreenCurveTranslation(*object);
            return;
        }

        if (m_Mode == TransformMode::Scale) {
            const float sensitivity = precision ? 0.001f : 0.01f;
            float drag = 0.0f;
            switch (m_Axis) {
            case AxisConstraint::X:
                drag = mouseDx;
                break;
            case AxisConstraint::Y:
                drag = -mouseDy;
                break;
            case AxisConstraint::Z:
                return;
            case AxisConstraint::None:
            default:
                drag = mouseDx - mouseDy;
                break;
            }
            m_ScaleFactor = std::max(0.05f, m_ScaleFactor * (1.0f + drag * sensitivity));
            ApplyScreenCurveScale(*object);
            return;
        }

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
    m_ScreenCurve = false;
    m_OriginalControlPoints.clear();
    m_AccumulatedTranslation = {0.0f, 0.0f};
    m_ScaleFactor = 1.0f;
    ClearNumericInput();
}

void TransformTool::Cancel(Scene& scene) {
    if (IsActive() && m_TargetId.has_value()) {
        if (SceneObject* object = scene.FindById(*m_TargetId)) {
            if (m_ScreenCurve) {
                object->controlPoints = m_OriginalControlPoints;
            } else {
                object->transform = m_OriginalTransform;
            }
        }
    }

    m_Mode = TransformMode::None;
    m_Axis = AxisConstraint::None;
    m_TargetId.reset();
    m_ScreenCurve = false;
    m_OriginalControlPoints.clear();
    m_AccumulatedTranslation = {0.0f, 0.0f};
    m_ScaleFactor = 1.0f;
    ClearNumericInput();
}

void TransformTool::ToggleAxisConstraint(AxisConstraint axis, Scene& scene) {
    if (!IsActive()) {
        return;
    }

    if (m_ScreenCurve && axis == AxisConstraint::Z) {
        return;
    }

    // Changing or clearing the axis drops typed input and restores the start state.
    if (IsNumericInputActive()) {
        if (SceneObject* object = scene.FindById(*m_TargetId)) {
            if (m_ScreenCurve) {
                object->controlPoints = m_OriginalControlPoints;
                m_AccumulatedTranslation = {0.0f, 0.0f};
                m_ScaleFactor = 1.0f;
            } else {
                object->transform = m_OriginalTransform;
            }
        }
        ClearNumericInput();
    }

    m_Axis = (m_Axis == axis) ? AxisConstraint::None : axis;
}

bool TransformTool::CanAppendNumericChar(char c) const {
    if (c == '-') {
        return m_NumericInput.empty() && m_Mode == TransformMode::Translate;
    }
    if (c == '.') {
        return m_NumericInput.find('.') == std::string::npos;
    }
    return c >= '0' && c <= '9';
}

bool TransformTool::AppendNumericChar(char c, Scene& scene) {
    if (!m_TargetId.has_value()) {
        return false;
    }

    if (m_Mode == TransformMode::Translate) {
        if (m_Axis == AxisConstraint::None) {
            return false;
        }
    } else if (m_Mode == TransformMode::Scale) {
        // Exact uniform scale factor (screen curve).
        if (!m_ScreenCurve) {
            return false;
        }
    } else {
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
    if ((m_Mode != TransformMode::Translate && m_Mode != TransformMode::Scale) ||
        !IsNumericInputActive()) {
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

    if (m_ScreenCurve) {
        object->controlPoints = m_OriginalControlPoints;
        m_AccumulatedTranslation = {0.0f, 0.0f};
        m_ScaleFactor = 1.0f;

        if (m_NumericInput.empty() || m_NumericInput == "-" || m_NumericInput == "." ||
            m_NumericInput == "-.") {
            return;
        }

        float value = 0.0f;
        const char* begin = m_NumericInput.data();
        const char* end = begin + m_NumericInput.size();
        const auto result = std::from_chars(begin, end, value);
        if (result.ec != std::errc{} || result.ptr != end) {
            return;
        }

        if (m_Mode == TransformMode::Translate) {
            const float w = static_cast<float>(std::max(m_LastWidth, 1));
            const float h = static_cast<float>(std::max(m_LastHeight, 1));
            switch (m_Axis) {
            case AxisConstraint::X:
                m_AccumulatedTranslation.x = value / w;
                break;
            case AxisConstraint::Y:
                m_AccumulatedTranslation.y = value / h;
                break;
            case AxisConstraint::Z:
            case AxisConstraint::None:
            default:
                return;
            }
            ApplyScreenCurveTranslation(*object);
            return;
        }

        if (m_Mode == TransformMode::Scale) {
            m_ScaleFactor = std::max(0.05f, value);
            ApplyScreenCurveScale(*object);
        }
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

    const char* modeLabel = "Translate mode";
    switch (m_Mode) {
    case TransformMode::Rotate: modeLabel = "Rotate mode"; break;
    case TransformMode::Scale: modeLabel = "Scale mode"; break;
    case TransformMode::Translate:
    case TransformMode::None:
    default:
        break;
    }

    const char* axisLabel = "Free";
    switch (m_Axis) {
    case AxisConstraint::X: axisLabel = "X"; break;
    case AxisConstraint::Y: axisLabel = "Y"; break;
    case AxisConstraint::Z: axisLabel = "Z"; break;
    case AxisConstraint::None: default: break;
    }

    const bool showNumericHint =
        (m_Mode == TransformMode::Translate && m_Axis != AxisConstraint::None) ||
        (m_Mode == TransformMode::Scale && m_ScreenCurve);

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
        if (m_ScreenCurve) {
            ImGui::Text("%s (screen XY)", modeLabel);
        } else {
            ImGui::TextUnformatted(modeLabel);
        }
        if (m_ScreenCurve) {
            ImGui::Text("Axis: %s  |  Shift: precision", axisLabel);
        } else {
            ImGui::Text("Axis: %s  |  Shift: precision", axisLabel);
        }
        if (showNumericHint) {
            if (IsNumericInputActive()) {
                if (m_Mode == TransformMode::Scale) {
                    ImGui::Text("Scale: %s", m_NumericInput.c_str());
                } else if (m_ScreenCurve) {
                    ImGui::Text("Offset: %s px", m_NumericInput.c_str());
                } else {
                    ImGui::Text("Offset: %s", m_NumericInput.c_str());
                }
            } else if (m_Mode == TransformMode::Scale) {
                ImGui::TextUnformatted("Type a scale factor (e.g. 1.5)");
            } else {
                ImGui::TextUnformatted(
                    m_ScreenCurve ? "Type a pixel offset" : "Type a number for exact offset");
            }
        }
        if (m_ScreenCurve) {
            ImGui::TextUnformatted("LMB/Enter: confirm   Esc/RMB: cancel   X/Y: constrain");
        } else {
            ImGui::TextUnformatted("LMB/Enter: confirm   Esc/RMB: cancel   X/Y/Z: constrain");
        }
    }
    ImGui::End();
}
