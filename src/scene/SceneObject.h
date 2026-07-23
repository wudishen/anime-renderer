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
};

// Experimental: bind a B-spline control/fit point to a mesh vertex (screen follow).
struct ControlPointAttachment {
    int controlPointIndex = -1;
    int meshObjectId = -1;
    int vertexIndex = -1;
};

struct FitPointAttachment {
    int fitPointIndex = -1;
    int meshObjectId = -1;
    int vertexIndex = -1;
};

struct SceneObject {
    int id = 0;
    std::string name;
    SceneObjectType type = SceneObjectType::Mesh;
    std::vector<glm::vec3> vertices; // CPU copy for mesh/camera gizmo picking
    std::vector<glm::vec3> controlPoints; // Curves: normalized screen (0,0)=top-left..(1,1); z unused
    std::vector<ControlPointAttachment> controlPointAttachments; // B-spline only
    std::vector<FitPointAttachment> fitPointAttachments; // B-spline only
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

    int FitPointCount() const {
        if (controlPoints.size() < 4) {
            return 0;
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

    void SetControlPointAttachment(int controlPointIndex, int meshObjectId, int vertexIndex) {
        if (const auto existing = FindAttachmentIndex(controlPointIndex)) {
            controlPointAttachments[*existing] = {controlPointIndex, meshObjectId, vertexIndex};
            return;
        }
        controlPointAttachments.push_back({controlPointIndex, meshObjectId, vertexIndex});
    }

    void SetFitPointAttachment(int fitPointIndex, int meshObjectId, int vertexIndex) {
        if (const auto existing = FindFitAttachmentIndex(fitPointIndex)) {
            fitPointAttachments[*existing] = {fitPointIndex, meshObjectId, vertexIndex};
            return;
        }
        fitPointAttachments.push_back({fitPointIndex, meshObjectId, vertexIndex});
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
