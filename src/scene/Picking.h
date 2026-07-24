#pragma once

#include "Scene.h"
#include "tools/CurveEditTool.h"

#include <glm/glm.hpp>
#include <optional>

namespace Picking {

struct CurveHandleHit {
    CurvePointKind kind = CurvePointKind::Control;
    int index = -1;
};

struct MeshAnchorHit {
    int objectId = -1;
    MeshAnchorKind kind = MeshAnchorKind::Vertex;
    int vertexIndex = -1; // used only for Vertex
};

// Returns the closest hit object id under the cursor, or nullopt if none.
// Curves are screen-space annotations; meshes use 3D ray-triangle tests.
std::optional<int> PickObject(
    const Scene& scene,
    float mouseX,
    float mouseY,
    int viewportWidth,
    int viewportHeight,
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& cameraPosition);

// Pick a control point on a screen-space curve. Returns CP index, or nullopt.
std::optional<int> PickControlPoint(
    const SceneObject& object,
    float mouseX,
    float mouseY,
    int viewportWidth,
    int viewportHeight,
    float handleRadiusPixels = 12.0f);

// Pick the closest curve handle (control or B-spline on-curve fit point).
// If kindFilter is set, only that handle kind is considered.
std::optional<CurveHandleHit> PickCurveHandle(
    const SceneObject& object,
    float mouseX,
    float mouseY,
    int viewportWidth,
    int viewportHeight,
    float handleRadiusPixels = 12.0f,
    std::optional<CurvePointKind> kindFilter = std::nullopt);

// Pick the closest mesh anchor (vertex or derived centroid) by screen proximity.
std::optional<MeshAnchorHit> PickMeshAnchor(
    const Scene& scene,
    float mouseX,
    float mouseY,
    int viewportWidth,
    int viewportHeight,
    const glm::mat4& view,
    const glm::mat4& projection,
    float handleRadiusPixels = 14.0f);

} // namespace Picking
