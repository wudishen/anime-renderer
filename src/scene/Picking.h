#pragma once

#include "Scene.h"
#include <glm/glm.hpp>
#include <optional>

namespace Picking {

// Returns the closest hit object id under the cursor, or nullopt if none.
std::optional<int> PickObject(
    const Scene& scene,
    float mouseX,
    float mouseY,
    int viewportWidth,
    int viewportHeight,
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& cameraPosition);

} // namespace Picking
