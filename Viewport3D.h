#pragma once

// ═══════════════════════════════════════════════════════════════
//  Viewport3D.h
//  基于 QOpenGLWidget (Qt 6) + OpenGL 3.3 Core Profile
//  Visual Studio 2022 + Qt 6.10 msvc2022_64 专用
//
//  工具栏：视角选择 | Full Render / Wireframe
//  场景  ：addBox / addSphere / addPlane / addCylinder / addCone
//  摄像机：中键旋转 | Shift+中键平移 | 滚轮缩放
//  快捷键：F 聚焦选中 | Delete 删除选中
//
//  坐标原点：网格 XZ 平面交点 = 世界坐标 (0, 0, 0)
//            Y 轴向上，X 轴向右（红），Z 轴向前（蓝）
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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <vector>
#include <memory>

// ───────────────────────────────────────────────────────────────
//  渲染模式
// ───────────────────────────────────────────────────────────────
enum class RenderMode { FullRender, Wireframe };

// ───────────────────────────────────────────────────────────────
//  Gizmo 工具模式（3ds Max 风格）
//  None   — 普通选择
//  Move   — W: XYZ 平移轴（箭头）
//  Rotate — E: XYZ 旋转环（经纬线球）
//  Scale  — R: XYZ 缩放轴（方块头）
// ───────────────────────────────────────────────────────────────
enum class GizmoMode { None, Move, Rotate, Scale };

// Gizmo 轴枚举（用于拖拽时判断约束方向）
enum class GizmoAxis { None, X, Y, Z };

// ───────────────────────────────────────────────────────────────
//  物体种类（用于 Explorer 分组）
// ───────────────────────────────────────────────────────────────
enum class ObjectKind {
    Mesh,    ///< 普通几何体 → Scene Objects
    Light,   ///< 灯光       → Scene Lights
    Lua      ///< Lua 脚本实体 → Scene Lua
};

// ───────────────────────────────────────────────────────────────
//  场景对象（几何体 + GPU 资源）
// ───────────────────────────────────────────────────────────────
struct SceneObject
{
    QString    name;
    ObjectKind kind     = ObjectKind::Mesh;

    QVector3D  position { 0, 0, 0 };
    QVector3D  rotation { 0, 0, 0 };  ///< 欧拉角（度）
    QVector3D  scale    { 1, 1, 1 };
    QColor     color    { 180, 140, 80 };

    bool       visible  = true;       ///< 显示 / 隐藏（影响渲染，不删除物体）
    int        layer    = 0;          ///< 图层索引（0=Default）

    // ── CPU 端缓存（供 SceneRunnerWidget 跨 context 重新上传）──
    std::vector<float>    vertices;
    std::vector<uint32_t> indices;

    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer            vbo { QOpenGLBuffer::VertexBuffer };
    QOpenGLBuffer            ebo { QOpenGLBuffer::IndexBuffer  };
    int indexCount = 0;
};

// ───────────────────────────────────────────────────────────────
//  轻量属性快照（供 PropertyEditorPanel 读写，无 OpenGL 依赖）
// ───────────────────────────────────────────────────────────────
struct ObjectProps
{
    QVector3D position;
    QVector3D rotation;
    QVector3D scale;
    bool      visible = true;
    int       layer   = 0;
};

// ───────────────────────────────────────────────────────────────
//  轨道摄像机（3ds Max 风格）
// ───────────────────────────────────────────────────────────────
struct OrbitCamera
{
    float     yaw   =  35.f;
    float     pitch =  25.f;
    float     dist  =  12.f;
    QVector3D target{ 0, 0, 0 };   ///< 始终对准世界坐标原点方向

    QVector3D  position()                    const;
    QMatrix4x4 viewMatrix()                  const;
    QMatrix4x4 projMatrix(float a, bool ortho = false) const;
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
    void addBox     (const QString& name,
                     QVector3D pos   = {0,0,0},
                     QVector3D size  = {1,1,1},
                     QColor    color = {200,140,60});

    void addSphere  (const QString& name,
                     QVector3D pos    = {0,0,0},
                     float     radius = 1.f,
                     QColor    color  = {100,160,220});

    void addCylinder(const QString& name,
                     QVector3D pos    = {0,0,0},
                     float     radius = 0.5f,
                     float     height = 1.f,
                     QColor    color  = {120,180,120});

    void addCone    (const QString& name,
                     QVector3D pos    = {0,0,0},
                     float     radius = 0.5f,
                     float     height = 1.f,
                     QColor    color  = {180,120,180});

    void addPlane   (const QString& name,
                     QVector3D pos  = {0,0,0},
                     float     size = 5.f,
                     QColor    color = {100,100,100});

    /// 灯光占位（小黄球），kind=Light → 分到 Scene Lights
    void addLight   (const QString& name,
                     QVector3D pos  = {0,0,0},
                     QColor    color = {255,240,100});

    void clearScene();
    void setRenderMode(RenderMode mode);

    // ── 属性读写（供 PropertyEditorPanel 双向绑定）────────────
    /// 按名称读取属性快照；找不到返回 false
    bool getObjectProps(const QString& name, ObjectProps& out) const;
    /// 按名称写入属性并触发重绘；找不到返回 false
    bool setObjectProps(const QString& name, const ObjectProps& props);
    // 返回当前场景对象的只读引用，供 SceneRunnerWidget 拍快照
    const std::vector<std::unique_ptr<SceneObject>>& sceneObjects() const
    {
        return m_objects;
    }

signals:
    /// name = 物体名称，kind = 种类（用于 Explorer 分组）
    void objectAdded   (const QString& name, ObjectKind kind);
    void objectRemoved (const QString& name);
    void objectSelected(const QString& name);
    void sceneCleared  ();

    /// 属性被 Viewport 内部操作改变时发出（目前保留扩展）
    void objectPropsChanged(const QString& name);

public slots:
    /// 由 Explorer 的 deleteRequested 信号触发，按名称删除物体
    void deleteObjectByName(const QString& name);

protected:
    void initializeGL()            override;
    void resizeGL(int w, int h)    override;
    void paintGL()                 override;

    void mousePressEvent  (QMouseEvent*    e) override;
    void mouseMoveEvent   (QMouseEvent*    e) override;
    void mouseReleaseEvent(QMouseEvent*    e) override;
    void wheelEvent       (QWheelEvent*    e) override;
    void keyPressEvent    (QKeyEvent*      e) override;
    void dragEnterEvent   (QDragEnterEvent* e) override;
    void dropEvent        (QDropEvent*     e) override;

private:
    // ── 初始化 ────────────────────────────────────────────────
    void initShaders();
    void initGrid();
    void initAxes();

    // ── 渲染 ──────────────────────────────────────────────────
    void renderGrid();
    void renderAxes();
    void renderObjects();

    // ── 几何生成 ──────────────────────────────────────────────
    static void buildBox     (std::vector<float>& v, std::vector<uint32_t>& i, QVector3D half);
    static void buildSphere  (std::vector<float>& v, std::vector<uint32_t>& i, float r, int stacks=24, int slices=32);
    static void buildPlane   (std::vector<float>& v, std::vector<uint32_t>& i, float half, int segs=10);
    static void buildCylinder(std::vector<float>& v, std::vector<uint32_t>& i, float radius=0.5f, float height=1.f, int slices=32);
    static void buildCone    (std::vector<float>& v, std::vector<uint32_t>& i, float radius=0.5f, float height=1.f, int slices=32);

    // ── 右键菜单 ──────────────────────────────────────────────
    void showContextMenu(const QPoint& pos);

    // ── GPU 上传 ──────────────────────────────────────────────
    void uploadObject(SceneObject& obj,
                      const std::vector<float>&    verts,
                      const std::vector<uint32_t>& indices);

    // ── 通用内部添加（设置 kind 后调用）──────────────────────
    void finalizeAdd(std::unique_ptr<SceneObject> obj,
                     const std::vector<float>&    verts,
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

    // ── 地面网格（XZ 平面，交点即世界原点 0,0,0）────────────
    QOpenGLVertexArrayObject m_gridVAO;
    QOpenGLBuffer            m_gridVBO { QOpenGLBuffer::VertexBuffer };
    int m_gridVertCount = 0;

    // ── 坐标轴图示 ────────────────────────────────────────────
    QOpenGLVertexArrayObject m_axisVAO;
    QOpenGLBuffer            m_axisVBO { QOpenGLBuffer::VertexBuffer };

    // ── 渲染状态 ──────────────────────────────────────────────
    RenderMode m_renderMode = RenderMode::FullRender;
    bool       m_useOrtho   = false;
    int m_vpW = 1, m_vpH = 1;

    // ── 鼠标 ──────────────────────────────────────────────────
    QPoint m_lastMouse;
    bool   m_rotating = false;
    bool   m_panning  = false;

    // ── Overlay 控件 ──────────────────────────────────────────
    QWidget*   m_overlayBar  = nullptr;
    QComboBox* m_viewCombo   = nullptr;
    QComboBox* m_renderCombo = nullptr;
    QLabel*    m_statsLabel  = nullptr;

    // ── Gizmo ─────────────────────────────────────────────────
    GizmoMode  m_gizmoMode      = GizmoMode::None;
    GizmoAxis  m_gizmoAxis      = GizmoAxis::None;
    bool       m_gizmoDragging  = false;
    QPoint     m_gizmoDragStart;
    QVector3D  m_gizmoDragOrigPos;
    QVector3D  m_gizmoDragOrigRot;
    QVector3D  m_gizmoDragOrigSca;

    // Gizmo VAO/VBO（每帧动态重填，轻量）
    QOpenGLVertexArrayObject m_gizmoVAO;
    QOpenGLBuffer            m_gizmoVBO { QOpenGLBuffer::VertexBuffer };
    bool                     m_gizmoGPUReady = false;

    // ── Gizmo 内部方法 ────────────────────────────────────────
    void      renderGizmo();
    GizmoAxis pickGizmoAxis(const QPoint& screenPos);
    void      applyGizmoDrag(const QPoint& currentPos);
    QPointF   worldToScreen(const QVector3D& wp) const;
    // 几何辅助
    static void buildArrow (std::vector<float>& v, const QVector3D& dir,
                             const QVector3D& col, float len, bool scaleCube);
    static void buildRing  (std::vector<float>& v, const QVector3D& axis,
                             const QVector3D& col, float r, int segs = 48);
};
