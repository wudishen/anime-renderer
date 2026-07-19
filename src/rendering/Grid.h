#pragma once

#include "Shader.h"
#include <glad/glad.h>
#include <glm/glm.hpp>

class Grid {
public:
    // Builds an XZ floor grid centered at the origin.
    // halfCells: number of cells from center to each edge.
    // cellSize: world size of one cell.
    Grid(int halfCells = 10, float cellSize = 1.0f);
    ~Grid();

    Grid(const Grid&) = delete;
    Grid& operator=(const Grid&) = delete;

    void Draw(Shader& shader, const glm::mat4& viewProjection) const;

private:
    void Upload(const float* data, GLsizei floatCount, GLuint& vao, GLuint& vbo, GLsizei& vertexCount);

    GLuint m_GridVAO = 0;
    GLuint m_GridVBO = 0;
    GLsizei m_GridVertexCount = 0;

    GLuint m_AxisVAO = 0;
    GLuint m_AxisVBO = 0;
    GLsizei m_AxisVertexCount = 0;
};
