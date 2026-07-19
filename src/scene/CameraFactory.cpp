#include "CameraFactory.h"

#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace CameraFactory {
namespace {

std::vector<glm::vec3> BuildCameraGizmoVertices() {
    // Simple camera body + frustum pyramid in local space (looking down -Z).
    const glm::vec3 bodyMin(-0.15f, -0.1f, -0.1f);
    const glm::vec3 bodyMax(0.15f, 0.1f, 0.15f);

    auto quad = [](std::vector<glm::vec3>& out, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
        out.push_back(a);
        out.push_back(b);
        out.push_back(c);
        out.push_back(a);
        out.push_back(c);
        out.push_back(d);
    };

    std::vector<glm::vec3> vertices;
    // Body box
    const glm::vec3 p000(bodyMin.x, bodyMin.y, bodyMin.z);
    const glm::vec3 p001(bodyMin.x, bodyMin.y, bodyMax.z);
    const glm::vec3 p010(bodyMin.x, bodyMax.y, bodyMin.z);
    const glm::vec3 p011(bodyMin.x, bodyMax.y, bodyMax.z);
    const glm::vec3 p100(bodyMax.x, bodyMin.y, bodyMin.z);
    const glm::vec3 p101(bodyMax.x, bodyMin.y, bodyMax.z);
    const glm::vec3 p110(bodyMax.x, bodyMax.y, bodyMin.z);
    const glm::vec3 p111(bodyMax.x, bodyMax.y, bodyMax.z);

    quad(vertices, p001, p101, p111, p011); // +Z back
    quad(vertices, p000, p010, p110, p100); // -Z front of body
    quad(vertices, p000, p001, p011, p010); // -X
    quad(vertices, p100, p110, p111, p101); // +X
    quad(vertices, p010, p011, p111, p110); // +Y
    quad(vertices, p000, p100, p101, p001); // -Y

    // Frustum pointing toward -Z
    const glm::vec3 tip(0.0f, 0.0f, -0.55f);
    const glm::vec3 f0(-0.22f, -0.16f, -0.1f);
    const glm::vec3 f1(0.22f, -0.16f, -0.1f);
    const glm::vec3 f2(0.22f, 0.16f, -0.1f);
    const glm::vec3 f3(-0.22f, 0.16f, -0.1f);
    vertices.insert(vertices.end(), {tip, f0, f1, tip, f1, f2, tip, f2, f3, tip, f3, f0});

    return vertices;
}

} // namespace

SceneObject CreateLookAt(
    const std::string& name,
    const glm::vec3& eye,
    const glm::vec3& target,
    const glm::vec3& up) {
    SceneObject object;
    object.type = SceneObjectType::Camera;
    object.name = name;
    object.color = {0.35f, 0.75f, 0.95f};
    object.transform = glm::inverse(glm::lookAt(eye, target, up));
    object.vertices = GizmoVertices();
    object.UploadMesh();
    return object;
}

std::vector<glm::vec3> GizmoVertices() {
    return BuildCameraGizmoVertices();
}

SceneObject Create(Preset preset, float distance) {
    const glm::vec3 origin(0.0f);
    switch (preset) {
    case Preset::Top:
        return CreateLookAt("Top Camera", {0.0f, distance, 0.0f}, origin, {0.0f, 0.0f, -1.0f});
    case Preset::Bottom:
        return CreateLookAt("Bottom Camera", {0.0f, -distance, 0.0f}, origin, {0.0f, 0.0f, 1.0f});
    case Preset::Left:
        return CreateLookAt("Left Camera", {-distance, 0.0f, 0.0f}, origin, {0.0f, 1.0f, 0.0f});
    case Preset::Right:
        return CreateLookAt("Right Camera", {distance, 0.0f, 0.0f}, origin, {0.0f, 1.0f, 0.0f});
    case Preset::Front:
        return CreateLookAt("Front Camera", {0.0f, 0.0f, distance}, origin, {0.0f, 1.0f, 0.0f});
    case Preset::Back:
        return CreateLookAt("Back Camera", {0.0f, 0.0f, -distance}, origin, {0.0f, 1.0f, 0.0f});
    case Preset::Free:
    default:
        // Default perspective looking toward the origin along -Z.
        return CreateLookAt("Camera", {0.0f, 0.0f, distance}, origin, {0.0f, 1.0f, 0.0f});
    }
}

} // namespace CameraFactory
