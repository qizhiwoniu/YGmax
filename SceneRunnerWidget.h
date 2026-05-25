#pragma once
#include <QOpenGLWidget>
#include "Viewport3D.h"   // 复用 SceneObject 类型

// 内嵌/独立窗口两用的"只读运行"视口
// 不允许编辑，仅做动画 tick + 渲染
class SceneRunnerWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT
public:
    explicit SceneRunnerWidget(
        const std::vector<std::unique_ptr<SceneObject>>& objects, // 拷贝一份快照
        QWidget* parent = nullptr);
    ~SceneRunnerWidget() override;

    void stop();   // 停止循环，可安全销毁

protected:
    void initializeGL()  override;
    void resizeGL(int w, int h) override;
    void paintGL()       override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    // 场景快照（深拷贝 mesh 数据，共享顶点内容即可）
    std::vector<std::shared_ptr<SceneObject>> m_objects;

    QOpenGLShaderProgram m_meshProg;
    OrbitCamera          m_camera;
    QTimer* m_timer = nullptr;
    float                m_time = 0.f;
    int                  m_vpW = 1, m_vpH = 1;

    void uploadObject(SceneObject& obj);
};