#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

namespace {
void CheckShaderCompile(GLuint shader, const char* label) {
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success) {
        return;
    }
    char log[1024];
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    std::cerr << "Shader compile failed (" << label << "):\n" << log << std::endl;
}

void CheckProgramLink(GLuint program) {
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success) {
        return;
    }
    char log[1024];
    glGetProgramInfoLog(program, sizeof(log), nullptr, log);
    std::cerr << "Shader link failed:\n" << log << std::endl;
}

std::string ReadFileOrEmpty(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Failed to open shader file: " << path << std::endl;
        return {};
    }
    std::stringstream stream;
    stream << file.rdbuf();
    return stream.str();
}
} // namespace

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
    const std::string vertexCode = ReadFileOrEmpty(vertexPath);
    const std::string fragmentCode = ReadFileOrEmpty(fragmentPath);

    const char* vCode = vertexCode.c_str();
    const char* fCode = fragmentCode.c_str();

    const GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vCode, nullptr);
    glCompileShader(vertex);
    CheckShaderCompile(vertex, vertexPath.c_str());

    const GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fCode, nullptr);
    glCompileShader(fragment);
    CheckShaderCompile(fragment, fragmentPath.c_str());

    programID = glCreateProgram();
    glAttachShader(programID, vertex);
    glAttachShader(programID, fragment);
    glLinkProgram(programID);
    CheckProgramLink(programID);

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader() {
    glDeleteProgram(programID);
}

void Shader::Use() {
    glUseProgram(programID);
}

void Shader::SetMat4(const char* name, const glm::mat4& value) const {
    const GLint location = glGetUniformLocation(programID, name);
    if (location < 0) {
        return;
    }
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetVec3(const char* name, const glm::vec3& value) const {
    const GLint location = glGetUniformLocation(programID, name);
    if (location < 0) {
        return;
    }
    glUniform3fv(location, 1, glm::value_ptr(value));
}

void Shader::SetVec2(const char* name, const glm::vec2& value) const {
    const GLint location = glGetUniformLocation(programID, name);
    if (location < 0) {
        return;
    }
    glUniform2fv(location, 1, glm::value_ptr(value));
}

void Shader::SetFloat(const char* name, float value) const {
    const GLint location = glGetUniformLocation(programID, name);
    if (location < 0) {
        return;
    }
    glUniform1f(location, value);
}

void Shader::SetInt(const char* name, int value) const {
    const GLint location = glGetUniformLocation(programID, name);
    if (location < 0) {
        return;
    }
    glUniform1i(location, value);
}
