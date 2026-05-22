#pragma once

// ─────────────────────────────────────────────────────────────
//  Viewport3D.h
//  中央 3-D Viewport 面板。
//
//  当前实现为「占位渲染器」：用 QPainter 画出与参考截图一致的
//  暗色网格地面 + 坐标轴 + 摄像机/渲染模式工具栏，
//  便于在集成 OpenGL/bgfx/Vulkan 之前保持 UI 完整可用。
//
//  后续接入真正的图形后端只需：
//    1. 把基类换成 QOpenGLWidget / QWindow（Vulkan）
//    2. 重写 paintGL() / render()
//    3. 保留本文件的 overlay 工具栏部分
// ─────────────────────────────────────────────────────────────

#include <QWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QComboBox>

class Viewport3D : public QWidget
{
    Q_OBJECT

public:
    explicit Viewport3D(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event)   override;
    void mouseMoveEvent(QMouseEvent* event)    override;
    void wheelEvent(QWheelEvent* event)        override;

private:
    void drawGrid(QPainter& p);
    void drawAxes(QPainter& p);
    void buildOverlayToolBar();

    // ── Overlay 工具栏 ────────────────────────────────────
    QWidget*     m_overlayBar   = nullptr;
    QComboBox*   m_viewCombo    = nullptr;   // Persp / Top / Front …
    QComboBox*   m_renderCombo  = nullptr;   // Full Render / Wireframe …

    // ── 鼠标交互（简易轨道控制，供后续 3D 后端复用）────────
    QPoint m_lastMousePos;
    float  m_orbitYaw   = 30.f;
    float  m_orbitPitch = 20.f;
    float  m_zoomDist   = 10.f;
};
