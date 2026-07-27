#pragma once

#include "LoopBlinnCubic.h"
#include <glm/glm.hpp>
#include <vector>

// One or more cubic Bézier segments as Loop-Blinn strokes.
// Open: 4 control points → one cubic.
// Closed: 3*n control points (n >= 2) → n cubics; segment i uses
//   P[3i], P[3i+1], P[3i+2], P[3*((i+1)%n)] so the path joins back to the start.
class LoopBlinnCubicPathStroke {
public:
    LoopBlinnCubicPathStroke() = default;
    ~LoopBlinnCubicPathStroke() = default;

    LoopBlinnCubicPathStroke(const LoopBlinnCubicPathStroke&) = delete;
    LoopBlinnCubicPathStroke& operator=(const LoopBlinnCubicPathStroke&) = delete;
    LoopBlinnCubicPathStroke(LoopBlinnCubicPathStroke&&) noexcept = default;
    LoopBlinnCubicPathStroke& operator=(LoopBlinnCubicPathStroke&&) noexcept = default;

    bool Build(const std::vector<glm::vec2>& controlPoints, bool closed, float z = 0.0f);

    bool BuildOnPlane(
        const std::vector<glm::vec2>& controlPoints,
        bool closed,
        const glm::vec3& origin,
        const glm::vec3& axisU,
        const glm::vec3& axisV);

    void Destroy();
    void Draw() const;

    bool IsValid() const { return !m_Segments.empty(); }
    size_t GetSegmentCount() const { return m_Segments.size(); }

private:
    std::vector<LoopBlinnCubicStroke> m_Segments;
};
