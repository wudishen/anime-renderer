#pragma once
#include <glm/glm.hpp>

class Camera {
public:
    Camera();

    void Pan(float dx, float dy);
    void Orbit(float yawDelta, float pitchDelta);
    void Zoom(float delta);

    glm::vec3 GetPosition() const;
    float GetDistance() const { return m_Distance; }
    glm::vec3 GetForward() const { return Forward(); }
    glm::vec3 GetRight() const { return Right(); }
    glm::vec3 GetUp() const { return Up(); }

    glm::mat4 GetView() const;
    glm::mat4 GetProjection(float aspect) const;
    glm::mat4 GetViewProjection(float aspect) const;

private:
    glm::vec3 m_Target{0.0f};
    float m_Distance = 3.0f;
    float m_Yaw = 0.0f;    // degrees around Y
    float m_Pitch = 0.0f;  // degrees around X

    float m_PanSpeed = 0.0025f;
    float m_OrbitSpeed = 0.25f;
    float m_ZoomSpeed = 0.15f;
    float m_MinDistance = 0.5f;
    float m_MaxDistance = 50.0f;

    glm::vec3 Forward() const;
    glm::vec3 Right() const;
    glm::vec3 Up() const;
};
