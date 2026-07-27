#include "LoopBlinnBSpline.h"

#include <array>

namespace {

// Uniform cubic B-spline segment (Pi..Pi+3) → cubic Bézier control points.
// Guarantees C2 joins between adjacent segments that share three B-spline CPs.
std::array<glm::vec2, 4> BSplineSegmentToBezier(
    const glm::vec2& p0,
    const glm::vec2& p1,
    const glm::vec2& p2,
    const glm::vec2& p3) {
    return {{
        (p0 + 4.0f * p1 + p2) / 6.0f,
        (4.0f * p1 + 2.0f * p2) / 6.0f,
        (2.0f * p1 + 4.0f * p2) / 6.0f,
        (p1 + 4.0f * p2 + p3) / 6.0f,
    }};
}

} // namespace

bool LoopBlinnBSplineStroke::Build(
    const std::vector<glm::vec2>& controlPoints,
    bool closed,
    float z) {
    return BuildOnPlane(
        controlPoints,
        closed,
        glm::vec3(0.0f, 0.0f, z),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
}

bool LoopBlinnBSplineStroke::BuildOnPlane(
    const std::vector<glm::vec2>& controlPoints,
    bool closed,
    const glm::vec3& origin,
    const glm::vec3& axisU,
    const glm::vec3& axisV) {
    Destroy();

    if (controlPoints.size() < 4) {
        return false;
    }

    const size_t n = controlPoints.size();
    if (closed) {
        m_Segments.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            const std::array<glm::vec2, 4> bezier = BSplineSegmentToBezier(
                controlPoints[i],
                controlPoints[(i + 1) % n],
                controlPoints[(i + 2) % n],
                controlPoints[(i + 3) % n]);

            LoopBlinnCubicStroke segment;
            if (segment.BuildOnPlane(bezier, origin, axisU, axisV)) {
                m_Segments.push_back(std::move(segment));
            }
        }
    } else {
        m_Segments.reserve(n - 3);
        for (size_t i = 0; i + 3 < n; ++i) {
            const std::array<glm::vec2, 4> bezier = BSplineSegmentToBezier(
                controlPoints[i],
                controlPoints[i + 1],
                controlPoints[i + 2],
                controlPoints[i + 3]);

            LoopBlinnCubicStroke segment;
            if (segment.BuildOnPlane(bezier, origin, axisU, axisV)) {
                m_Segments.push_back(std::move(segment));
            }
        }
    }

    return !m_Segments.empty();
}

void LoopBlinnBSplineStroke::Destroy() {
    m_Segments.clear();
}

void LoopBlinnBSplineStroke::Draw() const {
    for (const LoopBlinnCubicStroke& segment : m_Segments) {
        segment.Draw();
    }
}
