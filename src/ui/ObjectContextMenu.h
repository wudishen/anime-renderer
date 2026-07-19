#pragma once

#include "scene/Scene.h"

namespace ObjectContextMenu {

enum class Action {
    None,
    Delete,
    Translate,
    Scale,
    Rotate,
    GoToView,
};

struct ActionRequest {
    Action action = Action::None;
    int objectId = 0;
};

void Open(int objectId);

// Draws shared menu items and returns the chosen action (if any).
ActionRequest DrawMenuItems(Scene& scene, int objectId);

// Draws the viewport context popup. Returns a requested action for the caller to handle.
ActionRequest Draw(Scene& scene);

} // namespace ObjectContextMenu
