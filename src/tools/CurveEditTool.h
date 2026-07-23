#pragma once

#include "scene/Scene.h"
#include "tools/TransformTool.h"

#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <vector>

enum class CurvePointKind {
    Control,
    Fit,
};

// Edit-mode tool for screen-space curve points (normalized 0..1 viewport coords).
// Control points: direct CV edit. Fit points (B-spline): on-curve handles; moving one
// re-solves CVs so the curve still passes through every fit point.
// Experimental: tag a control point to a mesh vertex so it follows that vertex on screen.
class CurveEditTool {
public:
    bool IsActive() const { return m_Active; }
    bool IsTranslating() const { return m_Translating; }
    bool IsTaggingVertex() const { return m_TaggingVertex; }
    std::optional<int> GetTargetId() const { return m_TargetId; }
    std::optional<int> GetSelectedPoint() const { return m_SelectedPoint; }
    std::optional<CurvePointKind> GetSelectedPointKind() const {
        return m_SelectedPoint.has_value() ? std::optional<CurvePointKind>(m_SelectedKind) : std::nullopt;
    }
    CurvePointKind GetHandleMode() const { return m_HandleMode; }
    AxisConstraint GetAxisConstraint() const { return m_Axis; }
    bool IsNumericInputActive() const { return !m_NumericInput.empty(); }

    void Begin(int objectId, Scene& scene);
    void End();
    void CancelTranslate(Scene& scene);

    // Toggle control-point vs fit-point editing (B-spline only). Cancels an in-progress drag.
    void ToggleHandleMode(Scene& scene);
    void SetHandleMode(CurvePointKind mode, Scene& scene);

    bool BeginTranslatePoint(CurvePointKind kind, int pointIndex, Scene& scene);
    // mouseDx/Dy in pixels; width/height is the framebuffer size.
    void Update(Scene& scene, float mouseDx, float mouseDy, int viewportWidth, int viewportHeight, bool precision);
    void ConfirmTranslate();

    void ToggleAxisConstraint(AxisConstraint axis, Scene& scene);
    bool AppendNumericChar(char c, Scene& scene);
    bool BackspaceNumeric(Scene& scene);

    bool AddSegment(Scene& scene);
    bool RemoveSegment(Scene& scene);

    // Experimental mesh-vertex tagging (control-point mode only).
    bool BeginTagVertex(Scene& scene);
    void CancelTagVertex();
    bool CompleteTagVertex(int meshObjectId, int vertexIndex, Scene& scene);
    bool UntagSelectedPoint(Scene& scene);

    void DrawStatusUi(Scene& scene);

private:
    void ClearNumericInput();
    void ApplyNumericOffset(Scene& scene, int viewportWidth, int viewportHeight);
    bool CanAppendNumericChar(char c) const;
    glm::vec3* GetSelectedControlPoint(Scene& scene);
    bool ApplyWorkingFitPoints(Scene& scene);
    void RestoreOriginalControls(Scene& scene);
    void ClearPointSelection();

    bool m_Active = false;
    bool m_Translating = false;
    bool m_TaggingVertex = false;
    AxisConstraint m_Axis = AxisConstraint::None;
    std::optional<int> m_TargetId;
    std::optional<int> m_SelectedPoint;
    CurvePointKind m_SelectedKind = CurvePointKind::Control;
    CurvePointKind m_HandleMode = CurvePointKind::Control;
    glm::vec3 m_OriginalPoint{0.0f};
    glm::vec2 m_OriginalFitPoint{0.0f};
    std::vector<glm::vec3> m_OriginalControls;
    std::vector<glm::vec2> m_WorkingFitPoints;
    std::string m_NumericInput;
    int m_LastWidth = 1;
    int m_LastHeight = 1;
};
