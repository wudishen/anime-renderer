#pragma once

#include "scene/Scene.h"
#include <optional>

// Edit-mode tool for camera properties (currently vertical field of view).
class CameraEditTool {
public:
    bool IsActive() const { return m_Active; }
    std::optional<int> GetTargetId() const { return m_TargetId; }

    void Begin(int objectId, Scene& scene);
    void End();

    void DrawStatusUi(Scene& scene);

private:
    bool m_Active = false;
    std::optional<int> m_TargetId;
};
