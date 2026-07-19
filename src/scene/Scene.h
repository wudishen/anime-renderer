#pragma once

#include "SceneObject.h"
#include <optional>
#include <vector>

class Scene {
public:
    SceneObject& AddObject(SceneObject object);
    bool RemoveObject(int id);
    void Clear();

    const std::vector<SceneObject>& GetObjects() const { return m_Objects; }
    std::vector<SceneObject>& GetObjects() { return m_Objects; }

    void SetSelectedId(std::optional<int> id) { m_SelectedId = id; }
    std::optional<int> GetSelectedId() const { return m_SelectedId; }
    bool IsSelected(int id) const { return m_SelectedId.has_value() && *m_SelectedId == id; }

    void SetActiveViewCameraId(std::optional<int> id) { m_ActiveViewCameraId = id; }
    std::optional<int> GetActiveViewCameraId() const { return m_ActiveViewCameraId; }
    SceneObject* GetActiveViewCamera();
    const SceneObject* GetActiveViewCamera() const;

    SceneObject* FindById(int id);
    const SceneObject* FindById(int id) const;

private:
    std::vector<SceneObject> m_Objects;
    std::optional<int> m_SelectedId;
    std::optional<int> m_ActiveViewCameraId;
    int m_NextId = 1;
};
