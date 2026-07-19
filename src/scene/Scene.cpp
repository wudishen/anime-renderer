#include "Scene.h"
#include <algorithm>

SceneObject& Scene::AddObject(SceneObject object) {
    if (object.id == 0) {
        object.id = m_NextId++;
    } else {
        m_NextId = std::max(m_NextId, object.id + 1);
    }
    m_Objects.push_back(std::move(object));
    return m_Objects.back();
}

void Scene::Clear() {
    m_Objects.clear();
    m_SelectedId.reset();
    m_ActiveViewCameraId.reset();
    m_NextId = 1;
}

bool Scene::RemoveObject(int id) {
    const auto it = std::find_if(m_Objects.begin(), m_Objects.end(), [id](const SceneObject& object) {
        return object.id == id;
    });
    if (it == m_Objects.end()) {
        return false;
    }

    m_Objects.erase(it);
    if (m_SelectedId.has_value() && *m_SelectedId == id) {
        m_SelectedId.reset();
    }
    if (m_ActiveViewCameraId.has_value() && *m_ActiveViewCameraId == id) {
        m_ActiveViewCameraId.reset();
    }
    return true;
}

SceneObject* Scene::GetActiveViewCamera() {
    if (!m_ActiveViewCameraId.has_value()) {
        return nullptr;
    }
    SceneObject* object = FindById(*m_ActiveViewCameraId);
    if (!object || !object->IsCamera()) {
        m_ActiveViewCameraId.reset();
        return nullptr;
    }
    return object;
}

const SceneObject* Scene::GetActiveViewCamera() const {
    if (!m_ActiveViewCameraId.has_value()) {
        return nullptr;
    }
    const SceneObject* object = FindById(*m_ActiveViewCameraId);
    if (!object || !object->IsCamera()) {
        return nullptr;
    }
    return object;
}

SceneObject* Scene::FindById(int id) {
    for (SceneObject& object : m_Objects) {
        if (object.id == id) {
            return &object;
        }
    }
    return nullptr;
}

const SceneObject* Scene::FindById(int id) const {
    for (const SceneObject& object : m_Objects) {
        if (object.id == id) {
            return &object;
        }
    }
    return nullptr;
}
