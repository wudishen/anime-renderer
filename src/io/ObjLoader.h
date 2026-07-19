#pragma once

#include "scene/SceneObject.h"
#include <optional>
#include <string>

namespace ObjLoader {

struct LoadResult {
    std::optional<SceneObject> object;
    std::string error; // empty on success
};

// Loads a Wavefront OBJ into a renderable scene object.
// Positions are centered and uniformly scaled to a convenient size.
LoadResult Load(const std::string& path);

} // namespace ObjLoader
