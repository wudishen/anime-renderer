#include "MeshFactory.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>

namespace MeshFactory {
namespace {

constexpr float kPi = glm::pi<float>();

} // namespace

std::vector<glm::vec3> TriangleVertices() {
    return {
        {-0.5f, -0.5f, 0.0f},
        {0.5f, -0.5f, 0.0f},
        {0.0f, 0.5f, 0.0f},
    };
}

std::vector<glm::vec3> SphereVertices(int slices, int stacks, float radius) {
    slices = std::max(slices, 3);
    stacks = std::max(stacks, 2);

    std::vector<glm::vec3> vertices;
    vertices.reserve(static_cast<size_t>(slices * stacks * 6));

    auto sample = [&](int stack, int slice) {
        const float v = static_cast<float>(stack) / static_cast<float>(stacks);
        const float u = static_cast<float>(slice) / static_cast<float>(slices);
        const float phi = v * kPi;
        const float theta = u * 2.0f * kPi;
        const float sinPhi = std::sin(phi);
        return glm::vec3(
            radius * sinPhi * std::cos(theta),
            radius * std::cos(phi),
            radius * sinPhi * std::sin(theta));
    };

    for (int stack = 0; stack < stacks; ++stack) {
        for (int slice = 0; slice < slices; ++slice) {
            const glm::vec3 p00 = sample(stack, slice);
            const glm::vec3 p10 = sample(stack, slice + 1);
            const glm::vec3 p01 = sample(stack + 1, slice);
            const glm::vec3 p11 = sample(stack + 1, slice + 1);

            if (stack != 0) {
                vertices.push_back(p00);
                vertices.push_back(p01);
                vertices.push_back(p10);
            }
            if (stack != stacks - 1) {
                vertices.push_back(p10);
                vertices.push_back(p01);
                vertices.push_back(p11);
            }
        }
    }

    return vertices;
}

std::vector<glm::vec3> ConeVertices(int slices, float radius, float height) {
    slices = std::max(slices, 3);

    std::vector<glm::vec3> vertices;
    vertices.reserve(static_cast<size_t>(slices * 6));

    const glm::vec3 tip(0.0f, height, 0.0f);
    const glm::vec3 baseCenter(0.0f, 0.0f, 0.0f);

    auto basePoint = [&](int slice) {
        const float u = static_cast<float>(slice) / static_cast<float>(slices);
        const float theta = u * 2.0f * kPi;
        return glm::vec3(radius * std::cos(theta), 0.0f, radius * std::sin(theta));
    };

    for (int slice = 0; slice < slices; ++slice) {
        const glm::vec3 a = basePoint(slice);
        const glm::vec3 b = basePoint(slice + 1);

        // Side
        vertices.push_back(tip);
        vertices.push_back(a);
        vertices.push_back(b);

        // Base (outward -Y)
        vertices.push_back(baseCenter);
        vertices.push_back(b);
        vertices.push_back(a);
    }

    return vertices;
}

SceneObject CreateTriangle() {
    SceneObject object;
    object.name = "triangle";
    object.color = {1.0f, 0.5f, 0.2f};
    object.vertices = TriangleVertices();
    object.UploadMesh();
    return object;
}

SceneObject CreateSphere(int slices, int stacks, float radius) {
    SceneObject object;
    object.name = "sphere";
    object.color = {0.45f, 0.7f, 1.0f};
    object.vertices = SphereVertices(slices, stacks, radius);
    object.UploadMesh();
    return object;
}

SceneObject CreateCone(int slices, float radius, float height) {
    SceneObject object;
    object.name = "cone";
    object.color = {0.7f, 0.9f, 0.4f};
    object.vertices = ConeVertices(slices, radius, height);
    object.UploadMesh();
    return object;
}

} // namespace MeshFactory
