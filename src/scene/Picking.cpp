#include "Picking.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

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

glm::vec2 NormToPixel(const glm::vec3& cp, int width, int height) {
    return {
        cp.x * static_cast<float>(width),
        cp.y * static_cast<float>(height),
    };
}

glm::vec2 EvalCubicBezier2(const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, float t) {
    const float u = 1.0f - t;
    return (u * u * u) * p0 + (3.0f * u * u * t) * p1 + (3.0f * u * t * t) * p2 + (t * t * t) * p3;
}

glm::vec2 EvalBSplineSegment2(const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, float t) {
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float b0 = (1.0f - 3.0f * t + 3.0f * t2 - t3) / 6.0f;
    const float b1 = (4.0f - 6.0f * t2 + 3.0f * t3) / 6.0f;
    const float b2 = (1.0f + 3.0f * t + 3.0f * t2 - 3.0f * t3) / 6.0f;
    const float b3 = t3 / 6.0f;
    return b0 * p0 + b1 * p1 + b2 * p2 + b3 * p3;
}

float DistPointSegmentSq(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b) {
    const glm::vec2 ab = b - a;
    const float abLenSq = glm::dot(ab, ab);
    if (abLenSq < 1e-12f) {
        const glm::vec2 d = p - a;
        return glm::dot(d, d);
    }
    const float u = std::clamp(glm::dot(p - a, ab) / abLenSq, 0.0f, 1.0f);
    const glm::vec2 closest = a + ab * u;
    const glm::vec2 d = p - closest;
    return glm::dot(d, d);
}

bool PickScreenCurve(
    const SceneObject& object,
    float mouseX,
    float mouseY,
    int width,
    int height,
    float thresholdPixels,
    float& outBestDistSq) {
    if (!object.IsCurve() || object.controlPoints.size() < 2 || width <= 0 || height <= 0) {
        return false;
    }

    const glm::vec2 mouse(mouseX, mouseY);
    const float thresholdSq = thresholdPixels * thresholdPixels;

    std::vector<glm::vec2> samples;
    samples.reserve(64);

    auto pixelCp = [&](size_t i) {
        return NormToPixel(object.controlPoints[i], width, height);
    };

    if (object.IsBezierCurve() && object.controlPoints.size() >= 4) {
        const glm::vec2 p0 = pixelCp(0);
        const glm::vec2 p1 = pixelCp(1);
        const glm::vec2 p2 = pixelCp(2);
        const glm::vec2 p3 = pixelCp(3);
        constexpr int kSteps = 32;
        for (int i = 0; i <= kSteps; ++i) {
            samples.push_back(EvalCubicBezier2(p0, p1, p2, p3, static_cast<float>(i) / kSteps));
        }
    } else if (object.IsBSplineCurve() && object.controlPoints.size() >= 4) {
        constexpr int kSteps = 12;
        for (size_t seg = 0; seg + 3 < object.controlPoints.size(); ++seg) {
            const glm::vec2 p0 = pixelCp(seg);
            const glm::vec2 p1 = pixelCp(seg + 1);
            const glm::vec2 p2 = pixelCp(seg + 2);
            const glm::vec2 p3 = pixelCp(seg + 3);
            for (int i = 0; i <= kSteps; ++i) {
                if (seg > 0 && i == 0) {
                    continue;
                }
                samples.push_back(
                    EvalBSplineSegment2(p0, p1, p2, p3, static_cast<float>(i) / kSteps));
            }
        }
    } else {
        for (size_t i = 0; i < object.controlPoints.size(); ++i) {
            samples.push_back(pixelCp(i));
        }
    }

    bool hit = false;
    for (size_t i = 0; i + 1 < samples.size(); ++i) {
        const float distSq = DistPointSegmentSq(mouse, samples[i], samples[i + 1]);
        if (distSq <= thresholdSq && distSq < outBestDistSq) {
            outBestDistSq = distSq;
            hit = true;
        }
    }
    return hit;
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

    // Screen-space curves first (annotations sit on top of the 3D view).
    std::optional<int> bestCurveId;
    float bestCurveDistSq = std::numeric_limits<float>::max();
    for (const SceneObject& object : scene.GetObjects()) {
        if (!object.IsCurve()) {
            continue;
        }
        if (PickScreenCurve(object, mouseX, mouseY, viewportWidth, viewportHeight, 10.0f, bestCurveDistSq)) {
            bestCurveId = object.id;
        }
    }
    if (bestCurveId.has_value()) {
        return bestCurveId;
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
        if (object.IsCamera() || object.IsCurve() || object.vertices.size() < 3) {
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

std::optional<int> PickControlPoint(
    const SceneObject& object,
    float mouseX,
    float mouseY,
    int viewportWidth,
    int viewportHeight,
    float handleRadiusPixels) {
    if (!object.IsCurve() || object.controlPoints.empty() || viewportWidth <= 0 || viewportHeight <= 0) {
        return std::nullopt;
    }

    const glm::vec2 mouse(mouseX, mouseY);
    const float radiusSq = handleRadiusPixels * handleRadiusPixels;

    std::optional<int> bestIndex;
    float bestDistSq = std::numeric_limits<float>::max();

    for (size_t i = 0; i < object.controlPoints.size(); ++i) {
        const glm::vec2 pixel = NormToPixel(object.controlPoints[i], viewportWidth, viewportHeight);
        const glm::vec2 d = mouse - pixel;
        const float distSq = glm::dot(d, d);
        if (distSq <= radiusSq && distSq < bestDistSq) {
            bestDistSq = distSq;
            bestIndex = static_cast<int>(i);
        }
    }

    return bestIndex;
}

} // namespace Picking
