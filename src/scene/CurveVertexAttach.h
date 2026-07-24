#pragma once

#include "scene/Scene.h"
#include <glm/glm.hpp>

namespace CurveVertexAttach {

bool ProjectWorldToScreenNorm(
    const glm::vec3& world,
    const glm::mat4& view,
    const glm::mat4& projection,
    glm::vec2& outNorm);

bool ComputeMeshCentroidWorld(const SceneObject& mesh, glm::vec3& outWorld);

bool ComputeMeshAnchorWorld(
    const Scene& scene,
    int objectId,
    MeshAnchorKind kind,
    int vertexIndex,
    glm::vec3& outWorld);

bool ProjectMeshAnchor(
    const Scene& scene,
    int objectId,
    MeshAnchorKind kind,
    int vertexIndex,
    const glm::mat4& view,
    const glm::mat4& projection,
    glm::vec2& outNorm);

bool IsMeshAnchorValid(const Scene& scene, int objectId, MeshAnchorKind kind, int vertexIndex);

// Evaluate all Derived Point scripts into derivedWorldPosition.
void EvaluateDerivedPoints(Scene& scene);

// Update all B-spline control/fit points tagged to mesh/derived anchors.
void SyncAttachedControlPoints(
    Scene& scene,
    const glm::mat4& view,
    const glm::mat4& projection);

} // namespace CurveVertexAttach
