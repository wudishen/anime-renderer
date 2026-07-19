#include "Picking.h"

#include <limits>

namespace Picking {
namespace {

bool RayTriangleIntersect(
    const glm::vec3& origin,
    const glm::vec3& dir,
    const glm::vec3& v0,
    const glm::vec3& v1,
    const glm::vec3& v2,
    float& outT) {
    constexpr float kEpsilon = 1e-6f;
    const glm::vec3 edge1 = v1 - v0;
    const glm::vec3 edge2 = v2 - v0;
    const glm::vec3 pvec = glm::cross(dir, edge2);
    const float det = glm::dot(edge1, pvec);
    if (det > -kEpsilon && det < kEpsilon) {
        return false;
    }

    const float invDet = 1.0f / det;
    const glm::vec3 tvec = origin - v0;
    const float u = glm::dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    const glm::vec3 qvec = glm::cross(tvec, edge1);
    const float v = glm::dot(dir, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    const float t = glm::dot(edge2, qvec) * invDet;
    if (t < kEpsilon) {
        return false;
    }

    outT = t;
    return true;
}

} // namespace

std::optional<int> PickObject(
    const Scene& scene,
    float mouseX,
    float mouseY,
    int viewportWidth,
    int viewportHeight,
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& cameraPosition) {
    if (viewportWidth <= 0 || viewportHeight <= 0) {
        return std::nullopt;
    }

    const float ndcX = (2.0f * mouseX) / static_cast<float>(viewportWidth) - 1.0f;
    const float ndcY = 1.0f - (2.0f * mouseY) / static_cast<float>(viewportHeight);

    const glm::mat4 invVP = glm::inverse(projection * view);
    glm::vec4 nearPoint = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    const glm::vec3 origin = cameraPosition;
    const glm::vec3 dir = glm::normalize(glm::vec3(farPoint) - glm::vec3(nearPoint));

    std::optional<int> bestId;
    float bestT = std::numeric_limits<float>::max();

    for (const SceneObject& object : scene.GetObjects()) {
        // Cameras are viewport bookmarks — select them from the Objects list only.
        if (object.IsCamera() || object.vertices.size() < 3) {
            continue;
        }

        for (size_t i = 0; i + 2 < object.vertices.size(); i += 3) {
            const glm::vec3 v0 = glm::vec3(object.transform * glm::vec4(object.vertices[i], 1.0f));
            const glm::vec3 v1 = glm::vec3(object.transform * glm::vec4(object.vertices[i + 1], 1.0f));
            const glm::vec3 v2 = glm::vec3(object.transform * glm::vec4(object.vertices[i + 2], 1.0f));

            float t = 0.0f;
            if (RayTriangleIntersect(origin, dir, v0, v1, v2, t) && t < bestT) {
                bestT = t;
                bestId = object.id;
            }
        }
    }

    return bestId;
}

} // namespace Picking
