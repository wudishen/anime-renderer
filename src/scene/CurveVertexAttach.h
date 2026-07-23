#pragma once

#include "scene/Scene.h"
#include <glm/glm.hpp>

namespace CurveVertexAttach {

// Project a world-space point to normalized screen coords (0,0)=top-left .. (1,1)=bottom-right.
// Returns false if the point is behind the camera (w <= 0).
bool ProjectWorldToScreenNorm(
    const glm::vec3& world,
    const glm::mat4& view,
    const glm::mat4& projection,
    glm::vec2& outNorm);

// Update all B-spline control points that are tagged to mesh vertices.
void SyncAttachedControlPoints(
    Scene& scene,
    const glm::mat4& view,
    const glm::mat4& projection);

} // namespace CurveVertexAttach
