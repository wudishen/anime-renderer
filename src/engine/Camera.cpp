#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;

float ToRadians(float degrees) {
    return degrees * (kPi / 180.0f);
}
} // namespace

Camera::Camera() = default;

glm::vec3 Camera::Forward() const {
    const float yawRad = ToRadians(m_Yaw);
    const float pitchRad = ToRadians(m_Pitch);
    return glm::normalize(glm::vec3(
        std::cos(pitchRad) * std::sin(yawRad),
        std::sin(pitchRad),
        -std::cos(pitchRad) * std::cos(yawRad)
    ));
}

glm::vec3 Camera::Right() const {
    return glm::normalize(glm::cross(Forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 Camera::Up() const {
    return glm::normalize(glm::cross(Right(), Forward()));
}

void Camera::Pan(float dx, float dy) {
    m_Target -= Right() * (dx * m_PanSpeed * m_Distance);
    m_Target += Up() * (dy * m_PanSpeed * m_Distance);
}

void Camera::Orbit(float yawDelta, float pitchDelta) {
    m_Yaw += yawDelta * m_OrbitSpeed;
    m_Pitch += pitchDelta * m_OrbitSpeed;
    m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f);
}

void Camera::Zoom(float delta) {
    m_Distance *= (1.0f - delta * m_ZoomSpeed);
    m_Distance = std::clamp(m_Distance, m_MinDistance, m_MaxDistance);
}

glm::vec3 Camera::GetPosition() const {
    return m_Target - Forward() * m_Distance;
}

glm::mat4 Camera::GetView() const {
    return glm::lookAt(GetPosition(), m_Target, Up());
}

glm::mat4 Camera::GetProjection(float aspect) const {
    return glm::perspective(ToRadians(45.0f), aspect, 0.1f, 100.0f);
}

glm::mat4 Camera::GetViewProjection(float aspect) const {
    return GetProjection(aspect) * GetView();
}
