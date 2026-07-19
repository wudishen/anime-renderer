#pragma once

#include "scene/Scene.h"
#include <string>

namespace SceneFile {

// Current on-disk format version written by this build.
constexpr int kCurrentFormatVersion = 2;

// Oldest format version this build can still migrate and open.
constexpr int kMinReadableFormatVersion = 1;

struct Result {
    bool ok = false;
    std::string error;
    std::string warning; // non-fatal notes (migrations, skipped unknowns, etc.)
};

// Saves the full renderer scene state to a versioned .animescene JSON file.
Result Save(const Scene& scene, const std::string& path);

// Loads a scene file. Older formats are migrated forward.
// Newer formats than this build supports are rejected with a clear error.
Result Load(Scene& scene, const std::string& path);

} // namespace SceneFile
