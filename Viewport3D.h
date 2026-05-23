#pragma once

// ═══════════════════════════════════════════════════════════════
//  Viewport3D.h
//  基于 QOpenGLWidget (Qt 6) + OpenGL 3.3 Core Profile
//  Visual Studio 2022 + Qt 6.10 msvc2022_64 专用
//
//  工具栏：视角选择 | Full Render / Wireframe  （去掉了 Play / 全屏）
//  场景  ：addBox / addSphere / addPlane
//  摄像机：中键旋转 | Shift+中键平移 | 滚轮缩放
//  快捷键：F 聚焦选中 | Delete 删除选中
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
#include <QHBoxLayout>
#include <QToolButton>
#include <QComboBox>
#include <QLabel>
#include <vector>
#include <memory>

// ───────────────────────────────────────────────────────────────
//  渲染模式
// ───────────────────────────────────────────────────────────────
enum class RenderMode { FullRender, Wireframe };

// ───────────────────────────────────────────────────────────────
//  场景对象（几何体 + GPU资源）
// ───────────────────────────────────────────────────────────────
struct SceneObject
{
    QString   name;
    QVector3D position{ 0,0,0 };
    QVector3D rotation{ 0,0,0 };   // 欧拉角（度）
    QVector3D scale   { 1,1,1 };
    QColor    color   { 180,140,80 };

    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer            vbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLBuffer            ebo{ QOpenGLBuffer::IndexBuffer  };
    int indexCount = 0;
};

// ───────────────────────────────────────────────────────────────
//  轨道摄像机（3ds Max 风格）
// ───────────────────────────────────────────────────────────────
struct OrbitCamera
{
    float     yaw   =  35.f;
    float     pitch =  25.f;
    float     dist  =  12.f;
    QVector3D target{ 0,0,0 };

    QVector3D  position()          const;
    QMatrix4x4 viewMatrix()        const;
    QMatrix4x4 projMatrix(float a) const;
};

// ───────────────────────────────────────────────────────────────
//  Viewport3D
// ───────────────────────────────────────────────────────────────
class Viewport3D : public QOpenGLWidget,
                   protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit Viewport3D(QWidget* parent = nullptr);
    ~Viewport3D() override;

    // ── 场景 API ──────────────────────────────────────────────
    void addBox   (const QString& name,
                   QVector3D pos  = {0,0,0},
                   QVector3D size = {1,1,1},
                   QColor color   = {200,140,60});

    void addSphere(const QString& name,
                   QVector3D pos  = {0,0,0},
                   float radius   = 1.f,
                   QColor color   = {100,160,220});

    void addPlane (const QString& name,
                   QVector3D pos  = {0,0,0},
                   float size     = 5.f,
                   QColor color   = {100,100,100});

    void clearScene();
    void setRenderMode(RenderMode mode);

signals:
    void objectSelected(const QString& name);   // 供 PropertyEditor / Explorer 联动

protected:
    void initializeGL()           override;
    void resizeGL(int w, int h)   override;
    void paintGL()                override;

    void mousePressEvent  (QMouseEvent*  e) override;
    void mouseMoveEvent   (QMouseEvent*  e) override;
    void mouseReleaseEvent(QMouseEvent*  e) override;
    void wheelEvent       (QWheelEvent*  e) override;
    void keyPressEvent    (QKeyEvent*    e) override;

private:
    // ── 初始化 ────────────────────────────────────────────────
    void initShaders();
    void initGrid();
    void initAxes();

    // ── 渲染 ──────────────────────────────────────────────────
    void renderGrid();
    void renderAxes();
    void renderObjects();

    // ── 几何生成（静态工具函数）──────────────────────────────
    static void buildBox   (std::vector<float>& v, std::vector<uint32_t>& i, QVector3D half);
    static void buildSphere(std::vector<float>& v, std::vector<uint32_t>& i, float r, int stacks=24, int slices=32);
    static void buildPlane (std::vector<float>& v, std::vector<uint32_t>& i, float half, int segs=10);

    // ── GPU 上传 ──────────────────────────────────────────────
    void uploadObject(SceneObject& obj,
                      const std::vector<float>& verts,
                      const std::vector<uint32_t>& indices);

    // ── 点击拾取 ──────────────────────────────────────────────
    void pickObject(const QPoint& screenPos);

    // ── Overlay 工具栏 ────────────────────────────────────────
    void buildOverlayToolBar();

    // ── Shader ────────────────────────────────────────────────
    QOpenGLShaderProgram m_meshShader;   // Blinn-Phong
    QOpenGLShaderProgram m_flatShader;   // 无光照（网格/轴）

    // ── 场景数据 ──────────────────────────────────────────────
    std::vector<std::unique_ptr<SceneObject>> m_objects;
    int m_selectedIdx = -1;

    // ── 摄像机 ────────────────────────────────────────────────
    OrbitCamera m_camera;

    // ── 地面网格 ──────────────────────────────────────────────
    QOpenGLVertexArrayObject m_gridVAO;
    QOpenGLBuffer            m_gridVBO{ QOpenGLBuffer::VertexBuffer };
    int m_gridVertCount = 0;

    // ── 坐标轴图示 ────────────────────────────────────────────
    QOpenGLVertexArrayObject m_axisVAO;
    QOpenGLBuffer            m_axisVBO{ QOpenGLBuffer::VertexBuffer };

    // ── 渲染状态 ──────────────────────────────────────────────
    RenderMode m_renderMode = RenderMode::FullRender;
    int m_vpW = 1, m_vpH = 1;

    // ── 鼠标 ──────────────────────────────────────────────────
    QPoint m_lastMouse;
    bool   m_rotating = false;
    bool   m_panning  = false;

    // ── Overlay 控件 ──────────────────────────────────────────
    QWidget*   m_overlayBar   = nullptr;
    QComboBox* m_viewCombo    = nullptr;
    QComboBox* m_renderCombo  = nullptr;
    QLabel*    m_statsLabel   = nullptr;
};
