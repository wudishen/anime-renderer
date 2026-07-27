#include "BSplineFit.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

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

// Solve A x = b for small dense n (closed B-spline circulant systems).
bool SolveDense(std::vector<std::vector<float>>& a, std::vector<float>& b, std::vector<float>& xOut) {
    const int n = static_cast<int>(b.size());
    if (n == 0 || static_cast<int>(a.size()) != n) {
        return false;
    }
    for (int col = 0; col < n; ++col) {
        int pivot = col;
        float best = std::fabs(a[static_cast<size_t>(col)][static_cast<size_t>(col)]);
        for (int row = col + 1; row < n; ++row) {
            const float v = std::fabs(a[static_cast<size_t>(row)][static_cast<size_t>(col)]);
            if (v > best) {
                best = v;
                pivot = row;
            }
        }
        if (best < 1e-12f) {
            return false;
        }
        if (pivot != col) {
            std::swap(a[static_cast<size_t>(col)], a[static_cast<size_t>(pivot)]);
            std::swap(b[static_cast<size_t>(col)], b[static_cast<size_t>(pivot)]);
        }
        const float diag = a[static_cast<size_t>(col)][static_cast<size_t>(col)];
        for (int row = col + 1; row < n; ++row) {
            const float f = a[static_cast<size_t>(row)][static_cast<size_t>(col)] / diag;
            for (int j = col; j < n; ++j) {
                a[static_cast<size_t>(row)][static_cast<size_t>(j)] -=
                    f * a[static_cast<size_t>(col)][static_cast<size_t>(j)];
            }
            b[static_cast<size_t>(row)] -= f * b[static_cast<size_t>(col)];
        }
    }

    xOut.assign(static_cast<size_t>(n), 0.0f);
    for (int i = n - 1; i >= 0; --i) {
        float sum = b[static_cast<size_t>(i)];
        for (int j = i + 1; j < n; ++j) {
            sum -= a[static_cast<size_t>(i)][static_cast<size_t>(j)] * xOut[static_cast<size_t>(j)];
        }
        const float diag = a[static_cast<size_t>(i)][static_cast<size_t>(i)];
        if (std::fabs(diag) < 1e-12f) {
            return false;
        }
        xOut[static_cast<size_t>(i)] = sum / diag;
    }
    return true;
}

std::vector<glm::vec3> SolveControlsFromFitPointsOpen(const std::vector<glm::vec2>& fitPoints) {
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

std::vector<glm::vec3> SolveControlsFromFitPointsClosed(const std::vector<glm::vec2>& fitPoints) {
    const int n = static_cast<int>(fitPoints.size());
    if (n < 4) {
        return {};
    }

    std::vector<glm::vec3> controls(static_cast<size_t>(n), glm::vec3(0.0f));
    auto solveComponent = [&](int component) {
        std::vector<std::vector<float>> a(
            static_cast<size_t>(n),
            std::vector<float>(static_cast<size_t>(n), 0.0f));
        std::vector<float> b(static_cast<size_t>(n), 0.0f);
        for (int i = 0; i < n; ++i) {
            a[static_cast<size_t>(i)][static_cast<size_t>(i)] = 1.0f;
            a[static_cast<size_t>(i)][static_cast<size_t>((i + 1) % n)] = 4.0f;
            a[static_cast<size_t>(i)][static_cast<size_t>((i + 2) % n)] = 1.0f;
            b[static_cast<size_t>(i)] = 6.0f * fitPoints[static_cast<size_t>(i)][component];
        }
        std::vector<float> x;
        if (!SolveDense(a, b, x)) {
            return false;
        }
        for (int i = 0; i < n; ++i) {
            controls[static_cast<size_t>(i)][component] = x[static_cast<size_t>(i)];
        }
        return true;
    };

    if (!solveComponent(0) || !solveComponent(1)) {
        return {};
    }
    return controls;
}

} // namespace

std::vector<glm::vec2> FitPointsFromControls(const std::vector<glm::vec2>& controls, bool closed) {
    std::vector<glm::vec2> fit;
    if (controls.size() < 4) {
        return fit;
    }

    if (closed) {
        const size_t n = controls.size();
        fit.reserve(n);
        for (size_t j = 0; j < n; ++j) {
            fit.push_back(
                (controls[j] + 4.0f * controls[(j + 1) % n] + controls[(j + 2) % n]) / 6.0f);
        }
        return fit;
    }

    fit.reserve(controls.size() - 2);
    for (size_t j = 0; j + 2 < controls.size(); ++j) {
        fit.push_back((controls[j] + 4.0f * controls[j + 1] + controls[j + 2]) / 6.0f);
    }
    return fit;
}

std::vector<glm::vec2> FitPointsFromControls(const std::vector<glm::vec3>& controls, bool closed) {
    std::vector<glm::vec2> as2d;
    as2d.reserve(controls.size());
    for (const glm::vec3& c : controls) {
        as2d.emplace_back(c.x, c.y);
    }
    return FitPointsFromControls(as2d, closed);
}

std::vector<glm::vec3> SolveControlsFromFitPoints(
    const std::vector<glm::vec2>& fitPoints,
    bool closed) {
    if (closed) {
        return SolveControlsFromFitPointsClosed(fitPoints);
    }
    return SolveControlsFromFitPointsOpen(fitPoints);
}

int SegmentCount(const std::vector<glm::vec3>& controlPoints, bool closed) {
    if (controlPoints.size() < 4) {
        return 0;
    }
    if (closed) {
        return static_cast<int>(controlPoints.size());
    }
    return static_cast<int>(controlPoints.size()) - 3;
}

bool AddSegment(std::vector<glm::vec3>& controlPoints, bool closed) {
    constexpr size_t kMaxControlPoints = 64;
    if (controlPoints.size() < 4 || controlPoints.size() >= kMaxControlPoints) {
        return false;
    }

    const size_t n = controlPoints.size();
    if (closed) {
        // Insert a new CP between the last and first to grow the loop.
        const glm::vec3 mid = 0.5f * (controlPoints[n - 1] + controlPoints[0]);
        controlPoints.push_back(glm::vec3(mid.x, mid.y, 0.0f));
        return true;
    }

    const glm::vec3 next = controlPoints[n - 1] + (controlPoints[n - 1] - controlPoints[n - 2]);
    controlPoints.push_back(glm::vec3(next.x, next.y, 0.0f));
    return true;
}

bool RemoveSegment(std::vector<glm::vec3>& controlPoints, bool closed) {
    // Keep at least one full cubic loop / open cubic (4 control points).
    if (controlPoints.size() <= 4) {
        return false;
    }
    (void)closed;
    controlPoints.pop_back();
    return true;
}

} // namespace BSplineFit
