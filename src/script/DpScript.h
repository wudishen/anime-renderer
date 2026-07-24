#pragma once

#include "scene/Scene.h"
#include <glm/glm.hpp>
#include <string>

// DpScript: tiny scripting language for derived world-space points.
//
// Example:
//   dp = (triangle.p1 + triangle.p2 + triangle.p3) / 3
//   return dp
//
// Mesh bindings (by scene object name):
//   name.p1, name.p2, ...   1-based vertex (world space)
//   name.v0, name.v1, ...   0-based vertex (world space)
//   name.centroid           average of all verts (world space)
//   name.count              vertex count (number)
//
// Built-ins: avg(a,b,...), vec(x,y,z), len(v), normalize(v), dot(a,b), cross(a,b)
namespace DpScript {

struct Result {
    bool ok = false;
    glm::vec3 point{0.0f};
    std::string error;
};

// Evaluate script against the current scene. Returns one world-space point.
Result Evaluate(const std::string& source, const Scene& scene);

} // namespace DpScript
