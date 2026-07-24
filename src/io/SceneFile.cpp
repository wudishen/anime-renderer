#include "SceneFile.h"
#include "scene/CameraFactory.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace SceneFile {
namespace {

using json = nlohmann::json;

constexpr const char* kMagic = "AnimeRendererScene";

json Mat4ToJson(const glm::mat4& m) {
    json arr = json::array();
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            arr.push_back(m[col][row]);
        }
    }
    return arr;
}

bool Mat4FromJson(const json& arr, glm::mat4& out, std::string& error) {
    if (!arr.is_array() || arr.size() != 16) {
        error = "Object transform must be an array of 16 numbers.";
        return false;
    }
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            const json& value = arr[col * 4 + row];
            if (!value.is_number()) {
                error = "Object transform contains a non-number.";
                return false;
            }
            out[col][row] = value.get<float>();
        }
    }
    return true;
}

json Vec3ToJson(const glm::vec3& v) {
    return json::array({v.x, v.y, v.z});
}

const char* MeshAnchorKindToString(MeshAnchorKind kind) {
    switch (kind) {
    case MeshAnchorKind::Centroid:
        return "centroid";
    case MeshAnchorKind::Vertex:
    default:
        return "vertex";
    }
}

MeshAnchorKind MeshAnchorKindFromString(const std::string& value) {
    if (value == "centroid") {
        return MeshAnchorKind::Centroid;
    }
    return MeshAnchorKind::Vertex;
}

bool Vec3FromJson(const json& arr, glm::vec3& out, std::string& error, const char* label) {
    if (!arr.is_array() || arr.size() != 3 || !arr[0].is_number() || !arr[1].is_number() || !arr[2].is_number()) {
        error = std::string(label) + " must be an array of 3 numbers.";
        return false;
    }
    out = {arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
    return true;
}

std::string TypeToString(SceneObjectType type) {
    switch (type) {
    case SceneObjectType::Camera:
        return "camera";
    case SceneObjectType::BezierCurve:
        return "bezier";
    case SceneObjectType::BSplineCurve:
        return "bspline";
    case SceneObjectType::Mesh:
    default:
        return "mesh";
    }
}

// Migrate document root from version N to N+1.
// When the format changes, add a case here. Load upgrades in-memory JSON only;
// the file on disk is left unchanged until the user saves again.
bool MigrateToNextVersion(json& root, int fromVersion, std::string& error) {
    switch (fromVersion) {
    case 1:
        // v1 -> v2: optional camera background fields; no data rewrite required.
        root["formatVersion"] = 2;
        return true;
    case 2:
        // v2 -> v3: optional bezier/bspline objects with controlPoints.
        root["formatVersion"] = 3;
        return true;
    default:
        error = "No migration path from format version " + std::to_string(fromVersion) + ".";
        return false;
    }
}

bool MigrateToCurrent(json& root, std::string& warning, std::string& error) {
    if (!root.contains("formatVersion") || !root["formatVersion"].is_number_integer()) {
        error = "Scene file is missing a valid formatVersion.";
        return false;
    }

    int version = root["formatVersion"].get<int>();
    if (version > kCurrentFormatVersion) {
        error =
            "This scene was saved with a newer Anime Renderer format (v" +
            std::to_string(version) + "), but this build only understands up to v" +
            std::to_string(kCurrentFormatVersion) +
            ".\n\nOpen it in a newer version of the app, or re-save it from a compatible build.";
        return false;
    }
    if (version < kMinReadableFormatVersion) {
        error =
            "This scene uses format v" + std::to_string(version) +
            ", which is older than the oldest version this build can migrate (v" +
            std::to_string(kMinReadableFormatVersion) + ").";
        return false;
    }

    while (version < kCurrentFormatVersion) {
        if (!MigrateToNextVersion(root, version, error)) {
            return false;
        }
        if (!root.contains("formatVersion") || !root["formatVersion"].is_number_integer()) {
            error = "Migration failed to update formatVersion.";
            return false;
        }
        const int nextVersion = root["formatVersion"].get<int>();
        if (nextVersion <= version) {
            error = "Migration did not advance formatVersion.";
            return false;
        }
        if (!warning.empty()) {
            warning += "\n";
        }
        warning += "Upgraded scene data from format v" + std::to_string(version) +
                   " to v" + std::to_string(nextVersion) + ".";
        version = nextVersion;
    }

    return true;
}

} // namespace

Result Save(const Scene& scene, const std::string& path) {
    json root;
    root["magic"] = kMagic;
    root["formatVersion"] = kCurrentFormatVersion;
    root["activeViewCameraId"] =
        scene.GetActiveViewCameraId().has_value() ? json(*scene.GetActiveViewCameraId()) : json(nullptr);
    root["selectedId"] =
        scene.GetSelectedId().has_value() ? json(*scene.GetSelectedId()) : json(nullptr);

    json objects = json::array();
    for (const SceneObject& object : scene.GetObjects()) {
        json entry;
        entry["id"] = object.id;
        entry["name"] = object.name;
        entry["type"] = TypeToString(object.type);
        entry["color"] = Vec3ToJson(object.color);
        entry["transform"] = Mat4ToJson(object.transform);

        if (object.IsCamera()) {
            entry["cameraFovDegrees"] = object.cameraFovDegrees;
            if (object.background.HasImage()) {
                json bg;
                bg["imagePath"] = object.background.imagePath;
                bg["opacity"] = object.background.opacity;
                bg["offset"] = json::array({object.background.offset.x, object.background.offset.y});
                bg["scale"] = object.background.scale;
                bg["enabled"] = object.background.enabled;
                entry["background"] = std::move(bg);
            }
        } else if (object.IsCurve()) {
            json cps = json::array();
            for (const glm::vec3& cp : object.controlPoints) {
                cps.push_back(Vec3ToJson(cp));
            }
            entry["controlPoints"] = std::move(cps);
            if (object.IsBSplineCurve() && !object.controlPointAttachments.empty()) {
                json attachments = json::array();
                for (const ControlPointAttachment& a : object.controlPointAttachments) {
                    json item;
                    item["controlPointIndex"] = a.controlPointIndex;
                    item["meshObjectId"] = a.meshObjectId;
                    item["anchorKind"] = MeshAnchorKindToString(a.anchorKind);
                    item["vertexIndex"] = a.vertexIndex;
                    attachments.push_back(std::move(item));
                }
                entry["controlPointAttachments"] = std::move(attachments);
            }
            if (object.IsBSplineCurve() && !object.fitPointAttachments.empty()) {
                json attachments = json::array();
                for (const FitPointAttachment& a : object.fitPointAttachments) {
                    json item;
                    item["fitPointIndex"] = a.fitPointIndex;
                    item["meshObjectId"] = a.meshObjectId;
                    item["anchorKind"] = MeshAnchorKindToString(a.anchorKind);
                    item["vertexIndex"] = a.vertexIndex;
                    attachments.push_back(std::move(item));
                }
                entry["fitPointAttachments"] = std::move(attachments);
            }
        } else {
            json vertices = json::array();
            for (const glm::vec3& v : object.vertices) {
                vertices.push_back(Vec3ToJson(v));
            }
            entry["vertices"] = vertices;
        }

        objects.push_back(std::move(entry));
    }
    root["objects"] = std::move(objects);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return {false, "Failed to write scene file:\n" + path, {}};
    }

    out << root.dump(2);
    if (!out) {
        return {false, "Failed while writing scene file:\n" + path, {}};
    }

    return {true, {}, {}};
}

Result Load(Scene& scene, const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {false, "Failed to open scene file:\n" + path, {}};
    }

    json root;
    try {
        in >> root;
    } catch (const json::exception& ex) {
        return {false, std::string("Invalid scene JSON:\n") + ex.what(), {}};
    }

    if (!root.is_object() || !root.contains("magic") || root["magic"] != kMagic) {
        return {false, "Not a valid Anime Renderer scene file.", {}};
    }

    std::string warning;
    std::string error;
    if (!MigrateToCurrent(root, warning, error)) {
        return {false, error, {}};
    }

    if (!root.contains("objects") || !root["objects"].is_array()) {
        return {false, "Scene file is missing an objects array.", {}};
    }

    Scene loaded;
    int skippedUnknown = 0;

    for (const json& entry : root["objects"]) {
        if (!entry.is_object()) {
            continue;
        }

        SceneObject object;
        object.id = entry.value("id", 0);
        object.name = entry.value("name", "Object");
        object.cameraFovDegrees = entry.value("cameraFovDegrees", 45.0f);

        const std::string type = entry.value("type", "mesh");
        if (type == "camera") {
            object.type = SceneObjectType::Camera;
            object.color = {0.35f, 0.75f, 0.95f};
            object.vertices = CameraFactory::GizmoVertices();
        } else if (type == "mesh") {
            object.type = SceneObjectType::Mesh;
            if (entry.contains("vertices") && entry["vertices"].is_array()) {
                for (const json& vertexJson : entry["vertices"]) {
                    glm::vec3 vertex{};
                    if (!Vec3FromJson(vertexJson, vertex, error, "Vertex")) {
                        return {false, error, warning};
                    }
                    object.vertices.push_back(vertex);
                }
            }
        } else if (type == "bezier" || type == "bspline") {
            object.type =
                (type == "bezier") ? SceneObjectType::BezierCurve : SceneObjectType::BSplineCurve;
            object.color = (type == "bezier") ? glm::vec3{0.95f, 0.85f, 0.25f}
                                              : glm::vec3{0.35f, 0.85f, 0.95f};
            if (entry.contains("controlPoints") && entry["controlPoints"].is_array()) {
                for (const json& cpJson : entry["controlPoints"]) {
                    glm::vec3 cp{};
                    if (!Vec3FromJson(cpJson, cp, error, "Control point")) {
                        return {false, error, warning};
                    }
                    object.controlPoints.push_back(cp);
                }
            }
            const size_t minCps = object.IsBezierCurve() ? 4 : 4;
            if (object.controlPoints.size() < minCps) {
                if (!warning.empty()) {
                    warning += "\n";
                }
                warning += "Skipped curve '" + object.name + "' with too few control points.";
                ++skippedUnknown;
                continue;
            }
            if (object.IsBSplineCurve() &&
                entry.contains("controlPointAttachments") &&
                entry["controlPointAttachments"].is_array()) {
                for (const json& item : entry["controlPointAttachments"]) {
                    if (!item.is_object()) {
                        continue;
                    }
                    ControlPointAttachment attach;
                    attach.controlPointIndex = item.value("controlPointIndex", -1);
                    attach.meshObjectId = item.value("meshObjectId", -1);
                    attach.anchorKind = MeshAnchorKindFromString(item.value("anchorKind", "vertex"));
                    attach.vertexIndex = item.value("vertexIndex", -1);
                    if (attach.anchorKind == MeshAnchorKind::Centroid) {
                        attach.vertexIndex = -1;
                    }
                    if (attach.controlPointIndex >= 0 &&
                        attach.controlPointIndex < static_cast<int>(object.controlPoints.size())) {
                        object.controlPointAttachments.push_back(attach);
                    }
                }
            }
            if (object.IsBSplineCurve() &&
                entry.contains("fitPointAttachments") &&
                entry["fitPointAttachments"].is_array()) {
                for (const json& item : entry["fitPointAttachments"]) {
                    if (!item.is_object()) {
                        continue;
                    }
                    FitPointAttachment attach;
                    attach.fitPointIndex = item.value("fitPointIndex", -1);
                    attach.meshObjectId = item.value("meshObjectId", -1);
                    attach.anchorKind = MeshAnchorKindFromString(item.value("anchorKind", "vertex"));
                    attach.vertexIndex = item.value("vertexIndex", -1);
                    if (attach.anchorKind == MeshAnchorKind::Centroid) {
                        attach.vertexIndex = -1;
                    }
                    if (attach.fitPointIndex >= 0 &&
                        attach.fitPointIndex < object.FitPointCount()) {
                        object.fitPointAttachments.push_back(attach);
                    }
                }
            }
        } else {
            // Forward-compatible: ignore object kinds this build does not understand.
            ++skippedUnknown;
            continue;
        }

        if (entry.contains("color")) {
            if (!Vec3FromJson(entry["color"], object.color, error, "Color")) {
                return {false, error, warning};
            }
        }

        if (!entry.contains("transform") || !Mat4FromJson(entry["transform"], object.transform, error)) {
            return {false, error.empty() ? "Object is missing transform." : error, warning};
        }

        if (object.IsCamera() && entry.contains("background") && entry["background"].is_object()) {
            const json& bg = entry["background"];
            object.background.opacity = bg.value("opacity", 0.5f);
            object.background.scale = bg.value("scale", 1.0f);
            object.background.enabled = bg.value("enabled", true);
            if (bg.contains("offset") && bg["offset"].is_array() && bg["offset"].size() == 2 &&
                bg["offset"][0].is_number() && bg["offset"][1].is_number()) {
                object.background.offset = {
                    bg["offset"][0].get<float>(),
                    bg["offset"][1].get<float>(),
                };
            }

            if (bg.contains("imagePath") && bg["imagePath"].is_string()) {
                const std::string savedPath = bg["imagePath"].get<std::string>();
                std::filesystem::path imagePath(savedPath);
                if (!std::filesystem::exists(imagePath)) {
                    imagePath = std::filesystem::path(path).parent_path() / savedPath;
                }
                if (std::filesystem::exists(imagePath)) {
                    if (!object.background.LoadFromFile(imagePath.string())) {
                        if (!warning.empty()) {
                            warning += "\n";
                        }
                        warning += "Failed to load background image for '" + object.name + "': " + imagePath.string();
                    }
                } else {
                    if (!warning.empty()) {
                        warning += "\n";
                    }
                    warning += "Missing background image for '" + object.name + "': " + savedPath;
                }
            }
        }

        if (!object.IsCurve()) {
            object.UploadMesh();
        }
        loaded.AddObject(std::move(object));
    }

    if (root.contains("selectedId") && !root["selectedId"].is_null() && root["selectedId"].is_number_integer()) {
        loaded.SetSelectedId(root["selectedId"].get<int>());
    }

    if (root.contains("activeViewCameraId") && !root["activeViewCameraId"].is_null() &&
        root["activeViewCameraId"].is_number_integer()) {
        loaded.SetActiveViewCameraId(root["activeViewCameraId"].get<int>());
    }

    if (!loaded.GetActiveViewCamera()) {
        for (const SceneObject& object : loaded.GetObjects()) {
            if (object.IsCamera()) {
                loaded.SetActiveViewCameraId(object.id);
                break;
            }
        }
    }

    if (!loaded.GetActiveViewCamera()) {
        SceneObject& camera = loaded.AddObject(CameraFactory::Create(CameraFactory::Preset::Free, 3.0f));
        loaded.SetActiveViewCameraId(camera.id);
        if (!warning.empty()) {
            warning += "\n";
        }
        warning += "No usable camera was found in the file; a default camera was created.";
    }

    if (skippedUnknown > 0) {
        if (!warning.empty()) {
            warning += "\n";
        }
        warning += "Skipped " + std::to_string(skippedUnknown) +
                   " object(s) with unknown types (saved by a newer app feature).";
    }

    const auto selectedId = loaded.GetSelectedId();
    const auto activeViewId = loaded.GetActiveViewCameraId();
    scene.Clear();
    for (SceneObject& object : loaded.GetObjects()) {
        scene.AddObject(std::move(object));
    }
    scene.SetSelectedId(selectedId);
    scene.SetActiveViewCameraId(activeViewId);

    return {true, {}, warning};
}

} // namespace SceneFile
