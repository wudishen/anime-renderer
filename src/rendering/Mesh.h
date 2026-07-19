#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

class Mesh {
public:
    Mesh() = default;
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void Upload(const std::vector<glm::vec3>& vertices);
    void Destroy();
    void Draw() const;

    GLsizei GetVertexCount() const { return m_VertexCount; }
    bool IsValid() const { return m_VAO != 0 && m_VertexCount > 0; }

private:
    GLuint m_VAO = 0;
    GLuint m_VBO = 0;
    GLsizei m_VertexCount = 0;
};
