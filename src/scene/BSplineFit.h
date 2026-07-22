#pragma once

#include <glm/glm.hpp>
#include <vector>

// Uniform cubic B-spline fit (on-curve) points and the inverse CV solve.
// Fit points lie on the curve at segment joins:
//   Q_j = (C_j + 4 C_{j+1} + C_{j+2}) / 6,  j = 0 .. n-3
// Solving from Q uses natural end conditions so the curve interpolates every Q.
namespace BSplineFit {

std::vector<glm::vec2> FitPointsFromControls(const std::vector<glm::vec2>& controls);
std::vector<glm::vec2> FitPointsFromControls(const std::vector<glm::vec3>& controls);

// m fit points (>= 2) -> m+2 control points (z = 0).
std::vector<glm::vec3> SolveControlsFromFitPoints(const std::vector<glm::vec2>& fitPoints);

// Uniform cubic B-spline: segments = controlPoints - 3 (minimum 1).
int SegmentCount(const std::vector<glm::vec3>& controlPoints);
bool AddSegment(std::vector<glm::vec3>& controlPoints);
bool RemoveSegment(std::vector<glm::vec3>& controlPoints);

} // namespace BSplineFit
