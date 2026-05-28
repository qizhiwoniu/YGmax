#include "stdafx.h"
#include "Viewport3D.h"
#include "SceneSerializer.h"   // SceneSnapshot, SceneObjectData

// ═══════════════════════════════════════════════════════════════
//  Viewport3D::takeSnapshot
//  把当前场景状态导出为纯数据快照（不含任何 OpenGL 资源），
//  供 SceneSerializer::saveScene() 使用。
//  ── 必须在 GL context 所在线程调用（与 Viewport 同线程）
// ═══════════════════════════════════════════════════════════════
SceneSnapshot Viewport3D::takeSnapshot() const
{
    SceneSnapshot snap;

    snap.camYaw    = m_camera.yaw;
    snap.camPitch  = m_camera.pitch;
    snap.camDist   = m_camera.dist;
    snap.camTarget = m_camera.target;

    snap.objects.reserve(m_objects.size());
    for (const auto& obj : m_objects)
    {
        SceneObjectData d;
        d.name     = obj->name;
        d.kind     = obj->kind;
        d.position = obj->position;
        d.rotation = obj->rotation;
        d.scale    = obj->scale;
        d.color    = obj->color;
        d.visible  = obj->visible;
        d.layer    = obj->layer;
        d.vertices = obj->vertices;   // CPU-side cache（已在 addBox/addSphere 等中保存）
        d.indices  = obj->indices;
        snap.objects.push_back(std::move(d));
    }

    return snap;
}

// ═══════════════════════════════════════════════════════════════
//  Viewport3D::loadSnapshot
//  从快照重建整个场景（清空当前场景 → 逐对象重新上传 GPU 资源）。
//  ── 必须在 GL context 所在线程调用；内部会 makeCurrent/doneCurrent
// ═══════════════════════════════════════════════════════════════
void Viewport3D::loadSnapshot(const SceneSnapshot& snap)
{
    makeCurrent();

    // 1. 清空现有场景（销毁 OpenGL 资源 + 发射 sceneCleared 给 Explorer）
    clearScene();

    // 2. 还原摄像机
    m_camera.yaw    = snap.camYaw;
    m_camera.pitch  = snap.camPitch;
    m_camera.dist   = snap.camDist;
    m_camera.target = snap.camTarget;

    // 3. 逐对象重建
    for (const SceneObjectData& d : snap.objects)
    {
        if (d.vertices.empty() || d.indices.empty()) continue;

        auto obj       = std::make_unique<SceneObject>();
        obj->name      = d.name;
        obj->kind      = d.kind;
        obj->position  = d.position;
        obj->rotation  = d.rotation;
        obj->scale     = d.scale;
        obj->color     = d.color;
        obj->visible   = d.visible;
        obj->layer     = d.layer;
        obj->vertices  = d.vertices;
        obj->indices   = d.indices;

        // 重新创建 VAO / VBO / EBO
        uploadObject(*obj, d.vertices, d.indices);

        // 通知 Explorer 同步节点树
        emit objectAdded(obj->name, obj->kind);

        m_objects.push_back(std::move(obj));
    }

    // 4. 更新统计标签
    m_statsLabel->setText(QString("%1 object(s)").arg(m_objects.size()));

    m_selectedIdx = -1;
    doneCurrent();
    update();
}
