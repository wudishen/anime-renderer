#pragma once

#include "SceneObject.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace CameraFactory {

enum class Preset {
    Free,
    Top,
    Bottom,
    Left,
    Right,
    Front,
    Back,
};

SceneObject Create(Preset preset, float distance = 5.0f);
SceneObject CreateLookAt(
    const std::string& name,
    const glm::vec3& eye,
    const glm::vec3& target,
    const glm::vec3& up);

// Shared camera viewport gizmo used when loading saved cameras.
std::vector<glm::vec3> GizmoVertices();

} // namespace CameraFactory
