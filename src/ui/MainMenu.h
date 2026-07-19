#pragma once

#include "scene/Scene.h"

struct GLFWwindow;

namespace MainMenu {
void Init(GLFWwindow* window);
void Shutdown();

void BeginFrame();
void Draw(Scene& scene);
void EndFrame();

bool WantCaptureMouse();
bool WantCaptureKeyboard();
} // namespace MainMenu
