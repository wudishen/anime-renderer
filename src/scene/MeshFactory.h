#pragma once

#include "SceneObject.h"

#include <vector>

namespace MeshFactory {

std::vector<glm::vec3> TriangleVertices();
std::vector<glm::vec3> SphereVertices(int slices = 24, int stacks = 16, float radius = 0.5f);
// Base in XY at y=0, tip at (0, height, 0).
std::vector<glm::vec3> ConeVertices(int slices = 24, float radius = 0.45f, float height = 1.0f);

SceneObject CreateTriangle();
SceneObject CreateSphere(int slices = 24, int stacks = 16, float radius = 0.5f);
SceneObject CreateCone(int slices = 24, float radius = 0.45f, float height = 1.0f);

} // namespace MeshFactory
