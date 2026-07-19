#pragma once

#include "CameraBackground.h"
#include "Shader.h"

class CameraBackgroundRenderer {
public:
    CameraBackgroundRenderer();
    ~CameraBackgroundRenderer();

    CameraBackgroundRenderer(const CameraBackgroundRenderer&) = delete;
    CameraBackgroundRenderer& operator=(const CameraBackgroundRenderer&) = delete;

    void Draw(const CameraBackground& background, float viewportAspect, Shader& shader) const;

private:
    GLuint m_VAO = 0;
    GLuint m_VBO = 0;
};
