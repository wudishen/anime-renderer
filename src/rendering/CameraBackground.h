#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

struct CameraBackground {
    std::string imagePath;
    GLuint textureId = 0;
    int width = 0;
    int height = 0;
    float opacity = 0.5f;
    glm::vec2 offset{0.0f, 0.0f}; // NDC offset
    float scale = 1.0f;
    bool enabled = true;

    CameraBackground() = default;
    ~CameraBackground();

    CameraBackground(const CameraBackground&) = delete;
    CameraBackground& operator=(const CameraBackground&) = delete;
    CameraBackground(CameraBackground&& other) noexcept;
    CameraBackground& operator=(CameraBackground&& other) noexcept;

    bool HasImage() const { return textureId != 0; }
    bool LoadFromFile(const std::string& path);
    void Clear();
};
