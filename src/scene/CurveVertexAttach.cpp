#include "CurveVertexAttach.h"

#include "BSplineFit.h"
#include "script/DpScript.h"

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
    const Scene& scene,
    int objectId,
    MeshAnchorKind kind,
    int vertexIndex,
    glm::vec3& outWorld) {
    const SceneObject* object = scene.FindById(objectId);
    if (!object) {
        return false;
    }

    switch (kind) {
    case MeshAnchorKind::DerivedPoint:
        if (!object->IsDerivedPoint() || !object->derivedEvalOk) {
            return false;
        }
        outWorld = object->derivedWorldPosition;
        return true;
    case MeshAnchorKind::Centroid:
        return ComputeMeshCentroidWorld(*object, outWorld);
    case MeshAnchorKind::Vertex:
    default:
        if (!object->IsMesh() ||
            vertexIndex < 0 ||
            vertexIndex >= static_cast<int>(object->vertices.size())) {
            return false;
        }
        outWorld = glm::vec3(
            object->transform * glm::vec4(object->vertices[static_cast<size_t>(vertexIndex)], 1.0f));
        return true;
    }
}

bool IsMeshAnchorValid(
    const Scene& scene,
    int objectId,
    MeshAnchorKind kind,
    int vertexIndex) {
    const SceneObject* object = scene.FindById(objectId);
    if (!object) {
        return false;
    }
    switch (kind) {
    case MeshAnchorKind::DerivedPoint:
        return object->IsDerivedPoint();
    case MeshAnchorKind::Centroid:
        return object->IsMesh() && !object->vertices.empty();
    case MeshAnchorKind::Vertex:
    default:
        return object->IsMesh() &&
               vertexIndex >= 0 &&
               vertexIndex < static_cast<int>(object->vertices.size());
    }
}

bool ProjectMeshAnchor(
    const Scene& scene,
    int objectId,
    MeshAnchorKind kind,
    int vertexIndex,
    const glm::mat4& view,
    const glm::mat4& projection,
    glm::vec2& outNorm) {
    glm::vec3 world{};
    if (!ComputeMeshAnchorWorld(scene, objectId, kind, vertexIndex, world)) {
        return false;
    }
    return ProjectWorldToScreenNorm(world, view, projection, outNorm);
}

void EvaluateDerivedPoints(Scene& scene) {
    for (SceneObject& object : scene.GetObjects()) {
        if (!object.IsDerivedPoint()) {
            continue;
        }
        const DpScript::Result result = DpScript::Evaluate(object.derivedScript, scene);
        object.derivedEvalOk = result.ok;
        object.derivedEvalError = result.error;
        if (result.ok) {
            object.derivedWorldPosition = result.point;
        }
    }
}

void SyncAttachedControlPoints(
    Scene& scene,
    const glm::mat4& view,
    const glm::mat4& projection) {
    EvaluateDerivedPoints(scene);

    for (SceneObject& curve : scene.GetObjects()) {
        if (!curve.IsBSplineCurve()) {
            continue;
        }
        if (curve.controlPointAttachments.empty() && curve.fitPointAttachments.empty()) {
            continue;
        }

        curve.PruneAllPointAttachments();

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
                // Derived point may fail eval this frame; keep attachment.
                ++it;
                continue;
            }

            glm::vec3& cp = curve.controlPoints[static_cast<size_t>(attach.controlPointIndex)];
            cp.x = screenNorm.x;
            cp.y = screenNorm.y;
            cp.z = 0.0f;
            ++it;
        }

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
