#pragma once

#include <string>

namespace FileDialog {

// Opens a native file browser filtered to .obj files.
// Returns true and writes the selected path when the user confirms.
bool OpenObjFile(std::string& outPath);

bool OpenSceneFile(std::string& outPath);
bool SaveSceneFile(std::string& outPath);
bool OpenImageFile(std::string& outPath);

} // namespace FileDialog
