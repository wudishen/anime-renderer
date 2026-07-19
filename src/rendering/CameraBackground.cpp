#include "CameraBackground.h"

#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

CameraBackground::~CameraBackground() {
    Clear();
}

CameraBackground::CameraBackground(CameraBackground&& other) noexcept
    : imagePath(std::move(other.imagePath)),
      textureId(other.textureId),
      width(other.width),
      height(other.height),
      opacity(other.opacity),
      offset(other.offset),
      scale(other.scale),
      enabled(other.enabled) {
    other.textureId = 0;
    other.width = 0;
    other.height = 0;
}

CameraBackground& CameraBackground::operator=(CameraBackground&& other) noexcept {
    if (this != &other) {
        Clear();
        imagePath = std::move(other.imagePath);
        textureId = other.textureId;
        width = other.width;
        height = other.height;
        opacity = other.opacity;
        offset = other.offset;
        scale = other.scale;
        enabled = other.enabled;
        other.textureId = 0;
        other.width = 0;
        other.height = 0;
    }
    return *this;
}

void CameraBackground::Clear() {
    if (textureId != 0) {
        glDeleteTextures(1, &textureId);
        textureId = 0;
    }
    imagePath.clear();
    width = 0;
    height = 0;
}

bool CameraBackground::LoadFromFile(const std::string& path) {
    stbi_set_flip_vertically_on_load(true);
    int channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        std::cerr << "Failed to load image: " << path << " (" << stbi_failure_reason() << ")" << std::endl;
        return false;
    }

    GLuint newTexture = 0;
    glGenTextures(1, &newTexture);
    glBindTexture(GL_TEXTURE_2D, newTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    if (textureId != 0) {
        glDeleteTextures(1, &textureId);
    }

    textureId = newTexture;
    imagePath = path;
    enabled = true;
    return true;
}
