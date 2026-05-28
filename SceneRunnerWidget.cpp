#include "stdafx.h"
#include "SceneRunnerWidget.h"
#include "SceneShaders.h"   // kMeshVert / kMeshFrag

#include <QPainter>
#include <QtMath>

// ═══════════════════════════════════════════════════════════════
//  构造：只做 CPU 快照，绝不碰任何 GL 对象
// ═══════════════════════════════════════════════════════════════
SceneRunnerWidget::SceneRunnerWidget(
    const std::vector<std::unique_ptr<SceneObject>>& src,
    QWidget* parent)
    : QOpenGLWidget(parent)
{
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSamples(4);
    fmt.setDepthBufferSize(24);
    setFormat(fmt);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // ── CPU 快照（此时没有 GL context，不能创建任何 GL 对象）──
    for (const auto& o : src) {
        if (!o || o->vertices.empty() || o->indices.empty()) continue;
        RunnerSnapshot snap;
        snap.name     = o->name;
        snap.position = o->position;
        snap.rotation = o->rotation;
        snap.scale    = o->scale;
        snap.color    = o->color;
        snap.visible  = o->visible;
        snap.vertices = o->vertices;   // 深拷贝
        snap.indices  = o->indices;
        m_snapshots.push_back(std::move(snap));
    }

    connect(&m_renderTimer, &QTimer::timeout, this, [this]() {
        ++m_frameCount;
        if (m_fpsTimer.elapsed() >= 1000) {
            m_fps = m_frameCount * 1000.f / (float)m_fpsTimer.restart();
            m_frameCount = 0;
        }
        update();
    });
}

SceneRunnerWidget::~SceneRunnerWidget()
{
    stop();
    makeCurrent();
    for (auto& g : m_gpu) {
        g->vao.destroy();
        g->vbo.destroy();
        g->ebo.destroy();
    }
    doneCurrent();
}

void SceneRunnerWidget::stop()
{
    m_renderTimer.stop();
}

// ═══════════════════════════════════════════════════════════════
//  initializeGL —— 此时 GL context 已就绪，才创建 GPU 资源
// ═══════════════════════════════════════════════════════════════
void SceneRunnerWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.13f, 0.13f, 0.14f, 1.f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    initShaders();
    uploadObjects();   // 快照 → GPU

    m_fpsTimer.start();
    m_renderTimer.start(16);
}

void SceneRunnerWidget::initShaders()
{
    m_shader.addShaderFromSourceCode(QOpenGLShader::Vertex,   kMeshVert);
    m_shader.addShaderFromSourceCode(QOpenGLShader::Fragment, kMeshFrag);
    if (!m_shader.link())
        qWarning() << "[Runner] shader link:" << m_shader.log();
}

// ── 关键修复：VBO 必须在 VAO 绑定期间绑定；
//             EBO 也必须在 VAO 绑定期间绑定（Core Profile 要求）；
//             VAO release 之前不要 release EBO（否则 VAO 不记录 EBO）
void SceneRunnerWidget::uploadObjects()
{
    m_gpu.clear();
    m_gpu.reserve(m_snapshots.size());

    for (const auto& snap : m_snapshots)
    {
        auto g = std::make_unique<RunnerGPU>();

        // 1. 先创建并绑定 VAO
        g->vao.create();
        g->vao.bind();

        // 2. 创建、绑定、上传 VBO（GL_ARRAY_BUFFER）
        g->vbo.create();
        g->vbo.bind();
        g->vbo.allocate(snap.vertices.data(),
                        (int)(snap.vertices.size() * sizeof(float)));

        // 3. 创建、绑定、上传 EBO（GL_ELEMENT_ARRAY_BUFFER）
        //    必须在 VAO 仍然绑定时完成，VAO 才会记住这个 EBO
        g->ebo.create();
        g->ebo.bind();
        g->ebo.allocate(snap.indices.data(),
                        (int)(snap.indices.size() * sizeof(uint32_t)));

        // 4. 顶点属性指针（在 VBO 绑定期间设置）
        //    stride = 6 floats: [x, y, z, nx, ny, nz]
        glEnableVertexAttribArray(0);   // position
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);   // normal
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                              6 * sizeof(float), (void*)(3 * sizeof(float)));

        // 5. 释放 VAO（此时 EBO 仍绑定，VAO 已记录；VBO 由 VAO 间接记录）
        //    注意：不在这里 release VBO/EBO，避免驱动差异
        g->vao.release();

        // 6. release VBO/EBO（VAO 已解绑，现在 release 安全）
        g->vbo.release();
        g->ebo.release();

        g->indexCount = (int)snap.indices.size();
        m_gpu.push_back(std::move(g));
    }
}

// ═══════════════════════════════════════════════════════════════
//  resizeGL / paintGL
// ═══════════════════════════════════════════════════════════════
void SceneRunnerWidget::resizeGL(int w, int h)
{
    m_vpW = qMax(w, 1);
    m_vpH = qMax(h, 1);
    glViewport(0, 0, m_vpW, m_vpH);
}

void SceneRunnerWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderObjects();
    renderHUD();
}

void SceneRunnerWidget::renderObjects()
{
    if (m_snapshots.empty()) return;

    float asp = (float)m_vpW / (float)m_vpH;
    QMatrix4x4 proj = m_camera.projMatrix(asp);
    QMatrix4x4 view = m_camera.viewMatrix();
    QVector3D  cam  = m_camera.position();
    QVector3D  ldir = QVector3D(0.55f, 1.f, 0.45f).normalized();

    m_shader.bind();
    m_shader.setUniformValue("uLightDir",  ldir);
    m_shader.setUniformValue("uViewPos",   cam);
    m_shader.setUniformValue("uWireframe", false);

    for (int i = 0; i < (int)m_snapshots.size(); ++i)
    {
        const auto& snap = m_snapshots[i];
        const auto& g    = m_gpu[i];

        if (!snap.visible || g->indexCount == 0) continue;

        QMatrix4x4 model;
        model.translate(snap.position);
        model.rotate(snap.rotation.x(), 1, 0, 0);
        model.rotate(snap.rotation.y(), 0, 1, 0);
        model.rotate(snap.rotation.z(), 0, 0, 1);
        model.scale(snap.scale);

        QVector3D col(snap.color.redF(),
                      snap.color.greenF(),
                      snap.color.blueF());

        m_shader.setUniformValue("uMVP",       proj * view * model);
        m_shader.setUniformValue("uModel",     model);
        m_shader.setUniformValue("uNormalMat", model.normalMatrix());
        m_shader.setUniformValue("uColor",     col);
        m_shader.setUniformValue("uSelected",  false);

        g->vao.bind();
        glDrawElements(GL_TRIANGLES, g->indexCount, GL_UNSIGNED_INT, nullptr);
        g->vao.release();
    }

    m_shader.release();
}

void SceneRunnerWidget::renderHUD()
{
    if (!m_showHUD) return;

    int visible = 0;
    for (auto& s : m_snapshots) if (s.visible) ++visible;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing);

    p.fillRect(0, 0, width(), 32, QColor(0, 0, 0, 120));

    p.setPen(QColor(200, 200, 200));
    p.setFont(QFont("Segoe UI", 10));
    p.drawText(12, 20, QString("▶  运行中    物体: %1").arg(visible));
    p.drawText(width() - 80, 20, QString("FPS: %1").arg((int)m_fps));

    p.end();
}

// ═══════════════════════════════════════════════════════════════
//  鼠标 / 键盘
// ═══════════════════════════════════════════════════════════════
void SceneRunnerWidget::mousePressEvent(QMouseEvent* e)
{
    m_lastMouse = e->pos();
    if (e->button() == Qt::MiddleButton) {
        bool shift = e->modifiers() & Qt::ShiftModifier;
        m_rotating = !shift;
        m_panning  =  shift;
    }
    e->accept();
}

void SceneRunnerWidget::mouseMoveEvent(QMouseEvent* e)
{
    QPoint delta = e->pos() - m_lastMouse;
    m_lastMouse  = e->pos();

    if (m_rotating) {
        m_camera.yaw   -= delta.x() * 0.4f;
        m_camera.pitch += delta.y() * 0.4f;
        m_camera.pitch  = qBound(-89.f, m_camera.pitch, 89.f);
    } else if (m_panning) {
        QMatrix4x4 view = m_camera.viewMatrix();
        QVector3D right(view(0,0), view(0,1), view(0,2));
        QVector3D up   (view(1,0), view(1,1), view(1,2));
        float spd = m_camera.dist * 0.0015f;
        m_camera.target -= right * delta.x() * spd;
        m_camera.target += up    * delta.y() * spd;
    }
    e->accept();
}

void SceneRunnerWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::MiddleButton)
        m_rotating = m_panning = false;
    e->accept();
}

void SceneRunnerWidget::wheelEvent(QWheelEvent* e)
{
    float delta = e->angleDelta().y() / 120.f;
    m_camera.dist = qBound(0.5f, m_camera.dist - delta * m_camera.dist * 0.12f, 500.f);
    e->accept();
}

void SceneRunnerWidget::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape)
        emit exitRequested();
    else if (e->key() == Qt::Key_G)
        m_showHUD = !m_showHUD;
    else
        QOpenGLWidget::keyPressEvent(e);
}
