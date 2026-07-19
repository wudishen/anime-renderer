#include "ObjLoader.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>

namespace ObjLoader {
namespace {

int ResolveIndex(int index, int count) {
    if (index > 0) {
        return index - 1;
    }
    if (index < 0) {
        return count + index;
    }
    return -1;
}

int ParseVertexIndex(const std::string& token, int positionCount) {
    // Formats: v | v/vt | v/vt/vn | v//vn
    const auto slash = token.find('/');
    const std::string indexText = slash == std::string::npos ? token : token.substr(0, slash);
    if (indexText.empty()) {
        return -1;
    }
    return ResolveIndex(std::stoi(indexText), positionCount);
}

void NormalizeMesh(std::vector<glm::vec3>& vertices) {
    if (vertices.empty()) {
        return;
    }

    glm::vec3 minBounds = vertices[0];
    glm::vec3 maxBounds = vertices[0];
    for (const glm::vec3& v : vertices) {
        minBounds = glm::min(minBounds, v);
        maxBounds = glm::max(maxBounds, v);
    }

    const glm::vec3 center = 0.5f * (minBounds + maxBounds);
    const glm::vec3 extent = maxBounds - minBounds;
    const float maxExtent = std::max({extent.x, extent.y, extent.z, 1e-6f});
    const float scale = 2.0f / maxExtent;

    for (glm::vec3& v : vertices) {
        v = (v - center) * scale;
    }
}

} // namespace

LoadResult Load(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Failed to open OBJ: " << path << std::endl;
        return {std::nullopt, "Failed to open file:\n" + path};
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> triangles;
    size_t faceLineCount = 0;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream stream(line);
        std::string tag;
        stream >> tag;

        if (tag == "v") {
            glm::vec3 position{};
            stream >> position.x >> position.y >> position.z;
            positions.push_back(position);
        } else if (tag == "f") {
            ++faceLineCount;
            std::vector<int> faceIndices;
            std::string token;
            while (stream >> token) {
                try {
                    const int index = ParseVertexIndex(token, static_cast<int>(positions.size()));
                    if (index >= 0 && index < static_cast<int>(positions.size())) {
                        faceIndices.push_back(index);
                    }
                } catch (const std::exception&) {
                    // Skip malformed face tokens.
                }
            }

            if (faceIndices.size() < 3) {
                continue;
            }

            // Fan triangulation for triangles / quads / n-gons.
            for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                triangles.push_back(positions[faceIndices[0]]);
                triangles.push_back(positions[faceIndices[i]]);
                triangles.push_back(positions[faceIndices[i + 1]]);
            }
        }
    }

    if (triangles.empty()) {
        std::cerr << "OBJ contained no triangles: " << path << std::endl;
        std::ostringstream error;
        error << "Import failed: OBJ has no faces to render.\n\n"
              << "File: " << path << "\n"
              << "Vertices found: " << positions.size() << "\n"
              << "Face lines found: " << faceLineCount << "\n\n"
              << "Re-export from Blender with mesh faces (Geometry), not only vertices.";
        return {std::nullopt, error.str()};
    }

    NormalizeMesh(triangles);

    SceneObject object;
    object.name = std::filesystem::path(path).stem().string();
    object.vertices = std::move(triangles);
    object.color = {0.75f, 0.78f, 0.85f};
    object.mesh.Upload(object.vertices);

    std::cout << "Imported OBJ '" << object.name << "' (" << object.vertices.size() / 3
              << " triangles)" << std::endl;
    return {std::move(object), {}};
}

} // namespace ObjLoader
