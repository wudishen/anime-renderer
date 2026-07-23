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

namespace {

bool ProjectAttachedVertex(
    Scene& scene,
    int meshObjectId,
    int vertexIndex,
    const glm::mat4& view,
    const glm::mat4& projection,
    glm::vec2& outNorm) {
    SceneObject* mesh = scene.FindById(meshObjectId);
    if (!mesh || !mesh->IsMesh() ||
        vertexIndex < 0 ||
        vertexIndex >= static_cast<int>(mesh->vertices.size())) {
        return false;
    }

    const glm::vec3 world =
        glm::vec3(mesh->transform * glm::vec4(mesh->vertices[static_cast<size_t>(vertexIndex)], 1.0f));
    return ProjectWorldToScreenNorm(world, view, projection, outNorm);
}

} // namespace

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

        // 1) Control-point tags: pin CPs directly to mesh vertices.
        for (auto it = curve.controlPointAttachments.begin();
             it != curve.controlPointAttachments.end();) {
            const ControlPointAttachment& attach = *it;
            if (attach.controlPointIndex < 0 ||
                attach.controlPointIndex >= static_cast<int>(curve.controlPoints.size())) {
                it = curve.controlPointAttachments.erase(it);
                continue;
            }

            glm::vec2 screenNorm{};
            if (!ProjectAttachedVertex(
                    scene, attach.meshObjectId, attach.vertexIndex, view, projection, screenNorm)) {
                // Drop invalid mesh/vertex refs; keep if only behind camera this frame.
                SceneObject* mesh = scene.FindById(attach.meshObjectId);
                if (!mesh || !mesh->IsMesh() ||
                    attach.vertexIndex < 0 ||
                    attach.vertexIndex >= static_cast<int>(mesh->vertices.size())) {
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
            if (!ProjectAttachedVertex(
                    scene, attach.meshObjectId, attach.vertexIndex, view, projection, screenNorm)) {
                SceneObject* mesh = scene.FindById(attach.meshObjectId);
                if (!mesh || !mesh->IsMesh() ||
                    attach.vertexIndex < 0 ||
                    attach.vertexIndex >= static_cast<int>(mesh->vertices.size())) {
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
