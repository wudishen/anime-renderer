#include "CurveVertexAttach.h"

#include "BSplineFit.h"

namespace CurveVertexAttach {

bool ProjectWorldToScreenNorm(
    const glm::vec3& world,
    const glm::mat4& view,
    const glm::mat4& projection,
    glm::vec2& outNorm) {
    const glm::vec4 clip = projection * view * glm::vec4(world, 1.0f);
    if (clip.w <= 1e-6f) {
        return false;
    }

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    // NDC y is up; screen-normalized y is down.
    outNorm.x = ndc.x * 0.5f + 0.5f;
    outNorm.y = 1.0f - (ndc.y * 0.5f + 0.5f);
    return true;
}

bool ComputeMeshCentroidWorld(const SceneObject& mesh, glm::vec3& outWorld) {
    if (!mesh.IsMesh() || mesh.vertices.empty()) {
        return false;
    }

    glm::vec3 sum(0.0f);
    for (const glm::vec3& v : mesh.vertices) {
        sum += v;
    }
    const glm::vec3 local = sum / static_cast<float>(mesh.vertices.size());
    outWorld = glm::vec3(mesh.transform * glm::vec4(local, 1.0f));
    return true;
}

bool ComputeMeshAnchorWorld(
    const SceneObject& mesh,
    MeshAnchorKind kind,
    int vertexIndex,
    glm::vec3& outWorld) {
    if (!mesh.IsMesh()) {
        return false;
    }

    switch (kind) {
    case MeshAnchorKind::Centroid:
        return ComputeMeshCentroidWorld(mesh, outWorld);
    case MeshAnchorKind::Vertex:
    default:
        if (vertexIndex < 0 || vertexIndex >= static_cast<int>(mesh.vertices.size())) {
            return false;
        }
        outWorld = glm::vec3(
            mesh.transform * glm::vec4(mesh.vertices[static_cast<size_t>(vertexIndex)], 1.0f));
        return true;
    }
}

bool IsMeshAnchorValid(
    const Scene& scene,
    int meshObjectId,
    MeshAnchorKind kind,
    int vertexIndex) {
    const SceneObject* mesh = scene.FindById(meshObjectId);
    if (!mesh || !mesh->IsMesh() || mesh->vertices.empty()) {
        return false;
    }
    if (kind == MeshAnchorKind::Vertex) {
        return vertexIndex >= 0 && vertexIndex < static_cast<int>(mesh->vertices.size());
    }
    return kind == MeshAnchorKind::Centroid;
}

bool ProjectMeshAnchor(
    const Scene& scene,
    int meshObjectId,
    MeshAnchorKind kind,
    int vertexIndex,
    const glm::mat4& view,
    const glm::mat4& projection,
    glm::vec2& outNorm) {
    const SceneObject* mesh = scene.FindById(meshObjectId);
    if (!mesh) {
        return false;
    }

    glm::vec3 world{};
    if (!ComputeMeshAnchorWorld(*mesh, kind, vertexIndex, world)) {
        return false;
    }
    return ProjectWorldToScreenNorm(world, view, projection, outNorm);
}

void SyncAttachedControlPoints(
    Scene& scene,
    const glm::mat4& view,
    const glm::mat4& projection) {
    for (SceneObject& curve : scene.GetObjects()) {
        if (!curve.IsBSplineCurve()) {
            continue;
        }
        if (curve.controlPointAttachments.empty() && curve.fitPointAttachments.empty()) {
            continue;
        }

        curve.PruneAllPointAttachments();

        // 1) Control-point tags: pin CPs directly to mesh anchors.
        for (auto it = curve.controlPointAttachments.begin();
             it != curve.controlPointAttachments.end();) {
            const ControlPointAttachment& attach = *it;
            if (attach.controlPointIndex < 0 ||
                attach.controlPointIndex >= static_cast<int>(curve.controlPoints.size())) {
                it = curve.controlPointAttachments.erase(it);
                continue;
            }

            glm::vec2 screenNorm{};
            if (!ProjectMeshAnchor(
                    scene,
                    attach.meshObjectId,
                    attach.anchorKind,
                    attach.vertexIndex,
                    view,
                    projection,
                    screenNorm)) {
                if (!IsMeshAnchorValid(
                        scene, attach.meshObjectId, attach.anchorKind, attach.vertexIndex)) {
                    it = curve.controlPointAttachments.erase(it);
                    continue;
                }
                ++it;
                continue;
            }

            glm::vec3& cp = curve.controlPoints[static_cast<size_t>(attach.controlPointIndex)];
            cp.x = screenNorm.x;
            cp.y = screenNorm.y;
            cp.z = 0.0f;
            ++it;
        }

        // 2) Fit-point tags: move on-curve points, then re-solve CPs so the curve
        //    still interpolates every fit point.
        if (curve.fitPointAttachments.empty()) {
            continue;
        }

        std::vector<glm::vec2> fitPoints = BSplineFit::FitPointsFromControls(curve.controlPoints);
        bool anyFitUpdated = false;

        for (auto it = curve.fitPointAttachments.begin(); it != curve.fitPointAttachments.end();) {
            const FitPointAttachment& attach = *it;
            if (attach.fitPointIndex < 0 ||
                attach.fitPointIndex >= static_cast<int>(fitPoints.size())) {
                it = curve.fitPointAttachments.erase(it);
                continue;
            }

            glm::vec2 screenNorm{};
            if (!ProjectMeshAnchor(
                    scene,
                    attach.meshObjectId,
                    attach.anchorKind,
                    attach.vertexIndex,
                    view,
                    projection,
                    screenNorm)) {
                if (!IsMeshAnchorValid(
                        scene, attach.meshObjectId, attach.anchorKind, attach.vertexIndex)) {
                    it = curve.fitPointAttachments.erase(it);
                    continue;
                }
                ++it;
                continue;
            }

            fitPoints[static_cast<size_t>(attach.fitPointIndex)] = screenNorm;
            anyFitUpdated = true;
            ++it;
        }

        if (!anyFitUpdated) {
            continue;
        }

        std::vector<glm::vec3> solved = BSplineFit::SolveControlsFromFitPoints(fitPoints);
        if (!solved.empty()) {
            curve.controlPoints = std::move(solved);
        }
    }
}

} // namespace CurveVertexAttach
