#include "Grid.h"

#include <vector>

Grid::Grid(int halfCells, float cellSize) {
    if (halfCells < 1) {
        halfCells = 1;
    }
    if (cellSize <= 0.0f) {
        cellSize = 1.0f;
    }

    const float extent = static_cast<float>(halfCells) * cellSize;
    std::vector<float> gridLines;
    gridLines.reserve(static_cast<size_t>((halfCells * 2 + 1) * 2 * 2 * 3));

    for (int i = -halfCells; i <= halfCells; ++i) {
        if (i == 0) {
            continue; // center axes drawn separately
        }
        const float offset = static_cast<float>(i) * cellSize;

        // Line parallel to X (constant Z)
        gridLines.push_back(-extent);
        gridLines.push_back(0.0f);
        gridLines.push_back(offset);
        gridLines.push_back(extent);
        gridLines.push_back(0.0f);
        gridLines.push_back(offset);

        // Line parallel to Z (constant X)
        gridLines.push_back(offset);
        gridLines.push_back(0.0f);
        gridLines.push_back(-extent);
        gridLines.push_back(offset);
        gridLines.push_back(0.0f);
        gridLines.push_back(extent);
    }

    Upload(gridLines.data(), static_cast<GLsizei>(gridLines.size()), m_GridVAO, m_GridVBO, m_GridVertexCount);

    // X axis (red) and Z axis (blue) through the origin.
    const float axes[] = {
        -extent, 0.0f, 0.0f,  extent, 0.0f, 0.0f, // X
         0.0f, 0.0f, -extent,  0.0f, 0.0f, extent, // Z
    };
    Upload(axes, static_cast<GLsizei>(sizeof(axes) / sizeof(float)), m_AxisVAO, m_AxisVBO, m_AxisVertexCount);
}

Grid::~Grid() {
    if (m_GridVBO != 0) {
        glDeleteBuffers(1, &m_GridVBO);
    }
    if (m_GridVAO != 0) {
        glDeleteVertexArrays(1, &m_GridVAO);
    }
    if (m_AxisVBO != 0) {
        glDeleteBuffers(1, &m_AxisVBO);
    }
    if (m_AxisVAO != 0) {
        glDeleteVertexArrays(1, &m_AxisVAO);
    }
}

void Grid::Upload(const float* data, GLsizei floatCount, GLuint& vao, GLuint& vbo, GLsizei& vertexCount) {
    vertexCount = floatCount / 3;
    if (vertexCount <= 0) {
        return;
    }

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(floatCount * sizeof(float)), data, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Grid::Draw(Shader& shader, const glm::mat4& viewProjection) const {
    shader.Use();
    shader.SetMat4("uMVP", viewProjection);

    if (m_GridVAO != 0 && m_GridVertexCount > 0) {
        shader.SetVec3("uColor", glm::vec3(0.35f, 0.35f, 0.42f));
        glBindVertexArray(m_GridVAO);
        glDrawArrays(GL_LINES, 0, m_GridVertexCount);
    }

    if (m_AxisVAO != 0 && m_AxisVertexCount >= 4) {
        // X axis
        shader.SetVec3("uColor", glm::vec3(0.75f, 0.25f, 0.25f));
        glBindVertexArray(m_AxisVAO);
        glDrawArrays(GL_LINES, 0, 2);

        // Z axis
        shader.SetVec3("uColor", glm::vec3(0.25f, 0.45f, 0.85f));
        glDrawArrays(GL_LINES, 2, 2);
    }

    glBindVertexArray(0);
}
