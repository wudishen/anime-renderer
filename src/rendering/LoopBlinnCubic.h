#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <array>
#include <vector>

struct LoopBlinnVertex {
    glm::vec3 position;
    glm::vec3 klm;
};

// Experimental Loop-Blinn cubic Bézier stroke mesh (planar XY, integral cubics).
class LoopBlinnCubicStroke {
public:
    LoopBlinnCubicStroke() = default;
    ~LoopBlinnCubicStroke();

    LoopBlinnCubicStroke(const LoopBlinnCubicStroke&) = delete;
    LoopBlinnCubicStroke& operator=(const LoopBlinnCubicStroke&) = delete;
    LoopBlinnCubicStroke(LoopBlinnCubicStroke&& other) noexcept;
    LoopBlinnCubicStroke& operator=(LoopBlinnCubicStroke&& other) noexcept;

    // Builds GPU triangles covering the control hull with procedural (k,l,m) coords.
    // Returns false if the cubic degenerates to a line/point (nothing to draw).
    bool Build(const std::array<glm::vec2, 4>& controlPoints, float z = 0.0f);

    void Destroy();
    void Draw() const;

    bool IsValid() const { return m_VAO != 0 && m_VertexCount > 0; }
    GLsizei GetVertexCount() const { return m_VertexCount; }

private:
    GLuint m_VAO = 0;
    GLuint m_VBO = 0;
    GLsizei m_VertexCount = 0;
};
