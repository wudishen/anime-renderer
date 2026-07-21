#pragma once

#include "scene/Scene.h"
#include "tools/TransformTool.h"

#include <glm/glm.hpp>
#include <optional>
#include <string>

// Edit-mode tool for screen-space curve control points (normalized 0..1 viewport coords).
class CurveEditTool {
public:
    bool IsActive() const { return m_Active; }
    bool IsTranslating() const { return m_Translating; }
    std::optional<int> GetTargetId() const { return m_TargetId; }
    std::optional<int> GetSelectedPoint() const { return m_SelectedPoint; }
    AxisConstraint GetAxisConstraint() const { return m_Axis; }
    bool IsNumericInputActive() const { return !m_NumericInput.empty(); }

    void Begin(int objectId, Scene& scene);
    void End();
    void CancelTranslate(Scene& scene);

    bool BeginTranslatePoint(int pointIndex, Scene& scene);
    // mouseDx/Dy in pixels; width/height is the framebuffer size.
    void Update(Scene& scene, float mouseDx, float mouseDy, int viewportWidth, int viewportHeight, bool precision);
    void ConfirmTranslate();

    void ToggleAxisConstraint(AxisConstraint axis, Scene& scene);
    bool AppendNumericChar(char c, Scene& scene);
    bool BackspaceNumeric(Scene& scene);

    void DrawStatusUi() const;

private:
    void ClearNumericInput();
    void ApplyNumericOffset(Scene& scene, int viewportWidth, int viewportHeight);
    bool CanAppendNumericChar(char c) const;
    glm::vec3* GetSelectedControlPoint(Scene& scene);

    bool m_Active = false;
    bool m_Translating = false;
    AxisConstraint m_Axis = AxisConstraint::None;
    std::optional<int> m_TargetId;
    std::optional<int> m_SelectedPoint;
    glm::vec3 m_OriginalPoint{0.0f};
    std::string m_NumericInput;
    int m_LastWidth = 1;
    int m_LastHeight = 1;
};
