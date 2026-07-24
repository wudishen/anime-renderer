#include "CurveEditTool.h"

#include "scene/BSplineFit.h"
#include "scene/CurveVertexAttach.h"

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
    m_TaggingVertex = false;
    m_Axis = AxisConstraint::None;
    m_TargetId = objectId;
    m_SelectedPoint.reset();
    m_SelectedKind = CurvePointKind::Control;
    m_HandleMode = CurvePointKind::Control;
    m_OriginalControls.clear();
    m_WorkingFitPoints.clear();
    ClearNumericInput();
    scene.SetSelectedId(objectId);
}

void CurveEditTool::End() {
    m_Active = false;
    m_Translating = false;
    m_TaggingVertex = false;
    m_Axis = AxisConstraint::None;
    m_TargetId.reset();
    m_SelectedPoint.reset();
    m_SelectedKind = CurvePointKind::Control;
    m_HandleMode = CurvePointKind::Control;
    m_OriginalControls.clear();
    m_WorkingFitPoints.clear();
    ClearNumericInput();
}

void CurveEditTool::RestoreOriginalControls(Scene& scene) {
    if (!m_TargetId.has_value() || m_OriginalControls.empty()) {
        return;
    }
    if (SceneObject* object = scene.FindById(*m_TargetId)) {
        object->controlPoints = m_OriginalControls;
    }
}

bool CurveEditTool::ApplyWorkingFitPoints(Scene& scene) {
    if (!m_TargetId.has_value()) {
        return false;
    }
    SceneObject* object = scene.FindById(*m_TargetId);
    if (!object || !object->IsBSplineCurve()) {
        return false;
    }

    std::vector<glm::vec3> solved = BSplineFit::SolveControlsFromFitPoints(m_WorkingFitPoints);
    if (solved.empty()) {
        return false;
    }
    object->controlPoints = std::move(solved);
    return true;
}

void CurveEditTool::CancelTranslate(Scene& scene) {
    if (m_Translating) {
        if (m_SelectedKind == CurvePointKind::Fit) {
            RestoreOriginalControls(scene);
        } else if (glm::vec3* point = GetSelectedControlPoint(scene)) {
            *point = m_OriginalPoint;
        }
    }
    m_Translating = false;
    m_Axis = AxisConstraint::None;
    m_WorkingFitPoints.clear();
    ClearNumericInput();
}

bool CurveEditTool::BeginTranslatePoint(CurvePointKind kind, int pointIndex, Scene& scene) {
    if (!m_Active || !m_TargetId.has_value()) {
        return false;
    }

    SceneObject* object = scene.FindById(*m_TargetId);
    if (!object) {
        return false;
    }

    if (object->IsBSplineCurve() && kind != m_HandleMode) {
        return false;
    }

    if (m_TaggingVertex) {
        CancelTagVertex();
    }

    if (m_Translating) {
        CancelTranslate(scene);
    }

    if (kind == CurvePointKind::Control) {
        if (pointIndex < 0 || pointIndex >= static_cast<int>(object->controlPoints.size())) {
            return false;
        }
        m_SelectedKind = CurvePointKind::Control;
        m_SelectedPoint = pointIndex;
        m_OriginalPoint = object->controlPoints[static_cast<size_t>(pointIndex)];
        m_OriginalControls.clear();
        m_WorkingFitPoints.clear();

        // Attached CPs follow a mesh vertex — select only (no free drag).
        if (object->IsBSplineCurve() && object->IsControlPointAttached(pointIndex)) {
            m_Translating = false;
            m_Axis = AxisConstraint::None;
            ClearNumericInput();
            return true;
        }
    } else {
        if (!object->IsBSplineCurve()) {
            return false;
        }
        m_WorkingFitPoints = BSplineFit::FitPointsFromControls(object->controlPoints);
        if (pointIndex < 0 || pointIndex >= static_cast<int>(m_WorkingFitPoints.size())) {
            m_WorkingFitPoints.clear();
            return false;
        }
        m_SelectedKind = CurvePointKind::Fit;
        m_SelectedPoint = pointIndex;
        m_OriginalFitPoint = m_WorkingFitPoints[static_cast<size_t>(pointIndex)];
        m_OriginalControls = object->controlPoints;

        // Attached fit points follow a mesh vertex — select only (no free drag).
        if (object->IsFitPointAttached(pointIndex)) {
            m_Translating = false;
            m_Axis = AxisConstraint::None;
            m_WorkingFitPoints.clear();
            m_OriginalControls.clear();
            ClearNumericInput();
            return true;
        }
    }

    m_Translating = true;
    m_Axis = AxisConstraint::None;
    ClearNumericInput();
    return true;
}

glm::vec3* CurveEditTool::GetSelectedControlPoint(Scene& scene) {
    if (!m_TargetId.has_value() || !m_SelectedPoint.has_value() ||
        m_SelectedKind != CurvePointKind::Control) {
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

    auto applyDelta = [&](float& x, float& y) {
        switch (m_Axis) {
        case AxisConstraint::X:
            x += dxNorm;
            break;
        case AxisConstraint::Y:
            y += dyNorm;
            break;
        case AxisConstraint::Z:
            return false;
        case AxisConstraint::None:
        default:
            x += dxNorm;
            y += dyNorm;
            break;
        }
        return true;
    };

    if (m_SelectedKind == CurvePointKind::Fit) {
        if (!m_SelectedPoint.has_value() ||
            *m_SelectedPoint < 0 ||
            *m_SelectedPoint >= static_cast<int>(m_WorkingFitPoints.size())) {
            CancelTranslate(scene);
            return;
        }
        glm::vec2& fit = m_WorkingFitPoints[static_cast<size_t>(*m_SelectedPoint)];
        if (!applyDelta(fit.x, fit.y)) {
            return;
        }
        if (!ApplyWorkingFitPoints(scene)) {
            CancelTranslate(scene);
        }
        return;
    }

    glm::vec3* point = GetSelectedControlPoint(scene);
    if (!point) {
        CancelTranslate(scene);
        return;
    }
    if (!applyDelta(point->x, point->y)) {
        return;
    }
    point->z = 0.0f;
}

void CurveEditTool::ConfirmTranslate() {
    m_Translating = false;
    m_Axis = AxisConstraint::None;
    m_WorkingFitPoints.clear();
    m_OriginalControls.clear();
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
        if (m_SelectedKind == CurvePointKind::Fit) {
            RestoreOriginalControls(scene);
            if (m_SelectedPoint.has_value() &&
                *m_SelectedPoint >= 0 &&
                *m_SelectedPoint < static_cast<int>(m_WorkingFitPoints.size())) {
                m_WorkingFitPoints = BSplineFit::FitPointsFromControls(m_OriginalControls);
            }
        } else if (glm::vec3* point = GetSelectedControlPoint(scene)) {
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
    if (m_NumericInput.empty() || m_NumericInput == "-" || m_NumericInput == "." || m_NumericInput == "-.") {
        if (m_SelectedKind == CurvePointKind::Fit) {
            if (m_SelectedPoint.has_value() &&
                *m_SelectedPoint >= 0 &&
                *m_SelectedPoint < static_cast<int>(m_WorkingFitPoints.size())) {
                m_WorkingFitPoints = BSplineFit::FitPointsFromControls(m_OriginalControls);
                ApplyWorkingFitPoints(scene);
            }
        } else if (glm::vec3* point = GetSelectedControlPoint(scene)) {
            *point = m_OriginalPoint;
        }
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

    if (m_SelectedKind == CurvePointKind::Fit) {
        if (!m_SelectedPoint.has_value() || m_OriginalControls.empty()) {
            return;
        }
        m_WorkingFitPoints = BSplineFit::FitPointsFromControls(m_OriginalControls);
        if (*m_SelectedPoint >= static_cast<int>(m_WorkingFitPoints.size())) {
            return;
        }
        glm::vec2& fit = m_WorkingFitPoints[static_cast<size_t>(*m_SelectedPoint)];
        fit = m_OriginalFitPoint;
        switch (m_Axis) {
        case AxisConstraint::X:
            fit.x += value / w;
            break;
        case AxisConstraint::Y:
            fit.y += value / h;
            break;
        case AxisConstraint::Z:
        case AxisConstraint::None:
        default:
            break;
        }
        ApplyWorkingFitPoints(scene);
        return;
    }

    glm::vec3* point = GetSelectedControlPoint(scene);
    if (!point) {
        return;
    }

    *point = m_OriginalPoint;
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

void CurveEditTool::ClearPointSelection() {
    m_SelectedPoint.reset();
    m_SelectedKind = CurvePointKind::Control;
    m_OriginalControls.clear();
    m_WorkingFitPoints.clear();
}

void CurveEditTool::SetHandleMode(CurvePointKind mode, Scene& scene) {
    if (!m_Active || !m_TargetId.has_value()) {
        return;
    }

    SceneObject* object = scene.FindById(*m_TargetId);
    if (!object) {
        return;
    }

    if (mode == CurvePointKind::Fit && !object->IsBSplineCurve()) {
        return;
    }

    if (m_HandleMode == mode) {
        return;
    }

    if (m_Translating) {
        CancelTranslate(scene);
    }
    CancelTagVertex();

    m_HandleMode = mode;
    ClearPointSelection();
    ClearNumericInput();
}

void CurveEditTool::ToggleHandleMode(Scene& scene) {
    if (!m_Active || !m_TargetId.has_value()) {
        return;
    }
    SceneObject* object = scene.FindById(*m_TargetId);
    if (!object || !object->IsBSplineCurve()) {
        return;
    }
    SetHandleMode(
        m_HandleMode == CurvePointKind::Control ? CurvePointKind::Fit : CurvePointKind::Control,
        scene);
}

bool CurveEditTool::AddSegment(Scene& scene) {
    if (!m_Active || !m_TargetId.has_value()) {
        return false;
    }
    SceneObject* object = scene.FindById(*m_TargetId);
    if (!object || !object->IsBSplineCurve()) {
        return false;
    }
    if (m_Translating) {
        CancelTranslate(scene);
    }
    if (!BSplineFit::AddSegment(object->controlPoints)) {
        return false;
    }
    ClearPointSelection();
    ClearNumericInput();
    return true;
}

bool CurveEditTool::RemoveSegment(Scene& scene) {
    if (!m_Active || !m_TargetId.has_value()) {
        return false;
    }
    SceneObject* object = scene.FindById(*m_TargetId);
    if (!object || !object->IsBSplineCurve()) {
        return false;
    }
    if (m_Translating) {
        CancelTranslate(scene);
    }
    if (!BSplineFit::RemoveSegment(object->controlPoints)) {
        return false;
    }
    object->PruneAllPointAttachments();
    ClearPointSelection();
    ClearNumericInput();
    return true;
}

bool CurveEditTool::BeginTagVertex(Scene& scene) {
    if (!m_Active || !m_TargetId.has_value()) {
        return false;
    }
    SceneObject* object = scene.FindById(*m_TargetId);
    if (!object || !object->IsBSplineCurve()) {
        return false;
    }
    if (!m_SelectedPoint.has_value() || m_SelectedKind != m_HandleMode) {
        return false;
    }
    if (m_HandleMode == CurvePointKind::Control) {
        if (*m_SelectedPoint < 0 ||
            *m_SelectedPoint >= static_cast<int>(object->controlPoints.size())) {
            return false;
        }
    } else {
        if (*m_SelectedPoint < 0 || *m_SelectedPoint >= object->FitPointCount()) {
            return false;
        }
    }
    if (m_Translating) {
        CancelTranslate(scene);
    }
    m_TaggingVertex = true;
    ClearNumericInput();
    return true;
}

void CurveEditTool::CancelTagVertex() {
    m_TaggingVertex = false;
}

bool CurveEditTool::CompleteTagAnchor(
    int meshObjectId,
    MeshAnchorKind anchorKind,
    int vertexIndex,
    Scene& scene) {
    if (!m_TaggingVertex || !m_TargetId.has_value() || !m_SelectedPoint.has_value()) {
        return false;
    }
    SceneObject* curve = scene.FindById(*m_TargetId);
    if (!curve || !curve->IsBSplineCurve()) {
        return false;
    }
    if (!CurveVertexAttach::IsMeshAnchorValid(scene, meshObjectId, anchorKind, vertexIndex)) {
        return false;
    }

    if (m_SelectedKind == CurvePointKind::Control) {
        if (*m_SelectedPoint < 0 ||
            *m_SelectedPoint >= static_cast<int>(curve->controlPoints.size())) {
            return false;
        }
        curve->SetControlPointAttachment(*m_SelectedPoint, meshObjectId, anchorKind, vertexIndex);
    } else {
        if (*m_SelectedPoint < 0 || *m_SelectedPoint >= curve->FitPointCount()) {
            return false;
        }
        curve->SetFitPointAttachment(*m_SelectedPoint, meshObjectId, anchorKind, vertexIndex);
    }
    m_TaggingVertex = false;
    return true;
}

bool CurveEditTool::UntagSelectedPoint(Scene& scene) {
    if (!m_Active || !m_TargetId.has_value() || !m_SelectedPoint.has_value()) {
        return false;
    }
    SceneObject* curve = scene.FindById(*m_TargetId);
    if (!curve || !curve->IsBSplineCurve()) {
        return false;
    }

    const bool attached =
        (m_SelectedKind == CurvePointKind::Control)
            ? curve->IsControlPointAttached(*m_SelectedPoint)
            : curve->IsFitPointAttached(*m_SelectedPoint);
    if (!attached) {
        return false;
    }
    if (m_Translating) {
        CancelTranslate(scene);
    }
    CancelTagVertex();
    if (m_SelectedKind == CurvePointKind::Control) {
        curve->ClearControlPointAttachment(*m_SelectedPoint);
    } else {
        curve->ClearFitPointAttachment(*m_SelectedPoint);
    }
    return true;
}

void CurveEditTool::DrawStatusUi(Scene& scene) {
    if (!m_Active) {
        return;
    }

    SceneObject* object = m_TargetId.has_value() ? scene.FindById(*m_TargetId) : nullptr;
    const bool isBSpline = object && object->IsBSplineCurve();
    const int segments = isBSpline ? BSplineFit::SegmentCount(object->controlPoints) : 0;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float statusHeight =
        m_TaggingVertex ? 148.0f : (m_Translating ? 110.0f : (isBSpline ? 168.0f : 88.0f));
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
        if (isBSpline) {
            ImGui::Text("Segments: %d", segments);
            ImGui::SameLine();
            ImGui::BeginDisabled(segments <= 1);
            if (ImGui::SmallButton("-##SegMinus")) {
                RemoveSegment(scene);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(object->controlPoints.size() >= 64);
            if (ImGui::SmallButton("+##SegPlus")) {
                AddSegment(scene);
            }
            ImGui::EndDisabled();

            const bool controlMode = m_HandleMode == CurvePointKind::Control;
            if (controlMode) {
                ImGui::BeginDisabled(true);
            }
            if (ImGui::SmallButton("Control##HandleControl")) {
                SetHandleMode(CurvePointKind::Control, scene);
            }
            if (controlMode) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            if (!controlMode) {
                ImGui::BeginDisabled(true);
            }
            if (ImGui::SmallButton("Fit##HandleFit")) {
                SetHandleMode(CurvePointKind::Fit, scene);
            }
            if (!controlMode) {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Tab to toggle");

            const bool hasSelection =
                m_SelectedPoint.has_value() && m_SelectedKind == m_HandleMode;
            const bool attached =
                hasSelection &&
                ((m_HandleMode == CurvePointKind::Control)
                     ? object->IsControlPointAttached(*m_SelectedPoint)
                     : object->IsFitPointAttached(*m_SelectedPoint));

            ImGui::BeginDisabled(!hasSelection || m_Translating);
            if (ImGui::SmallButton(m_TaggingVertex ? "Tagging...##TagVtx" : "Tag to Vertex##TagVtx")) {
                BeginTagVertex(scene);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!attached);
            if (ImGui::SmallButton("Untag##UntagVtx")) {
                UntagSelectedPoint(scene);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("T / U");
        }
        if (m_TaggingVertex) {
            const char* kindLabel =
                m_SelectedKind == CurvePointKind::Fit ? "Fit" : "Control";
            ImGui::Text("Tag %s %d: click a mesh anchor",
                        kindLabel,
                        m_SelectedPoint.has_value() ? *m_SelectedPoint : -1);
            ImGui::TextUnformatted("Yellow: vertex   Orange: centroid   Green: scripted");
            ImGui::TextUnformatted("Esc: cancel tagging");
        } else if (!m_Translating) {
            if (isBSpline) {
                if (m_HandleMode == CurvePointKind::Fit) {
                    ImGui::TextUnformatted("Mode: fit points (magenta = tagged)");
                } else {
                    ImGui::TextUnformatted("Mode: control points (magenta = tagged)");
                }
            } else {
                ImGui::TextUnformatted("Click a control point to move it");
            }
            ImGui::TextUnformatted("Click a handle to move it");
            ImGui::TextUnformatted("Esc: exit edit mode");
        } else {
            const char* kindLabel =
                m_SelectedKind == CurvePointKind::Fit ? "Fit" : "Control";
            const char* axisLabel = "Free (XY)";
            switch (m_Axis) {
            case AxisConstraint::X: axisLabel = "X (pixels)"; break;
            case AxisConstraint::Y: axisLabel = "Y (pixels)"; break;
            case AxisConstraint::Z:
            case AxisConstraint::None: default: break;
            }
            ImGui::Text("%s %d  |  Axis: %s  |  Shift: precision",
                        kindLabel,
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
