#include "LoopBlinnCubic.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace {

constexpr float kEps = 1e-8f;

float Det3(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    return glm::dot(a, glm::cross(b, c));
}

// Power-basis coeffs from cubic Bernstein control points (homogeneous).
void BezierToPower(const glm::vec3 b[4], glm::vec3 c[4]) {
    c[0] = b[0];
    c[1] = -3.0f * b[0] + 3.0f * b[1];
    c[2] = 3.0f * b[0] - 6.0f * b[1] + 3.0f * b[2];
    c[3] = -b[0] + 3.0f * b[1] - 3.0f * b[2] + b[3];
}

// inv(M3) converts power-basis polynomial coeffs (t^3..s^3 rows) to Bézier coeffs.
glm::mat4 InverseM3() {
    // Row layout:
    // [1  0    0    0]
    // [1  1/3  0    0]
    // [1  2/3  1/3  0]
    // [1  1    1    1]
    return glm::mat4(
        1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f,
        0.0f, 0.0f, 1.0f / 3.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

enum class CubicType {
    Serpentine,
    Loop,
    CuspAtInfinity,
    Quadratic,
    Degenerate,
};

CubicType Classify(float d1, float d2, float d3, float& discrOut) {
    const float absD1 = std::fabs(d1);
    const float absD2 = std::fabs(d2);
    const float absD3 = std::fabs(d3);

    if (absD1 < kEps && absD2 < kEps && absD3 < kEps) {
        discrOut = 0.0f;
        return CubicType::Degenerate;
    }
    if (absD1 < kEps && absD2 < kEps) {
        discrOut = 0.0f;
        return CubicType::Quadratic;
    }
    if (absD1 < kEps) {
        discrOut = 0.0f;
        return CubicType::CuspAtInfinity;
    }

    discrOut = 3.0f * d2 * d2 - 4.0f * d1 * d3;
    if (discrOut > kEps) {
        return CubicType::Serpentine;
    }
    if (discrOut < -kEps) {
        return CubicType::Loop;
    }
    // discr ≈ 0 with d1 ≠ 0: finite cusp; serpentine formulas still apply.
    return CubicType::Serpentine;
}

void ComputeDCoefficients(const glm::vec3 b[4], float& d1, float& d2, float& d3) {
    glm::vec3 c[4];
    BezierToPower(b, c);
    d1 = -Det3(c[0], c[2], c[3]);
    d2 = Det3(c[0], c[1], c[3]);
    d3 = -Det3(c[0], c[1], c[2]);
}

bool ComputeKlm(const glm::vec3 b[4], glm::vec3 klm[4]) {
    float d1 = 0.0f;
    float d2 = 0.0f;
    float d3 = 0.0f;
    ComputeDCoefficients(b, d1, d2, d3);

    float discr = 0.0f;
    const CubicType type = Classify(d1, d2, d3, discr);

    // F columns = power-basis coeffs of (k, l, m, n); rows = t^3, t^2 s, t s^2, s^3.
    glm::mat4 F(0.0f);

    switch (type) {
    case CubicType::Degenerate:
        return false;

    case CubicType::CuspAtInfinity: {
        // d1≈0, d2≠0: cusp at parametric infinity (common for symmetric S-curves).
        const float tl = d3;
        const float sl = 3.0f * d2;
        F[0] = glm::vec4(tl, -sl, 0.0f, 0.0f);
        F[1] = glm::vec4(tl * tl * tl, -3.0f * sl * tl * tl, 3.0f * sl * sl * tl, -sl * sl * sl);
        F[2] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        F[3] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        break;
    }

    case CubicType::Quadratic: {
        // Degree-elevated quadratic coords so cubic shader evaluates k(k^2 - l).
        const float sign = (d3 < 0.0f) ? -1.0f : 1.0f;
        klm[0] = sign * glm::vec3(0.0f, 0.0f, 1.0f);
        klm[1] = sign * glm::vec3(1.0f / 3.0f, 0.0f, 1.0f);
        klm[2] = sign * glm::vec3(2.0f / 3.0f, 1.0f / 3.0f, 1.0f);
        klm[3] = sign * glm::vec3(1.0f, 1.0f, 1.0f);
        return true;
    }

    case CubicType::Serpentine: {
        const float sqrtTerm = std::sqrt(std::max(discr, 0.0f));
        const float invSqrt3 = 1.0f / std::sqrt(3.0f);
        const float tl = d2 + invSqrt3 * sqrtTerm;
        const float sl = 2.0f * d1;
        const float tm = d2 - invSqrt3 * sqrtTerm;
        const float sm = 2.0f * d1;

        F[0] = glm::vec4(tl * tm, -(sm * tl + sl * tm), sl * sm, 0.0f);
        F[1] = glm::vec4(tl * tl * tl, -3.0f * sl * tl * tl, 3.0f * sl * sl * tl, -sl * sl * sl);
        F[2] = glm::vec4(tm * tm * tm, -3.0f * sm * tm * tm, 3.0f * sm * sm * tm, -sm * sm * sm);
        F[3] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        break;
    }

    case CubicType::Loop: {
        const float sqrtTerm = std::sqrt(std::max(-discr, 0.0f));
        const float td = d2 + sqrtTerm;
        const float sd = 2.0f * d1;
        const float te = d2 - sqrtTerm;
        const float se = 2.0f * d1;

        F[0] = glm::vec4(td * te, -(se * td + sd * te), sd * se, 0.0f);
        F[1] = glm::vec4(
            td * td * te,
            -(se * td * td + 2.0f * sd * te * td),
            te * sd * sd + 2.0f * se * td * sd,
            -sd * sd * se);
        F[2] = glm::vec4(
            td * te * te,
            -(sd * te * te + 2.0f * se * td * te),
            td * se * se + 2.0f * sd * te * se,
            -sd * se * se);
        F[3] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        break;
    }
    }

    const glm::mat4 bezierCoeffs = InverseM3() * F;

    for (int i = 0; i < 4; ++i) {
        klm[i] = glm::vec3(bezierCoeffs[0][i], bezierCoeffs[1][i], bezierCoeffs[2][i]);
    }

    if ((type == CubicType::Serpentine || type == CubicType::Loop) && d1 < 0.0f) {
        for (int i = 0; i < 4; ++i) {
            klm[i].x = -klm[i].x;
            klm[i].y = -klm[i].y;
        }
    }

    return true;
}

float Cross2(const glm::vec2& a, const glm::vec2& b) {
    return a.x * b.y - a.y * b.x;
}

// Barycentric (u,v,w) of p w.r.t. triangle a,b,c. Allows values outside [0,1].
bool Barycentric2D(
    const glm::vec2& p,
    const glm::vec2& a,
    const glm::vec2& b,
    const glm::vec2& c,
    float& u,
    float& v,
    float& w) {
    const glm::vec2 v0 = b - a;
    const glm::vec2 v1 = c - a;
    const glm::vec2 v2 = p - a;
    const float d00 = glm::dot(v0, v0);
    const float d01 = glm::dot(v0, v1);
    const float d11 = glm::dot(v1, v1);
    const float d20 = glm::dot(v2, v0);
    const float d21 = glm::dot(v2, v1);
    const float denom = d00 * d11 - d01 * d01;
    if (std::fabs(denom) < kEps) {
        return false;
    }
    v = (d11 * d20 - d01 * d21) / denom;
    w = (d00 * d21 - d01 * d20) / denom;
    u = 1.0f - v - w;
    return true;
}

bool FindKlmBasis(
    const std::array<glm::vec2, 4>& cps,
    int& i0,
    int& i1,
    int& i2) {
    float bestArea = 0.0f;
    bool found = false;
    for (int a = 0; a < 4; ++a) {
        for (int b = a + 1; b < 4; ++b) {
            for (int c = b + 1; c < 4; ++c) {
                const float area = std::fabs(Cross2(cps[b] - cps[a], cps[c] - cps[a]));
                if (area > bestArea) {
                    bestArea = area;
                    i0 = a;
                    i1 = b;
                    i2 = c;
                    found = true;
                }
            }
        }
    }
    return found && bestArea > kEps;
}

// Push each CP away from the centroid and extrapolate KLM so f stays valid outside the hull.
void PadHullWithExtrapolatedKlm(
    const std::array<glm::vec2, 4>& cps,
    const glm::vec3 klmIn[4],
    float pad,
    std::array<glm::vec2, 4>& cpsOut,
    glm::vec3 klmOut[4]) {
    cpsOut = cps;
    for (int i = 0; i < 4; ++i) {
        klmOut[i] = klmIn[i];
    }
    if (pad <= 0.0f) {
        return;
    }

    int i0 = 0;
    int i1 = 1;
    int i2 = 2;
    if (!FindKlmBasis(cps, i0, i1, i2)) {
        return;
    }

    glm::vec2 centroid(0.0f);
    for (int i = 0; i < 4; ++i) {
        centroid += cps[static_cast<size_t>(i)];
    }
    centroid *= 0.25f;

    for (int i = 0; i < 4; ++i) {
        glm::vec2 dir = cps[static_cast<size_t>(i)] - centroid;
        const float len = glm::length(dir);
        if (len > kEps) {
            dir /= len;
        } else {
            // Degenerate toward centroid: push along an edge of the KLM basis.
            dir = cps[static_cast<size_t>(i1)] - cps[static_cast<size_t>(i0)];
            const float edgeLen = glm::length(dir);
            if (edgeLen > kEps) {
                dir = glm::vec2(-dir.y, dir.x) / edgeLen;
            } else {
                dir = glm::vec2(1.0f, 0.0f);
            }
        }

        const glm::vec2 padded = cps[static_cast<size_t>(i)] + dir * pad;
        cpsOut[static_cast<size_t>(i)] = padded;

        float u = 0.0f;
        float v = 0.0f;
        float w = 0.0f;
        if (Barycentric2D(
                padded,
                cps[static_cast<size_t>(i0)],
                cps[static_cast<size_t>(i1)],
                cps[static_cast<size_t>(i2)],
                u,
                v,
                w)) {
            klmOut[i] = u * klmIn[i0] + v * klmIn[i1] + w * klmIn[i2];
        }
    }
}

// Triangulate the convex hull of up to 4 planar control points.
void TriangulateHull(
    const glm::vec3 positions[4],
    const glm::vec3 klm[4],
    std::vector<LoopBlinnVertex>& out) {
    out.clear();

    // Gift-wrap / angular sort around centroid for unique hull verts.
    std::array<int, 4> indices = {0, 1, 2, 3};
    glm::vec2 centroid(0.0f);
    for (int i = 0; i < 4; ++i) {
        centroid += glm::vec2(positions[i]);
    }
    centroid *= 0.25f;

    std::sort(indices.begin(), indices.end(), [&](int ia, int ib) {
        const glm::vec2 a = glm::vec2(positions[ia]) - centroid;
        const glm::vec2 b = glm::vec2(positions[ib]) - centroid;
        const float angA = std::atan2(a.y, a.x);
        const float angB = std::atan2(b.y, b.x);
        if (angA != angB) {
            return angA < angB;
        }
        return glm::dot(a, a) < glm::dot(b, b);
    });

    // Remove near-duplicate consecutive hull points.
    std::vector<int> hull;
    hull.reserve(4);
    for (int idx : indices) {
        if (!hull.empty()) {
            const glm::vec2 prev = glm::vec2(positions[hull.back()]);
            const glm::vec2 cur = glm::vec2(positions[idx]);
            if (glm::dot(cur - prev, cur - prev) < 1e-12f) {
                continue;
            }
        }
        hull.push_back(idx);
    }
    if (hull.size() >= 2) {
        const glm::vec2 first = glm::vec2(positions[hull.front()]);
        const glm::vec2 last = glm::vec2(positions[hull.back()]);
        if (glm::dot(first - last, first - last) < 1e-12f) {
            hull.pop_back();
        }
    }

    // Ensure CCW.
    if (hull.size() >= 3) {
        float area = 0.0f;
        for (size_t i = 0; i < hull.size(); ++i) {
            const glm::vec2 p0 = glm::vec2(positions[hull[i]]);
            const glm::vec2 p1 = glm::vec2(positions[hull[(i + 1) % hull.size()]]);
            area += Cross2(p0, p1);
        }
        if (area < 0.0f) {
            std::reverse(hull.begin(), hull.end());
        }
    }

    if (hull.size() < 3) {
        return;
    }

    auto emit = [&](int i0, int i1, int i2) {
        const float area = Cross2(
            glm::vec2(positions[i1]) - glm::vec2(positions[i0]),
            glm::vec2(positions[i2]) - glm::vec2(positions[i0]));
        if (std::fabs(area) < kEps) {
            return;
        }
        out.push_back({positions[i0], klm[i0]});
        out.push_back({positions[i1], klm[i1]});
        out.push_back({positions[i2], klm[i2]});
    };

    // Fan from first hull vertex.
    for (size_t i = 1; i + 1 < hull.size(); ++i) {
        emit(hull[0], hull[i], hull[i + 1]);
    }
}

} // namespace

LoopBlinnCubicStroke::~LoopBlinnCubicStroke() {
    Destroy();
}

LoopBlinnCubicStroke::LoopBlinnCubicStroke(LoopBlinnCubicStroke&& other) noexcept
    : m_VAO(other.m_VAO), m_VBO(other.m_VBO), m_VertexCount(other.m_VertexCount) {
    other.m_VAO = 0;
    other.m_VBO = 0;
    other.m_VertexCount = 0;
}

LoopBlinnCubicStroke& LoopBlinnCubicStroke::operator=(LoopBlinnCubicStroke&& other) noexcept {
    if (this != &other) {
        Destroy();
        m_VAO = other.m_VAO;
        m_VBO = other.m_VBO;
        m_VertexCount = other.m_VertexCount;
        other.m_VAO = 0;
        other.m_VBO = 0;
        other.m_VertexCount = 0;
    }
    return *this;
}

bool LoopBlinnCubicStroke::Build(
    const std::array<glm::vec2, 4>& controlPoints,
    float z,
    float hullPad) {
    return BuildOnPlane(
        controlPoints,
        glm::vec3(0.0f, 0.0f, z),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        hullPad);
}

bool LoopBlinnCubicStroke::BuildOnPlane(
    const std::array<glm::vec2, 4>& controlPoints,
    const glm::vec3& origin,
    const glm::vec3& axisU,
    const glm::vec3& axisV,
    float hullPad) {
    Destroy();

    glm::vec3 b[4];
    for (int i = 0; i < 4; ++i) {
        b[i] = glm::vec3(controlPoints[i].x, controlPoints[i].y, 1.0f);
    }

    glm::vec3 klm[4];
    if (!ComputeKlm(b, klm)) {
        return false;
    }

    std::array<glm::vec2, 4> paddedCps{};
    glm::vec3 paddedKlm[4];
    PadHullWithExtrapolatedKlm(controlPoints, klm, hullPad, paddedCps, paddedKlm);

    glm::vec3 positions[4];
    for (int i = 0; i < 4; ++i) {
        positions[i] =
            origin + paddedCps[static_cast<size_t>(i)].x * axisU
            + paddedCps[static_cast<size_t>(i)].y * axisV;
    }

    std::vector<LoopBlinnVertex> vertices;
    TriangulateHull(positions, paddedKlm, vertices);
    if (vertices.size() < 3) {
        return false;
    }

    m_VertexCount = static_cast<GLsizei>(vertices.size());

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(LoopBlinnVertex)),
        vertices.data(),
        GL_STATIC_DRAW);

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(LoopBlinnVertex),
        reinterpret_cast<void*>(offsetof(LoopBlinnVertex, position)));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(LoopBlinnVertex),
        reinterpret_cast<void*>(offsetof(LoopBlinnVertex, klm)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return true;
}

void LoopBlinnCubicStroke::Destroy() {
    if (m_VBO != 0) {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }
    m_VertexCount = 0;
}

void LoopBlinnCubicStroke::Draw() const {
    if (!IsValid()) {
        return;
    }
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, m_VertexCount);
    glBindVertexArray(0);
}
