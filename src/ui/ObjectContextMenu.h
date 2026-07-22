#pragma once

#include "scene/Scene.h"
#include <optional>

namespace ObjectContextMenu {

enum class Action {
    None,
    Delete,
    Translate,
    Scale,
    Rotate,
    GoToView,
    Edit,
};

struct ActionRequest {
    Action action = Action::None;
    int objectId = 0;
};

void Open(int objectId);

// Draws shared menu items and returns the chosen action (if any).
// When curveEditObjectId matches objectId, whole-curve Translate/Scale are disabled.
ActionRequest DrawMenuItems(
    Scene& scene,
    int objectId,
    std::optional<int> curveEditObjectId = std::nullopt);

// Draws the viewport context popup. Returns a requested action for the caller to handle.
ActionRequest Draw(Scene& scene, std::optional<int> curveEditObjectId = std::nullopt);

} // namespace ObjectContextMenu
