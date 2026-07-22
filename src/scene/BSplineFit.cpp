#include "BSplineFit.h"

#include <cmath>

namespace BSplineFit {
namespace {

std::vector<float> SolveTridiagonal(
    std::vector<float> a,
    std::vector<float> b,
    std::vector<float> c,
    std::vector<float> d) {
    const int n = static_cast<int>(d.size());
    if (n == 0) {
        return {};
    }

    for (int i = 1; i < n; ++i) {
        const float denom = b[i - 1];
        if (std::fabs(denom) < 1e-12f) {
            return {};
        }
        const float w = a[i] / denom;
        b[i] -= w * c[i - 1];
        d[i] -= w * d[i - 1];
    }

    if (std::fabs(b[n - 1]) < 1e-12f) {
        return {};
    }

    std::vector<float> x(static_cast<size_t>(n));
    x[static_cast<size_t>(n - 1)] = d[static_cast<size_t>(n - 1)] / b[static_cast<size_t>(n - 1)];
    for (int i = n - 2; i >= 0; --i) {
        x[static_cast<size_t>(i)] =
            (d[static_cast<size_t>(i)] - c[static_cast<size_t>(i)] * x[static_cast<size_t>(i + 1)]) /
            b[static_cast<size_t>(i)];
    }
    return x;
}

} // namespace

std::vector<glm::vec2> FitPointsFromControls(const std::vector<glm::vec2>& controls) {
    std::vector<glm::vec2> fit;
    if (controls.size() < 4) {
        return fit;
    }
    fit.reserve(controls.size() - 2);
    for (size_t j = 0; j + 2 < controls.size(); ++j) {
        fit.push_back((controls[j] + 4.0f * controls[j + 1] + controls[j + 2]) / 6.0f);
    }
    return fit;
}

std::vector<glm::vec2> FitPointsFromControls(const std::vector<glm::vec3>& controls) {
    std::vector<glm::vec2> as2d;
    as2d.reserve(controls.size());
    for (const glm::vec3& c : controls) {
        as2d.emplace_back(c.x, c.y);
    }
    return FitPointsFromControls(as2d);
}

std::vector<glm::vec3> SolveControlsFromFitPoints(const std::vector<glm::vec2>& fitPoints) {
    const int m = static_cast<int>(fitPoints.size());
    if (m < 2) {
        return {};
    }

    const int n = m + 2;
    std::vector<glm::vec3> controls(static_cast<size_t>(n), glm::vec3(0.0f));
    controls[1] = glm::vec3(fitPoints[0].x, fitPoints[0].y, 0.0f);
    controls[static_cast<size_t>(m)] =
        glm::vec3(fitPoints[static_cast<size_t>(m - 1)].x, fitPoints[static_cast<size_t>(m - 1)].y, 0.0f);

    const int unk = m - 2;
    if (unk == 0) {
        controls[2] = controls[static_cast<size_t>(m)];
        controls[0] = 2.0f * controls[1] - controls[2];
        controls[3] = 2.0f * controls[2] - controls[1];
        return controls;
    }

    auto solveComponent = [&](int component) {
        std::vector<float> a(static_cast<size_t>(unk), 0.0f);
        std::vector<float> b(static_cast<size_t>(unk), 4.0f);
        std::vector<float> c(static_cast<size_t>(unk), 0.0f);
        std::vector<float> d(static_cast<size_t>(unk), 0.0f);

        for (int e = 0; e < unk; ++e) {
            const int i = e + 1; // fit index: C[i] + 4 C[i+1] + C[i+2] = 6 Q[i]
            d[static_cast<size_t>(e)] = 6.0f * fitPoints[static_cast<size_t>(i)][component];

            if (i == 1) {
                d[static_cast<size_t>(e)] -= controls[1][component];
            } else {
                a[static_cast<size_t>(e)] = 1.0f;
            }

            if (i == m - 2) {
                d[static_cast<size_t>(e)] -= controls[static_cast<size_t>(m)][component];
            } else {
                c[static_cast<size_t>(e)] = 1.0f;
            }
        }

        const std::vector<float> x = SolveTridiagonal(std::move(a), std::move(b), std::move(c), std::move(d));
        if (static_cast<int>(x.size()) != unk) {
            return false;
        }
        for (int j = 0; j < unk; ++j) {
            controls[static_cast<size_t>(j + 2)][component] = x[static_cast<size_t>(j)];
        }
        return true;
    };

    if (!solveComponent(0) || !solveComponent(1)) {
        return {};
    }

    controls[0] = 2.0f * controls[1] - controls[2];
    controls[static_cast<size_t>(n - 1)] =
        2.0f * controls[static_cast<size_t>(m)] - controls[static_cast<size_t>(m - 1)];
    return controls;
}

int SegmentCount(const std::vector<glm::vec3>& controlPoints) {
    if (controlPoints.size() < 4) {
        return 0;
    }
    return static_cast<int>(controlPoints.size()) - 3;
}

bool AddSegment(std::vector<glm::vec3>& controlPoints) {
    constexpr size_t kMaxControlPoints = 64;
    if (controlPoints.size() < 4 || controlPoints.size() >= kMaxControlPoints) {
        return false;
    }

    const size_t n = controlPoints.size();
    const glm::vec3 next = controlPoints[n - 1] + (controlPoints[n - 1] - controlPoints[n - 2]);
    controlPoints.push_back(glm::vec3(next.x, next.y, 0.0f));
    return true;
}

bool RemoveSegment(std::vector<glm::vec3>& controlPoints) {
    // Keep at least one cubic segment (4 control points).
    if (controlPoints.size() <= 4) {
        return false;
    }
    controlPoints.pop_back();
    return true;
}

} // namespace BSplineFit
