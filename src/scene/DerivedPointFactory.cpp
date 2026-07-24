#include "DerivedPointFactory.h"

namespace DerivedPointFactory {

SceneObject Create(const std::string& name) {
    SceneObject object;
    object.type = SceneObjectType::DerivedPoint;
    object.name = name;
    object.color = {0.45f, 1.0f, 0.55f};
    object.derivedScript =
        "# DpScript — return one world-space point (vec3)\n"
        "#\n"
        "# Meshes are available by object name (identifier names only):\n"
        "#   triangle.p1, triangle.p2, ...   (1-based verts, world space)\n"
        "#   triangle.v0, triangle.v1, ...   (0-based verts)\n"
        "#   triangle.centroid\n"
        "#   triangle.count\n"
        "#\n"
        "# Built-ins: avg(a,b,...), vec(x,y,z), len(v), normalize(v), dot(a,b), cross(a,b)\n"
        "#\n"
        "# Example:\n"
        "#   dp = (triangle.p1 + triangle.p2 + triangle.p3) / 3\n"
        "#   return dp\n"
        "\n"
        "return triangle.centroid\n";
    object.derivedWorldPosition = {0.0f, 0.0f, 0.0f};
    object.derivedEvalOk = false;
    object.derivedEvalError = "Not evaluated yet";
    return object;
}

} // namespace DerivedPointFactory
