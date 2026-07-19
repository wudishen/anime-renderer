#pragma once

#include "scene/Scene.h"
#include <glm/glm.hpp>
#include <optional>

enum class TransformMode {
    None,
    Translate,
    Rotate,
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

    void BeginTranslate(int objectId, Scene& scene);
    void BeginRotate(int objectId, Scene& scene);
    void Update(Scene& scene, const ViewBasis& view, float mouseDx, float mouseDy, bool precision);
    void Confirm();
    void Cancel(Scene& scene);

    // Pressing the same axis again clears the constraint.
    void ToggleAxisConstraint(AxisConstraint axis);

    void DrawStatusUi() const;

private:
    TransformMode m_Mode = TransformMode::None;
    AxisConstraint m_Axis = AxisConstraint::None;
    std::optional<int> m_TargetId;
    glm::mat4 m_OriginalTransform{1.0f};
};
