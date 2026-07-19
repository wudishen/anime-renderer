#include "FileDialog.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#endif

namespace FileDialog {
namespace {

#ifdef _WIN32
bool RunDialog(std::string& outPath, bool save, const char* filter, const char* title, const char* defaultExt) {
    char filePath[MAX_PATH] = "";
    if (!outPath.empty() && outPath.size() < MAX_PATH) {
        // Prefill with current path when available.
#ifdef _MSC_VER
        strncpy_s(filePath, outPath.c_str(), _TRUNCATE);
#else
        std::snprintf(filePath, MAX_PATH, "%s", outPath.c_str());
#endif
    }

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_NOCHANGEDIR | (save ? (OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST)
                                        : (OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST));

    const BOOL ok = save ? GetSaveFileNameA(&ofn) : GetOpenFileNameA(&ofn);
    if (!ok) {
        return false;
    }

    outPath = filePath;
    return true;
}
#endif

} // namespace

bool OpenObjFile(std::string& outPath) {
#ifdef _WIN32
    return RunDialog(
        outPath,
        false,
        "OBJ Files (*.obj)\0*.obj\0All Files (*.*)\0*.*\0",
        "Import OBJ",
        "obj");
#else
    (void)outPath;
    return false;
#endif
}

bool OpenSceneFile(std::string& outPath) {
#ifdef _WIN32
    return RunDialog(
        outPath,
        false,
        "Anime Renderer Scene (*.animescene)\0*.animescene\0All Files (*.*)\0*.*\0",
        "Open Scene",
        "animescene");
#else
    (void)outPath;
    return false;
#endif
}

bool SaveSceneFile(std::string& outPath) {
#ifdef _WIN32
    return RunDialog(
        outPath,
        true,
        "Anime Renderer Scene (*.animescene)\0*.animescene\0All Files (*.*)\0*.*\0",
        "Save Scene",
        "animescene");
#else
    (void)outPath;
    return false;
#endif
}

bool OpenImageFile(std::string& outPath) {
#ifdef _WIN32
    return RunDialog(
        outPath,
        false,
        "Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.tga)\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files (*.*)\0*.*\0",
        "Choose Background Image",
        "png");
#else
    (void)outPath;
    return false;
#endif
}

} // namespace FileDialog
