#pragma once

#include <glm/glm.hpp>
#include <vector>

// Uniform cubic B-spline fit (on-curve) points and the inverse CV solve.
// Open: Q_j = (C_j + 4 C_{j+1} + C_{j+2}) / 6,  j = 0 .. n-3  → n-2 fit points, n CVs.
// Closed (periodic): Q_j = (C_j + 4 C_{j+1} + C_{j+2}) / 6 with wrap → n fit points, n CVs.
// Open solve uses natural end conditions; closed solve uses a circulant linear system.
namespace BSplineFit {

std::vector<glm::vec2> FitPointsFromControls(
    const std::vector<glm::vec2>& controls,
    bool closed = false);
std::vector<glm::vec2> FitPointsFromControls(
    const std::vector<glm::vec3>& controls,
    bool closed = false);

// Open: m fit points (>= 2) -> m+2 control points.
// Closed: m fit points (>= 4) -> m control points.
std::vector<glm::vec3> SolveControlsFromFitPoints(
    const std::vector<glm::vec2>& fitPoints,
    bool closed = false);

// Open: segments = controlPoints - 3. Closed: segments = controlPoints (periodic).
int SegmentCount(const std::vector<glm::vec3>& controlPoints, bool closed = false);
bool AddSegment(std::vector<glm::vec3>& controlPoints, bool closed = false);
bool RemoveSegment(std::vector<glm::vec3>& controlPoints, bool closed = false);

} // namespace BSplineFit
