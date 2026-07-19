#include "Mesh.h"

Mesh::~Mesh() {
    Destroy();
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_VAO(other.m_VAO), m_VBO(other.m_VBO), m_VertexCount(other.m_VertexCount) {
    other.m_VAO = 0;
    other.m_VBO = 0;
    other.m_VertexCount = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        Destroy();
        m_VAO = other.m_VAO;
        m_VBO = other.m_VBO;
        m_VertexCount = other.m_VertexCount;
        other.m_VAO = 0;
        other.m_VBO = 0;
        other.m_VertexCount = 0;
    }
    return *this;
}

void Mesh::Upload(const std::vector<glm::vec3>& vertices) {
    Destroy();
    if (vertices.empty()) {
        return;
    }

    m_VertexCount = static_cast<GLsizei>(vertices.size());

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3)),
        vertices.data(),
        GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Mesh::Destroy() {
    if (m_VBO != 0) {
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }
    m_VertexCount = 0;
}

void Mesh::Draw() const {
    if (!IsValid()) {
        return;
    }
    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, m_VertexCount);
}
