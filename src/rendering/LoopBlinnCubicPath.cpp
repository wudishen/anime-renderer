#include "LoopBlinnCubicPath.h"

#include <array>

bool LoopBlinnCubicPathStroke::Build(
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

bool LoopBlinnCubicPathStroke::BuildOnPlane(
    const std::vector<glm::vec2>& controlPoints,
    bool closed,
    const glm::vec3& origin,
    const glm::vec3& axisU,
    const glm::vec3& axisV) {
    Destroy();

    if (closed) {
        // n segments share endpoints: 3n control points, n >= 2.
        if (controlPoints.size() < 6 || (controlPoints.size() % 3) != 0) {
            return false;
        }
        const size_t segmentCount = controlPoints.size() / 3;
        m_Segments.reserve(segmentCount);
        for (size_t i = 0; i < segmentCount; ++i) {
            const size_t i0 = 3 * i;
            const size_t i1 = 3 * i + 1;
            const size_t i2 = 3 * i + 2;
            const size_t i3 = 3 * ((i + 1) % segmentCount);
            const std::array<glm::vec2, 4> bezier{{
                controlPoints[i0],
                controlPoints[i1],
                controlPoints[i2],
                controlPoints[i3],
            }};
            LoopBlinnCubicStroke segment;
            if (segment.BuildOnPlane(bezier, origin, axisU, axisV)) {
                m_Segments.push_back(std::move(segment));
            }
        }
    } else {
        if (controlPoints.size() < 4) {
            return false;
        }
        const std::array<glm::vec2, 4> bezier{{
            controlPoints[0],
            controlPoints[1],
            controlPoints[2],
            controlPoints[3],
        }};
        LoopBlinnCubicStroke segment;
        if (segment.BuildOnPlane(bezier, origin, axisU, axisV)) {
            m_Segments.push_back(std::move(segment));
        }
    }

    return !m_Segments.empty();
}

void LoopBlinnCubicPathStroke::Destroy() {
    m_Segments.clear();
}

void LoopBlinnCubicPathStroke::Draw() const {
    for (const LoopBlinnCubicStroke& segment : m_Segments) {
        segment.Draw();
    }
}
