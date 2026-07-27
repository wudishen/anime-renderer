#include "CurveFactory.h"

namespace CurveFactory {

// Control points use normalized screen space: (0,0)=top-left, (1,1)=bottom-right.
// z is unused (kept 0). Curves are viewport annotations, not world geometry.

SceneObject CreateBezier() {
    SceneObject object;
    object.type = SceneObjectType::BezierCurve;
    object.name = "bezier";
    object.color = {0.95f, 0.85f, 0.25f};
    object.closed = false;
    object.controlPoints = {
        {0.20f, 0.38f, 0.0f},
        {0.35f, 0.18f, 0.0f},
        {0.65f, 0.58f, 0.0f},
        {0.80f, 0.38f, 0.0f},
    };
    return object;
}

SceneObject CreateBSpline() {
    SceneObject object;
    object.type = SceneObjectType::BSplineCurve;
    object.name = "bspline";
    object.color = {0.35f, 0.85f, 0.95f};
    object.closed = false;
    object.controlPoints = {
        {0.12f, 0.72f, 0.0f},
        {0.28f, 0.55f, 0.0f},
        {0.42f, 0.82f, 0.0f},
        {0.58f, 0.60f, 0.0f},
        {0.72f, 0.78f, 0.0f},
        {0.88f, 0.68f, 0.0f},
    };
    return object;
}

// Periodic uniform cubic B-spline: n CPs → n segments, C2-smooth including the join.
SceneObject CreateClosedBSpline() {
    SceneObject object;
    object.type = SceneObjectType::BSplineCurve;
    object.name = "closed-bspline";
    object.color = {0.45f, 0.95f, 0.75f};
    object.closed = true;
    // Control polygon around a soft oval (curve stays inside the polygon).
    object.controlPoints = {
        {0.50f, 0.22f, 0.0f},
        {0.68f, 0.28f, 0.0f},
        {0.78f, 0.45f, 0.0f},
        {0.72f, 0.65f, 0.0f},
        {0.50f, 0.78f, 0.0f},
        {0.28f, 0.65f, 0.0f},
        {0.22f, 0.45f, 0.0f},
        {0.32f, 0.28f, 0.0f},
    };
    return object;
}

} // namespace CurveFactory
