#pragma once
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

class Shader {
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    void Use();
    void SetMat4(const char* name, const glm::mat4& value) const;
    void SetVec3(const char* name, const glm::vec3& value) const;

private:
    GLuint programID;
};