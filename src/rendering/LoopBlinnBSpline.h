#pragma once

#include "LoopBlinnCubic.h"

#include <glm/glm.hpp>
#include <vector>

// Experimental uniform cubic B-spline stroke: converted to joined Loop-Blinn cubics.
// Open: each overlapping window of 4 CPs becomes one Bézier segment (C2 joins).
// Closed: n CPs → n periodic segments with wrap-around indices (C2 all the way around).
class LoopBlinnBSplineStroke {
public:
    LoopBlinnBSplineStroke() = default;
    ~LoopBlinnBSplineStroke() = default;

    LoopBlinnBSplineStroke(const LoopBlinnBSplineStroke&) = delete;
    LoopBlinnBSplineStroke& operator=(const LoopBlinnBSplineStroke&) = delete;
    LoopBlinnBSplineStroke(LoopBlinnBSplineStroke&&) noexcept = default;
    LoopBlinnBSplineStroke& operator=(LoopBlinnBSplineStroke&&) noexcept = default;

    // Needs at least 4 control points. Returns false if no drawable segments remain.
    bool Build(
        const std::vector<glm::vec2>& controlPoints,
        bool closed = false,
        float z = 0.0f);

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
