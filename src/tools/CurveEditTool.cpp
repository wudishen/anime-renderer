#include "CurveEditTool.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <charconv>

void CurveEditTool::Begin(int objectId, Scene& scene) {
    SceneObject* object = scene.FindById(objectId);
    if (!object || !object->IsCurve() || object->controlPoints.empty()) {
        return;
    }

    m_Active = true;
    m_Translating = false;
    m_Axis = AxisConstraint::None;
    m_TargetId = objectId;
    m_SelectedPoint.reset();
    ClearNumericInput();
    scene.SetSelectedId(objectId);
}

void CurveEditTool::End() {
    m_Active = false;
    m_Translating = false;
    m_Axis = AxisConstraint::None;
    m_TargetId.reset();
    m_SelectedPoint.reset();
    ClearNumericInput();
}

void CurveEditTool::CancelTranslate(Scene& scene) {
    if (m_Translating) {
        if (glm::vec3* point = GetSelectedControlPoint(scene)) {
            *point = m_OriginalPoint;
        }
    }
    m_Translating = false;
    m_Axis = AxisConstraint::None;
    ClearNumericInput();
}

bool CurveEditTool::BeginTranslatePoint(int pointIndex, Scene& scene) {
    if (!m_Active || !m_TargetId.has_value()) {
        return false;
    }

    SceneObject* object = scene.FindById(*m_TargetId);
    if (!object || pointIndex < 0 ||
        pointIndex >= static_cast<int>(object->controlPoints.size())) {
        return false;
    }

    if (m_Translating) {
        CancelTranslate(scene);
    }

    m_SelectedPoint = pointIndex;
    m_OriginalPoint = object->controlPoints[static_cast<size_t>(pointIndex)];
    m_Translating = true;
    m_Axis = AxisConstraint::None;
    ClearNumericInput();
    return true;
}

glm::vec3* CurveEditTool::GetSelectedControlPoint(Scene& scene) {
    if (!m_TargetId.has_value() || !m_SelectedPoint.has_value()) {
        return nullptr;
    }
    SceneObject* object = scene.FindById(*m_TargetId);
    if (!object) {
        return nullptr;
    }
    const int index = *m_SelectedPoint;
    if (index < 0 || index >= static_cast<int>(object->controlPoints.size())) {
        return nullptr;
    }
    return &object->controlPoints[static_cast<size_t>(index)];
}

void CurveEditTool::Update(
    Scene& scene,
    float mouseDx,
    float mouseDy,
    int viewportWidth,
    int viewportHeight,
    bool precision) {
    if (!m_Active || !m_Translating || IsNumericInputActive()) {
        return;
    }

    glm::vec3* point = GetSelectedControlPoint(scene);
    if (!point) {
        CancelTranslate(scene);
        return;
    }

    if (viewportWidth <= 0 || viewportHeight <= 0) {
        return;
    }
    m_LastWidth = viewportWidth;
    m_LastHeight = viewportHeight;

    if (std::fabs(mouseDx) < 1e-6f && std::fabs(mouseDy) < 1e-6f) {
        return;
    }

    const float scale = precision ? 0.1f : 1.0f;
    // Normalized screen space: +X right, +Y down (matches top-left origin).
    const float dxNorm = (mouseDx * scale) / static_cast<float>(viewportWidth);
    const float dyNorm = (mouseDy * scale) / static_cast<float>(viewportHeight);

    switch (m_Axis) {
    case AxisConstraint::X:
        point->x += dxNorm;
        break;
    case AxisConstraint::Y:
        point->y += dyNorm;
        break;
    case AxisConstraint::Z:
        return;
    case AxisConstraint::None:
    default:
        point->x += dxNorm;
        point->y += dyNorm;
        break;
    }
    point->z = 0.0f;
}

void CurveEditTool::ConfirmTranslate() {
    m_Translating = false;
    m_Axis = AxisConstraint::None;
    ClearNumericInput();
}

void CurveEditTool::ToggleAxisConstraint(AxisConstraint axis, Scene& scene) {
    if (!m_Active || !m_Translating) {
        return;
    }

    if (axis != AxisConstraint::X && axis != AxisConstraint::Y) {
        return;
    }

    if (IsNumericInputActive()) {
        if (glm::vec3* point = GetSelectedControlPoint(scene)) {
            *point = m_OriginalPoint;
        }
        ClearNumericInput();
    }

    m_Axis = (m_Axis == axis) ? AxisConstraint::None : axis;
}

bool CurveEditTool::CanAppendNumericChar(char c) const {
    if (c == '-') {
        return m_NumericInput.empty();
    }
    if (c == '.') {
        return m_NumericInput.find('.') == std::string::npos;
    }
    return c >= '0' && c <= '9';
}

bool CurveEditTool::AppendNumericChar(char c, Scene& scene) {
    if (!m_Translating || m_Axis == AxisConstraint::None) {
        return false;
    }
    if (!CanAppendNumericChar(c)) {
        return true;
    }

    m_NumericInput.push_back(c);
    ApplyNumericOffset(scene, m_LastWidth, m_LastHeight);
    return true;
}

bool CurveEditTool::BackspaceNumeric(Scene& scene) {
    if (!m_Translating || !IsNumericInputActive()) {
        return false;
    }

    m_NumericInput.pop_back();
    ApplyNumericOffset(scene, m_LastWidth, m_LastHeight);
    return true;
}

void CurveEditTool::ClearNumericInput() {
    m_NumericInput.clear();
}

void CurveEditTool::ApplyNumericOffset(Scene& scene, int viewportWidth, int viewportHeight) {
    glm::vec3* point = GetSelectedControlPoint(scene);
    if (!point) {
        return;
    }

    *point = m_OriginalPoint;

    if (m_NumericInput.empty() || m_NumericInput == "-" || m_NumericInput == "." || m_NumericInput == "-.") {
        return;
    }

    float value = 0.0f;
    const char* begin = m_NumericInput.data();
    const char* end = begin + m_NumericInput.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        return;
    }

    // Typed value is in pixels along the constrained screen axis.
    const float w = static_cast<float>(std::max(viewportWidth, 1));
    const float h = static_cast<float>(std::max(viewportHeight, 1));
    switch (m_Axis) {
    case AxisConstraint::X:
        point->x += value / w;
        break;
    case AxisConstraint::Y:
        point->y += value / h;
        break;
    case AxisConstraint::Z:
    case AxisConstraint::None:
    default:
        break;
    }
    point->z = 0.0f;
}

void CurveEditTool::DrawStatusUi() const {
    if (!m_Active) {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float statusHeight = m_Translating ? 110.0f : 72.0f;
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

    if (ImGui::Begin("##CurveEditStatus", nullptr, flags)) {
        ImGui::TextUnformatted("Edit screen-space curve");
        if (!m_Translating) {
            ImGui::TextUnformatted("Click a control point to move it");
            ImGui::TextUnformatted("Esc: exit edit mode");
        } else {
            const char* axisLabel = "Free (XY)";
            switch (m_Axis) {
            case AxisConstraint::X: axisLabel = "X (pixels)"; break;
            case AxisConstraint::Y: axisLabel = "Y (pixels)"; break;
            case AxisConstraint::Z:
            case AxisConstraint::None: default: break;
            }
            ImGui::Text("Point %d  |  Axis: %s  |  Shift: precision",
                        m_SelectedPoint.has_value() ? *m_SelectedPoint : -1,
                        axisLabel);
            if (m_Axis == AxisConstraint::X || m_Axis == AxisConstraint::Y) {
                if (IsNumericInputActive()) {
                    ImGui::Text("Offset: %s px", m_NumericInput.c_str());
                } else {
                    ImGui::TextUnformatted("Type a pixel offset");
                }
            }
            ImGui::TextUnformatted("LMB/Enter: confirm   Esc/RMB: cancel   X/Y: constrain");
        }
    }
    ImGui::End();
}
