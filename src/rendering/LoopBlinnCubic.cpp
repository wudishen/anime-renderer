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

CubicType ClassifyCubic2D(const std::array<glm::vec2, 4>& cps) {
    glm::vec3 b[4];
    for (int i = 0; i < 4; ++i) {
        b[i] = glm::vec3(cps[static_cast<size_t>(i)].x, cps[static_cast<size_t>(i)].y, 1.0f);
    }
    float d1 = 0.0f;
    float d2 = 0.0f;
    float d3 = 0.0f;
    ComputeDCoefficients(b, d1, d2, d3);
    float discr = 0.0f;
    return Classify(d1, d2, d3, discr);
}

// de Casteljau split of a cubic at parameter t into left [0,t] and right [t,1].
void SplitCubic(
    const std::array<glm::vec2, 4>& cps,
    float t,
    std::array<glm::vec2, 4>& leftOut,
    std::array<glm::vec2, 4>& rightOut) {
    const glm::vec2& p0 = cps[0];
    const glm::vec2& p1 = cps[1];
    const glm::vec2& p2 = cps[2];
    const glm::vec2& p3 = cps[3];

    const glm::vec2 p01 = glm::mix(p0, p1, t);
    const glm::vec2 p12 = glm::mix(p1, p2, t);
    const glm::vec2 p23 = glm::mix(p2, p3, t);
    const glm::vec2 p012 = glm::mix(p01, p12, t);
    const glm::vec2 p123 = glm::mix(p12, p23, t);
    const glm::vec2 p0123 = glm::mix(p012, p123, t);

    leftOut = {p0, p01, p012, p0123};
    rightOut = {p0123, p123, p23, p3};
}

constexpr int kMaxSubdivideDepth = 2; // was 5 → 32 tiny leaves; 2 → at most 4 leaves

// Recursively split loop-type cubics so each leaf is safe for a single Loop-Blinn stroke.
void CollectDrawableCubics(
    const std::array<glm::vec2, 4>& cps,
    int depth,
    std::vector<std::array<glm::vec2, 4>>& out) {
    const CubicType type = ClassifyCubic2D(cps);
    if (type == CubicType::Loop && depth < kMaxSubdivideDepth) {
        std::array<glm::vec2, 4> left{};
        std::array<glm::vec2, 4> right{};
        SplitCubic(cps, 0.5f, left, right);
        CollectDrawableCubics(left, depth + 1, out);
        CollectDrawableCubics(right, depth + 1, out);
        return;
    }
    out.push_back(cps);
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

glm::vec2 EvalCubic(const std::array<glm::vec2, 4>& cps, float t) {
    const float u = 1.0f - t;
    const float uu = u * u;
    const float tt = t * t;
    return uu * u * cps[0] + 3.0f * uu * t * cps[1] + 3.0f * u * tt * cps[2] + tt * t * cps[3];
}

glm::vec2 EvalCubicDeriv(const std::array<glm::vec2, 4>& cps, float t) {
    const float u = 1.0f - t;
    return 3.0f * u * u * (cps[1] - cps[0])
         + 6.0f * u * t * (cps[2] - cps[1])
         + 3.0f * t * t * (cps[3] - cps[2]);
}

float EvalF(const glm::vec3& klm) {
    return klm.x * klm.x * klm.x - klm.y * klm.z;
}

// Approximate |df/dn| near the curve for stable stroke width.
// Use |f(P+eps*N) - f(P)| / eps (not |f(P+N)| alone): f(P) is often nonzero, and on
// tiny subdivided leaves a fixed 1px probe can underflow both samples to 0.
float EstimateLeafFScale(const std::array<glm::vec2, 4>& cps, const glm::vec3 klm[4]) {
    int i0 = 0;
    int i1 = 1;
    int i2 = 2;
    if (!FindKlmBasis(cps, i0, i1, i2)) {
        return 1e-4f;
    }

    glm::vec2 mn = cps[0];
    glm::vec2 mx = cps[0];
    for (int i = 1; i < 4; ++i) {
        mn = glm::min(mn, cps[static_cast<size_t>(i)]);
        mx = glm::max(mx, cps[static_cast<size_t>(i)]);
    }
    const float bboxDiag = glm::length(mx - mn);

    const glm::vec2 p = EvalCubic(cps, 0.5f);
    glm::vec2 tangent = EvalCubicDeriv(cps, 0.5f);
    const float tLen = glm::length(tangent);
    if (tLen < kEps) {
        tangent = cps[3] - cps[0];
    }
    const float tLen2 = glm::length(tangent);
    if (tLen2 < kEps) {
        return 1e-4f;
    }
    const glm::vec2 normal = glm::vec2(-tangent.y, tangent.x) / tLen2;

    // Keep the probe a small fraction of the leaf so f stays in a locally linear regime.
    float eps = 1.0f;
    if (bboxDiag > kEps) {
        eps = std::clamp(0.05f * bboxDiag, 0.25f, 1.0f);
    }

    auto sampleF = [&](const glm::vec2& q, float& fOut) -> bool {
        float u = 0.0f;
        float v = 0.0f;
        float w = 0.0f;
        if (!Barycentric2D(
                q,
                cps[static_cast<size_t>(i0)],
                cps[static_cast<size_t>(i1)],
                cps[static_cast<size_t>(i2)],
                u,
                v,
                w)) {
            return false;
        }
        const glm::vec3 klmSample = u * klm[i0] + v * klm[i1] + w * klm[i2];
        fOut = EvalF(klmSample);
        return true;
    };

    float fp = 0.0f;
    float fpn = 0.0f;
    if (!sampleF(p, fp) || !sampleF(p + normal * eps, fpn)) {
        return 1e-4f;
    }

    const float scale = std::fabs(fpn - fp) / eps;
    return (scale > 1e-4f) ? scale : 1e-4f;
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
        out.push_back({positions[i0], klm[i0], 1.0f});
        out.push_back({positions[i1], klm[i1], 1.0f});
        out.push_back({positions[i2], klm[i2], 1.0f});
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

    std::vector<std::array<glm::vec2, 4>> leaves;
    CollectDrawableCubics(controlPoints, 0, leaves);
    if (leaves.empty()) {
        return false;
    }

    std::vector<LoopBlinnVertex> vertices;
    vertices.reserve(leaves.size() * 6);

    for (const std::array<glm::vec2, 4>& leaf : leaves) {
        glm::vec3 b[4];
        for (int i = 0; i < 4; ++i) {
            b[i] = glm::vec3(leaf[static_cast<size_t>(i)].x, leaf[static_cast<size_t>(i)].y, 1.0f);
        }

        glm::vec3 klm[4];
        if (!ComputeKlm(b, klm)) {
            continue;
        }

        std::array<glm::vec2, 4> paddedCps{};
        glm::vec3 paddedKlm[4];
        PadHullWithExtrapolatedKlm(leaf, klm, hullPad, paddedCps, paddedKlm);

        const float fScale = EstimateLeafFScale(leaf, klm);

        glm::vec3 positions[4];
        for (int i = 0; i < 4; ++i) {
            positions[i] =
                origin + paddedCps[static_cast<size_t>(i)].x * axisU
                + paddedCps[static_cast<size_t>(i)].y * axisV;
        }

        std::vector<LoopBlinnVertex> leafVerts;
        TriangulateHull(positions, paddedKlm, leafVerts);
        for (LoopBlinnVertex& vert : leafVerts) {
            vert.fScale = fScale;
        }
        vertices.insert(vertices.end(), leafVerts.begin(), leafVerts.end());
    }

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

    glVertexAttribPointer(
        2,
        1,
        GL_FLOAT,
        GL_FALSE,
        sizeof(LoopBlinnVertex),
        reinterpret_cast<void*>(offsetof(LoopBlinnVertex, fScale)));
    glEnableVertexAttribArray(2);

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
