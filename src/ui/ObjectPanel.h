#pragma once

#include "ObjectContextMenu.h"
#include "scene/Scene.h"
#include <optional>

namespace ObjectPanel {
ObjectContextMenu::ActionRequest Draw(
    Scene& scene,
    std::optional<int> curveEditObjectId = std::nullopt);
} // namespace ObjectPanel
