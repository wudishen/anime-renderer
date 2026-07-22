#pragma once

#include "scene/Scene.h"
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <vector>

enum class TransformMode {
    None,
    Translate,
    Rotate,
    Scale,
};

enum class AxisConstraint {
    None,
    X,
    Y,
    Z,
};

struct ViewBasis {
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float distance = 3.0f;
};

class TransformTool {
public:
    bool IsActive() const { return m_Mode != TransformMode::None; }
    TransformMode GetMode() const { return m_Mode; }
    AxisConstraint GetAxisConstraint() const { return m_Axis; }
    std::optional<int> GetTargetId() const { return m_TargetId; }
    bool IsNumericInputActive() const { return !m_NumericInput.empty(); }
    bool IsScreenCurveTarget() const { return m_ScreenCurve; }

    void BeginTranslate(int objectId, Scene& scene);
    void BeginRotate(int objectId, Scene& scene);
    void BeginScale(int objectId, Scene& scene);
    // viewportWidth/Height used for screen-space curve annotations (normalized 0..1).
    void Update(
        Scene& scene,
        const ViewBasis& view,
        float mouseDx,
        float mouseDy,
        bool precision,
        int viewportWidth,
        int viewportHeight);
    void Confirm();
    void Cancel(Scene& scene);

    // Pressing the same axis again clears the constraint.
    void ToggleAxisConstraint(AxisConstraint axis, Scene& scene);

    // Typed offset (translate) or scale factor (scale). Returns true if handled.
    bool AppendNumericChar(char c, Scene& scene);
    bool BackspaceNumeric(Scene& scene);

    void DrawStatusUi() const;

private:
    void ClearNumericInput();
    void ApplyNumericOffset(Scene& scene);
    bool CanAppendNumericChar(char c) const;
    void ApplyScreenCurveTranslation(SceneObject& object) const;
    void ApplyScreenCurveScale(SceneObject& object) const;
    bool BeginScreenCurve(int objectId, Scene& scene, TransformMode mode);

    TransformMode m_Mode = TransformMode::None;
    AxisConstraint m_Axis = AxisConstraint::None;
    std::optional<int> m_TargetId;
    glm::mat4 m_OriginalTransform{1.0f};
    std::vector<glm::vec3> m_OriginalControlPoints;
    glm::vec2 m_AccumulatedTranslation{0.0f};
    glm::vec2 m_ScaleCenter{0.0f};
    float m_ScaleFactor = 1.0f;
    bool m_ScreenCurve = false;
    int m_LastWidth = 1;
    int m_LastHeight = 1;
    std::string m_NumericInput;
};
