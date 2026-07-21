#pragma once

#include "rendering/Mesh.h"
#include "rendering/CameraBackground.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

enum class SceneObjectType {
    Mesh,
    Camera,
    BezierCurve,
    BSplineCurve,
};

struct SceneObject {
    int id = 0;
    std::string name;
    SceneObjectType type = SceneObjectType::Mesh;
    std::vector<glm::vec3> vertices; // CPU copy for mesh/camera gizmo picking
    std::vector<glm::vec3> controlPoints; // Curves: normalized screen (0,0)=top-left..(1,1); z unused
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
