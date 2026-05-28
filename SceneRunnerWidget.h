#pragma once

// ═══════════════════════════════════════════════════════════════
//  SceneRunnerWidget.h  —  v2
//  修复：GPU 资源与 CPU 快照分离，确保在正确的 GL context 中创建
// ═══════════════════════════════════════════════════════════════

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QElapsedTimer>
#include <vector>
#include <memory>

#include "Viewport3D.h"   // SceneObject, OrbitCamera

// ───────────────────────────────────────────────────────────────
//  CPU 端快照（构造函数中拷贝，不含任何 GL 对象）
// ───────────────────────────────────────────────────────────────
struct RunnerSnapshot
{
    QString   name;
    QVector3D position;
    QVector3D rotation;   // 欧拉角（度）
    QVector3D scale;
    QColor    color;
    bool      visible = true;

    std::vector<float>    vertices;   // x,y,z, nx,ny,nz
    std::vector<uint32_t> indices;
};

// ───────────────────────────────────────────────────────────────
//  GPU 端资源（仅在 initializeGL 之后创建）
// ───────────────────────────────────────────────────────────────
struct RunnerGPU
{
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer            vbo { QOpenGLBuffer::VertexBuffer };
    QOpenGLBuffer            ebo { QOpenGLBuffer::IndexBuffer  };
    int indexCount = 0;
};

// ───────────────────────────────────────────────────────────────
//  SceneRunnerWidget
// ───────────────────────────────────────────────────────────────
class SceneRunnerWidget : public QOpenGLWidget,
                          protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit SceneRunnerWidget(
        const std::vector<std::unique_ptr<SceneObject>>& sourceObjects,
        QWidget* parent = nullptr);

    ~SceneRunnerWidget() override;

    void stop();

signals:
    void exitRequested();

protected:
    void initializeGL()         override;
    void resizeGL(int w, int h) override;
    void paintGL()              override;

    void mousePressEvent  (QMouseEvent* e) override;
    void mouseMoveEvent   (QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent       (QWheelEvent* e) override;
    void keyPressEvent    (QKeyEvent*   e) override;

private:
    void initShaders();
    void uploadObjects();
    void renderObjects();
    void renderHUD();

    QOpenGLShaderProgram m_shader;

    // CPU 快照（构造时填充，不依赖 GL context）
    std::vector<RunnerSnapshot>            m_snapshots;
    // GPU 资源（initializeGL 时填充，与 m_snapshots 一一对应）
    std::vector<std::unique_ptr<RunnerGPU>> m_gpu;

    OrbitCamera m_camera;
    int  m_vpW = 1, m_vpH = 1;

    QPoint m_lastMouse;
    bool   m_rotating = false;
    bool   m_panning  = false;
    bool   m_showHUD  = false;   // G 键切换显示

    QTimer        m_renderTimer;
    QElapsedTimer m_fpsTimer;
    int           m_frameCount = 0;
    float         m_fps        = 0.f;
};
