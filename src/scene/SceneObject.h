#pragma once

#include "rendering/Mesh.h"
#include "rendering/CameraBackground.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <optional>
#include <string>
#include <vector>

enum class SceneObjectType {
    Mesh,
    Camera,
    BezierCurve,
    BSplineCurve,
    DerivedPoint,
};

// Experimental: what a curve point can follow.
enum class MeshAnchorKind {
    Vertex = 0,        // a single mesh vertex
    Centroid = 1,      // average of all local vertex positions (then transformed)
    DerivedPoint = 2,  // scripted Derived Point object (meshObjectId = that object's id)
};

// Experimental: bind a B-spline control/fit point to a mesh anchor (screen follow).
struct ControlPointAttachment {
    int controlPointIndex = -1;
    int meshObjectId = -1;
    MeshAnchorKind anchorKind = MeshAnchorKind::Vertex;
    int vertexIndex = -1; // used only when anchorKind == Vertex
};

struct FitPointAttachment {
    int fitPointIndex = -1;
    int meshObjectId = -1;
    MeshAnchorKind anchorKind = MeshAnchorKind::Vertex;
    int vertexIndex = -1;
};

struct SceneObject {
    int id = 0;
    std::string name;
    SceneObjectType type = SceneObjectType::Mesh;
    std::vector<glm::vec3> vertices; // CPU copy for mesh/camera gizmo picking
    std::vector<glm::vec3> controlPoints; // Curves: normalized screen (0,0)=top-left..(1,1); z unused
    bool closed = false; // Bézier polycurve or periodic B-spline loop
    std::vector<ControlPointAttachment> controlPointAttachments; // B-spline only
    std::vector<FitPointAttachment> fitPointAttachments; // B-spline only
    // DerivedPoint: DpScript source + last evaluated world position.
    std::string derivedScript;
    glm::vec3 derivedWorldPosition{0.0f};
    bool derivedEvalOk = false;
    std::string derivedEvalError;
    Mesh mesh;
    glm::vec3 color{1.0f, 0.5f, 0.2f};
    glm::mat4 transform{1.0f};
    float cameraFovDegrees = 45.0f;
    CameraBackground background;

    void UploadMesh() {
        mesh.Upload(vertices);
    }

    bool IsCamera() const { return type == SceneObjectType::Camera; }
    bool IsCurve() const {
        return type == SceneObjectType::BezierCurve || type == SceneObjectType::BSplineCurve;
    }
    bool IsBezierCurve() const { return type == SceneObjectType::BezierCurve; }
    bool IsBSplineCurve() const { return type == SceneObjectType::BSplineCurve; }
    bool IsMesh() const { return type == SceneObjectType::Mesh; }
    bool IsDerivedPoint() const { return type == SceneObjectType::DerivedPoint; }

    int FitPointCount() const {
        if (!IsBSplineCurve() || controlPoints.size() < 4) {
            return 0;
        }
        if (closed) {
            return static_cast<int>(controlPoints.size());
        }
        return static_cast<int>(controlPoints.size()) - 2;
    }

    std::optional<size_t> FindAttachmentIndex(int controlPointIndex) const {
        for (size_t i = 0; i < controlPointAttachments.size(); ++i) {
            if (controlPointAttachments[i].controlPointIndex == controlPointIndex) {
                return i;
            }
        }
        return std::nullopt;
    }

    std::optional<size_t> FindFitAttachmentIndex(int fitPointIndex) const {
        for (size_t i = 0; i < fitPointAttachments.size(); ++i) {
            if (fitPointAttachments[i].fitPointIndex == fitPointIndex) {
                return i;
            }
        }
        return std::nullopt;
    }

    bool IsControlPointAttached(int controlPointIndex) const {
        return FindAttachmentIndex(controlPointIndex).has_value();
    }

    bool IsFitPointAttached(int fitPointIndex) const {
        return FindFitAttachmentIndex(fitPointIndex).has_value();
    }

    void SetControlPointAttachment(
        int controlPointIndex,
        int meshObjectId,
        MeshAnchorKind anchorKind,
        int vertexIndex) {
        const ControlPointAttachment value{
            controlPointIndex,
            meshObjectId,
            anchorKind,
            anchorKind == MeshAnchorKind::Vertex ? vertexIndex : -1};
        if (const auto existing = FindAttachmentIndex(controlPointIndex)) {
            controlPointAttachments[*existing] = value;
            return;
        }
        controlPointAttachments.push_back(value);
    }

    void SetFitPointAttachment(
        int fitPointIndex,
        int meshObjectId,
        MeshAnchorKind anchorKind,
        int vertexIndex) {
        const FitPointAttachment value{
            fitPointIndex,
            meshObjectId,
            anchorKind,
            anchorKind == MeshAnchorKind::Vertex ? vertexIndex : -1};
        if (const auto existing = FindFitAttachmentIndex(fitPointIndex)) {
            fitPointAttachments[*existing] = value;
            return;
        }
        fitPointAttachments.push_back(value);
    }

    void ClearControlPointAttachment(int controlPointIndex) {
        if (const auto existing = FindAttachmentIndex(controlPointIndex)) {
            controlPointAttachments.erase(
                controlPointAttachments.begin() + static_cast<std::ptrdiff_t>(*existing));
        }
    }

    void ClearFitPointAttachment(int fitPointIndex) {
        if (const auto existing = FindFitAttachmentIndex(fitPointIndex)) {
            fitPointAttachments.erase(
                fitPointAttachments.begin() + static_cast<std::ptrdiff_t>(*existing));
        }
    }

    void PruneControlPointAttachments() {
        controlPointAttachments.erase(
            std::remove_if(
                controlPointAttachments.begin(),
                controlPointAttachments.end(),
                [&](const ControlPointAttachment& a) {
                    return a.controlPointIndex < 0 ||
                           a.controlPointIndex >= static_cast<int>(controlPoints.size());
                }),
            controlPointAttachments.end());
    }

    void PruneFitPointAttachments() {
        const int fitCount = FitPointCount();
        fitPointAttachments.erase(
            std::remove_if(
                fitPointAttachments.begin(),
                fitPointAttachments.end(),
                [&](const FitPointAttachment& a) {
                    return a.fitPointIndex < 0 || a.fitPointIndex >= fitCount;
                }),
            fitPointAttachments.end());
    }

    void PruneAllPointAttachments() {
        PruneControlPointAttachments();
        PruneFitPointAttachments();
    }

    glm::vec3 GetTranslation() const {
        return glm::vec3(transform[3]);
    }

    void SetTranslation(const glm::vec3& translation) {
        transform[3] = glm::vec4(translation, 1.0f);
    }

    void AddTranslation(const glm::vec3& delta) {
        SetTranslation(GetTranslation() + delta);
    }

    glm::vec3 GetRight() const {
        return glm::normalize(glm::vec3(transform[0]));
    }

    glm::vec3 GetUp() const {
        return glm::normalize(glm::vec3(transform[1]));
    }

    // OpenGL camera looks down local -Z.
    glm::vec3 GetForward() const {
        return glm::normalize(-glm::vec3(transform[2]));
    }

    glm::mat4 GetViewMatrix() const {
        return glm::inverse(transform);
    }
};
