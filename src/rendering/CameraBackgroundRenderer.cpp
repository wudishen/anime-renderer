#include "CameraBackgroundRenderer.h"

CameraBackgroundRenderer::CameraBackgroundRenderer() {
    // pos.xy, uv.xy
    const float quad[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
    };

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

CameraBackgroundRenderer::~CameraBackgroundRenderer() {
    if (m_VBO != 0) {
        glDeleteBuffers(1, &m_VBO);
    }
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
    }
}

void CameraBackgroundRenderer::Draw(const CameraBackground& background, float viewportAspect, Shader& shader) const {
    if (!background.HasImage() || !background.enabled || m_VAO == 0) {
        return;
    }

    const float imageAspect =
        background.height > 0 ? static_cast<float>(background.width) / static_cast<float>(background.height) : 1.0f;

    float scaleX = background.scale;
    float scaleY = background.scale;
    if (imageAspect > viewportAspect) {
        scaleY = background.scale * (viewportAspect / imageAspect);
    } else {
        scaleX = background.scale * (imageAspect / viewportAspect);
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader.Use();
    shader.SetVec2("uOffset", background.offset);
    shader.SetVec2("uScale", glm::vec2(scaleX, scaleY));
    shader.SetFloat("uOpacity", background.opacity);
    shader.SetInt("uTexture", 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, background.textureId);
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
