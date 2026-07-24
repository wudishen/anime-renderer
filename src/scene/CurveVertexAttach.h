#pragma once

#include "scene/Scene.h"
#include <glm/glm.hpp>
#include <optional>

namespace CurveVertexAttach {

// Project a world-space point to normalized screen coords (0,0)=top-left .. (1,1)=bottom-right.
// Returns false if the point is behind the camera (w <= 0).
bool ProjectWorldToScreenNorm(
    const glm::vec3& world,
    const glm::mat4& view,
    const glm::mat4& projection,
    glm::vec2& outNorm);

// Local-space average of mesh vertices, then transformed to world.
bool ComputeMeshCentroidWorld(const SceneObject& mesh, glm::vec3& outWorld);

// Resolve a mesh attachment target to world space.
bool ComputeMeshAnchorWorld(
    const SceneObject& mesh,
    MeshAnchorKind kind,
    int vertexIndex,
    glm::vec3& outWorld);

// Project an attachment target to normalized screen space.
bool ProjectMeshAnchor(
    const Scene& scene,
    int meshObjectId,
    MeshAnchorKind kind,
    int vertexIndex,
    const glm::mat4& view,
    const glm::mat4& projection,
    glm::vec2& outNorm);

// True if the mesh object can still resolve this attachment.
bool IsMeshAnchorValid(const Scene& scene, int meshObjectId, MeshAnchorKind kind, int vertexIndex);

// Update all B-spline control/fit points tagged to mesh anchors.
void SyncAttachedControlPoints(
    Scene& scene,
    const glm::mat4& view,
    const glm::mat4& projection);

} // namespace CurveVertexAttach
