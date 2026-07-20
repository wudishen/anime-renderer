#pragma once

#include "LoopBlinnCubic.h"

#include <glm/glm.hpp>
#include <vector>

// Experimental uniform cubic B-spline stroke: converted to joined Loop-Blinn cubics.
// Not a scene object; each overlapping window of 4 B-spline CPs becomes one Bézier segment.
class LoopBlinnBSplineStroke {
public:
    LoopBlinnBSplineStroke() = default;
    ~LoopBlinnBSplineStroke() = default;

    LoopBlinnBSplineStroke(const LoopBlinnBSplineStroke&) = delete;
    LoopBlinnBSplineStroke& operator=(const LoopBlinnBSplineStroke&) = delete;
    LoopBlinnBSplineStroke(LoopBlinnBSplineStroke&&) noexcept = default;
    LoopBlinnBSplineStroke& operator=(LoopBlinnBSplineStroke&&) noexcept = default;

    // Needs at least 4 control points. Returns false if no drawable segments remain.
    bool Build(const std::vector<glm::vec2>& controlPoints, float z = 0.0f);

    bool BuildOnPlane(
        const std::vector<glm::vec2>& controlPoints,
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
