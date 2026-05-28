#include "stdafx.h"
#include "Viewport3D.h"
#include "SceneShaders.h"
#include <QtMath>
#include <cmath>
#include <QMenu>
#include <QAction>

// ═══════════════════════════════════════════════════════════════
//  GLSL — Flat Shader（网格线 / 坐标轴，顶点色，无光照）
// ═══════════════════════════════════════════════════════════════
static const char* kFlatVert = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aColor;

uniform mat4 uMVP;
out vec3 vColor;

void main()
{
    vColor      = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

static const char* kFlatFrag = R"GLSL(
#version 330 core
in  vec3 vColor;
out vec4 fragColor;
uniform float uAlpha;

void main()
{
    fragColor = vec4(vColor, uAlpha);
}
)GLSL";

// ═══════════════════════════════════════════════════════════════
//  OrbitCamera
// ═══════════════════════════════════════════════════════════════
QVector3D OrbitCamera::position() const
{
    float yr = qDegreesToRadians(yaw);
    float pr = qDegreesToRadians(pitch);
    return target + QVector3D(
        dist * cosf(pr) * sinf(yr),
        dist * sinf(pr),
        dist * cosf(pr) * cosf(yr)
    );
}

QMatrix4x4 OrbitCamera::viewMatrix() const
{
    QMatrix4x4 m;
    m.lookAt(position(), target, {0,1,0});
    return m;
}

QMatrix4x4 OrbitCamera::projMatrix(float aspect, bool ortho) const
{
    QMatrix4x4 m;
    if (ortho) {
        float h = dist * 0.5f;
        m.ortho(-h * aspect, h * aspect, -h, h, 0.05f, 1000.f);
    } else {
        m.perspective(45.f, aspect, 0.05f, 1000.f);
    }
    return m;
}

// ═══════════════════════════════════════════════════════════════
//  构造 / 析构
// ═══════════════════════════════════════════════════════════════
Viewport3D::Viewport3D(QWidget* parent)
    : QOpenGLWidget(parent)
{
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSamples(4);
    fmt.setDepthBufferSize(24);
    setFormat(fmt);

    setMinimumSize(300, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);

    buildOverlayToolBar();
}

Viewport3D::~Viewport3D()
{
    makeCurrent();
    m_gridVAO.destroy(); m_gridVBO.destroy();
    m_axisVAO.destroy(); m_axisVBO.destroy();
    if (m_gizmoGPUReady) { m_gizmoVAO.destroy(); m_gizmoVBO.destroy(); }
    for (auto& o : m_objects) {
        o->vao.destroy();
        o->vbo.destroy();
        o->ebo.destroy();
    }
    doneCurrent();
}

// ═══════════════════════════════════════════════════════════════
//  Overlay 工具栏
// ═══════════════════════════════════════════════════════════════
void Viewport3D::buildOverlayToolBar()
{
    m_overlayBar = new QWidget(this);
    m_overlayBar->setFixedHeight(28);
    m_overlayBar->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_overlayBar->setStyleSheet(R"(
        QWidget          { background: rgba(22,22,26,210); }
        QComboBox        {
            background: transparent; border: none;
            color: #d0d0d0; font-size: 12px; padding: 0 6px;
        }
        QComboBox::drop-down { border:none; width:14px; }
        QComboBox QAbstractItemView {
            background: #2a2a2e; color: #d0d0d0;
            selection-background-color: #0e639c;
            border: 1px solid #444;
        }
        QLabel { color: #555; font-size: 12px; padding: 0 4px; }
        QLabel#stats { color: #777; font-size: 11px; padding: 0 8px; }
    )");

    auto* lay = new QHBoxLayout(m_overlayBar);
    lay->setContentsMargins(8, 0, 8, 0);
    lay->setSpacing(2);

    // 视角选择
    m_viewCombo = new QComboBox(m_overlayBar);
    m_viewCombo->addItems({ tr("透视"), tr("正交"), tr("上"), tr("下"),
                             tr("前"), tr("后"), tr("左"), tr("右") });
    m_viewCombo->setFixedWidth(72);

    connect(m_viewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        m_useOrtho = (idx == 1);
        switch (idx) {
        case 0: m_camera.yaw =  45.f; m_camera.pitch = 30.f;   break;
        case 1:                                                  break;
        case 2: m_camera.yaw =   0.f; m_camera.pitch = 89.9f;  break;
        case 3: m_camera.yaw =   0.f; m_camera.pitch = -89.9f; break;
        case 4: m_camera.yaw =   0.f; m_camera.pitch = 0.f;    break;
        case 5: m_camera.yaw = 180.f; m_camera.pitch = 0.f;    break;
        case 6: m_camera.yaw = -90.f; m_camera.pitch = 0.f;    break;
        case 7: m_camera.yaw =  90.f; m_camera.pitch = 0.f;    break;
        }
        update();
    });
    lay->addWidget(m_viewCombo);

    auto* sep = new QLabel("|", m_overlayBar);
    lay->addWidget(sep);

    // 渲染模式
    m_renderCombo = new QComboBox(m_overlayBar);
    m_renderCombo->addItems({ tr("Full Render"), tr("Wireframe") });
    m_renderCombo->setFixedWidth(108);
    lay->addWidget(m_renderCombo);

    connect(m_renderCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        setRenderMode(idx == 0 ? RenderMode::FullRender : RenderMode::Wireframe);
    });

    lay->addStretch(1);

    m_statsLabel = new QLabel("0 objects", m_overlayBar);
    m_statsLabel->setObjectName("stats");
    lay->addWidget(m_statsLabel);
}

// ═══════════════════════════════════════════════════════════════
//  initializeGL
// ═══════════════════════════════════════════════════════════════
void Viewport3D::initializeGL()
{
    initializeOpenGLFunctions();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.172f, 0.172f, 0.180f, 1.f);

    initShaders();
    initGrid();
    initAxes();

    // Gizmo VAO（内容每帧重填，这里只创建容器）
    m_gizmoVAO.create();
    m_gizmoVBO.create();
    m_gizmoVAO.bind();
    m_gizmoVBO.bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float),
                          (void*)(3*sizeof(float)));
    m_gizmoVAO.release();
    m_gizmoGPUReady = true;
}

void Viewport3D::initShaders()
{
    m_meshShader.addShaderFromSourceCode(QOpenGLShader::Vertex,   kMeshVert);
    m_meshShader.addShaderFromSourceCode(QOpenGLShader::Fragment, kMeshFrag);
    m_meshShader.link();

    m_flatShader.addShaderFromSourceCode(QOpenGLShader::Vertex,   kFlatVert);
    m_flatShader.addShaderFromSourceCode(QOpenGLShader::Fragment, kFlatFrag);
    m_flatShader.link();
}

// ─── 地面网格（XZ 平面，20×20 格）
//     网格中心 = X 轴线与 Z 轴线的交叉点 = 世界坐标 (0, 0, 0)
//     X 轴覆盖线：红色  Z 轴覆盖线：蓝色
// ─────────────────────────────────────────────────────────────
void Viewport3D::initGrid()
{
    const int   half = 10;
    const float step = 1.f;
    std::vector<float> v;   // x,y,z,r,g,b

    auto line = [&](float x0, float z0, float x1, float z1,
                    float r, float g, float b) {
        v.insert(v.end(), {x0, 0.f, z0, r, g, b,
                            x1, 0.f, z1, r, g, b});
    };

    // 普通格线（暗灰）
    for (int i = -half; i <= half; ++i) {
        float fi = (float)i;
        float br = 0.27f, bg = 0.27f, bb = 0.27f;
        line(fi*step, -(float)half*step, fi*step,  (float)half*step, br, bg, bb);
        line(-(float)half*step, fi*step, (float)half*step, fi*step,  br, bg, bb);
    }
    // 原点主轴覆盖（更亮）— X红 Z蓝，交点即 (0,0,0)
    line(-(float)half, 0.f, (float)half, 0.f,   0.72f, 0.18f, 0.18f); // X 红
    line(0.f, -(float)half, 0.f, (float)half,   0.18f, 0.42f, 0.72f); // Z 蓝

    m_gridVertCount = (int)v.size() / 6;
    m_gridVAO.create(); m_gridVAO.bind();
    m_gridVBO.create(); m_gridVBO.bind();
    m_gridVBO.allocate(v.data(), (int)(v.size() * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float),
                          (void*)(3*sizeof(float)));
    m_gridVAO.release();
}

// ─── 左下角坐标轴图示（XYZ = RGB）───────────────────────────
void Viewport3D::initAxes()
{
    const float L = 0.85f;
    std::vector<float> v = {
        0,0,0, 0.90f,0.18f,0.18f,  L,0,0, 0.90f,0.18f,0.18f,   // X 红
        0,0,0, 0.22f,0.72f,0.22f,  0,L,0, 0.22f,0.72f,0.22f,   // Y 绿
        0,0,0, 0.22f,0.42f,0.88f,  0,0,L, 0.22f,0.42f,0.88f,   // Z 蓝
    };
    m_axisVAO.create(); m_axisVAO.bind();
    m_axisVBO.create(); m_axisVBO.bind();
    m_axisVBO.allocate(v.data(), (int)(v.size() * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float),
                          (void*)(3*sizeof(float)));
    m_axisVAO.release();
}

// ═══════════════════════════════════════════════════════════════
//  resizeGL
// ═══════════════════════════════════════════════════════════════
void Viewport3D::resizeGL(int w, int h)
{
    m_vpW = qMax(w, 1);
    m_vpH = qMax(h, 1);
    glViewport(0, 0, m_vpW, m_vpH);
    m_overlayBar->setGeometry(0, 0, w, 28);
    m_overlayBar->raise();
}

// ═══════════════════════════════════════════════════════════════
//  paintGL
// ═══════════════════════════════════════════════════════════════
void Viewport3D::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderGrid();
    renderAxes();
    renderObjects();
    if (m_selectedIdx >= 0 && m_gizmoMode != GizmoMode::None)
        renderGizmo();
    m_overlayBar->raise();
}

void Viewport3D::renderGrid()
{
    float asp = (float)m_vpW / m_vpH;
    QMatrix4x4 mvp = m_camera.projMatrix(asp, m_useOrtho) * m_camera.viewMatrix();

    m_flatShader.bind();
    m_flatShader.setUniformValue("uMVP",   mvp);
    m_flatShader.setUniformValue("uAlpha", 1.0f);
    m_gridVAO.bind();
    glLineWidth(1.f);
    glDrawArrays(GL_LINES, 0, m_gridVertCount);
    m_gridVAO.release();
    m_flatShader.release();
}

void Viewport3D::renderAxes()
{
    const int sz = 80, mg = 12;
    glViewport(mg, mg, sz, sz);
    glClear(GL_DEPTH_BUFFER_BIT);

    float yr = qDegreesToRadians(m_camera.yaw);
    float pr = qDegreesToRadians(m_camera.pitch);
    QVector3D eye(cosf(pr)*sinf(yr), sinf(pr), cosf(pr)*cosf(yr));
    QMatrix4x4 rv; rv.lookAt(eye, {0,0,0}, {0,1,0});
    QMatrix4x4 ortho; ortho.ortho(-1.4f, 1.4f, -1.4f, 1.4f, -5.f, 5.f);

    m_flatShader.bind();
    m_flatShader.setUniformValue("uMVP",   ortho * rv);
    m_flatShader.setUniformValue("uAlpha", 1.0f);
    m_axisVAO.bind();
    glLineWidth(2.5f);
    glDrawArrays(GL_LINES, 0, 6);
    m_axisVAO.release();
    m_flatShader.release();

    glViewport(0, 0, m_vpW, m_vpH);
}

void Viewport3D::renderObjects()
{
    if (m_objects.empty()) return;

    float asp = (float)m_vpW / m_vpH;
    QMatrix4x4 proj = m_camera.projMatrix(asp, m_useOrtho);
    QMatrix4x4 view = m_camera.viewMatrix();
    QVector3D  cam  = m_camera.position();
    QVector3D  ldir = QVector3D(0.55f, 1.f, 0.45f).normalized();

    bool wire = (m_renderMode == RenderMode::Wireframe);
    if (wire) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    m_meshShader.bind();
    m_meshShader.setUniformValue("uLightDir",  ldir);
    m_meshShader.setUniformValue("uViewPos",   cam);
    m_meshShader.setUniformValue("uWireframe", wire);

    for (int i = 0; i < (int)m_objects.size(); ++i) {
        auto& obj = *m_objects[i];

        // 隐藏的物体跳过渲染（但仍保留在场景中）
        if (!obj.visible) continue;

        bool sel = (i == m_selectedIdx);

        QMatrix4x4 model;
        model.translate(obj.position);
        model.rotate(obj.rotation.x(), 1, 0, 0);
        model.rotate(obj.rotation.y(), 0, 1, 0);
        model.rotate(obj.rotation.z(), 0, 0, 1);
        model.scale(obj.scale);

        QVector3D col(obj.color.redF(), obj.color.greenF(), obj.color.blueF());

        m_meshShader.setUniformValue("uMVP",       proj * view * model);
        m_meshShader.setUniformValue("uModel",     model);
        m_meshShader.setUniformValue("uNormalMat", model.normalMatrix());
        m_meshShader.setUniformValue("uColor",     col);
        m_meshShader.setUniformValue("uSelected",  sel);

        obj.vao.bind();
        glDrawElements(GL_TRIANGLES, obj.indexCount, GL_UNSIGNED_INT, nullptr);
        obj.vao.release();
    }

    m_meshShader.release();
    if (wire) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

// ═══════════════════════════════════════════════════════════════
//  几何体生成
// ═══════════════════════════════════════════════════════════════
void Viewport3D::buildBox(std::vector<float>& v, std::vector<uint32_t>& idx, QVector3D h)
{
    struct Face { QVector3D n; QVector3D p[4]; };
    float hx=h.x(), hy=h.y(), hz=h.z();
    const Face faces[6]={
        {{ 0, 0, 1}, {{ hx, hy, hz},{-hx, hy, hz},{-hx,-hy, hz},{ hx,-hy, hz}}},
        {{ 0, 0,-1}, {{-hx, hy,-hz},{ hx, hy,-hz},{ hx,-hy,-hz},{-hx,-hy,-hz}}},
        {{-1, 0, 0}, {{-hx, hy, hz},{-hx, hy,-hz},{-hx,-hy,-hz},{-hx,-hy, hz}}},
        {{ 1, 0, 0}, {{ hx, hy,-hz},{ hx, hy, hz},{ hx,-hy, hz},{ hx,-hy,-hz}}},
        {{ 0, 1, 0}, {{ hx, hy,-hz},{-hx, hy,-hz},{-hx, hy, hz},{ hx, hy, hz}}},
        {{ 0,-1, 0}, {{ hx,-hy, hz},{-hx,-hy, hz},{-hx,-hy,-hz},{ hx,-hy,-hz}}},
    };
    for (auto& f : faces) {
        uint32_t b = (uint32_t)(v.size()/6);
        for (auto& p : f.p)
            v.insert(v.end(), {p.x(),p.y(),p.z(), f.n.x(),f.n.y(),f.n.z()});
        idx.insert(idx.end(), {b,b+1,b+2, b,b+2,b+3});
    }
}

void Viewport3D::buildSphere(std::vector<float>& v, std::vector<uint32_t>& idx,
                              float r, int stacks, int slices)
{
    for (int i=0; i<=stacks; ++i) {
        float phi = (float)M_PI * i / stacks;
        for (int j=0; j<=slices; ++j) {
            float theta = 2.f*(float)M_PI * j / slices;
            float x = sinf(phi)*sinf(theta), y=cosf(phi), z=sinf(phi)*cosf(theta);
            v.insert(v.end(), {r*x,r*y,r*z, x,y,z});
        }
    }
    for (int i=0; i<stacks; ++i)
        for (int j=0; j<slices; ++j) {
            uint32_t a=i*(slices+1)+j, b=a+slices+1;
            idx.insert(idx.end(), {a,b,a+1, b,b+1,a+1});
        }
}

void Viewport3D::buildPlane(std::vector<float>& v, std::vector<uint32_t>& idx,
                             float half, int segs)
{
    float step = 2.f*half/segs;
    for (int i=0; i<=segs; ++i)
        for (int j=0; j<=segs; ++j) {
            float x=-half+j*step, z=-half+i*step;
            v.insert(v.end(), {x, 0.f, z, 0.f, 1.f, 0.f});
        }
    for (int i=0; i<segs; ++i)
        for (int j=0; j<segs; ++j) {
            uint32_t a=i*(segs+1)+j;
            idx.insert(idx.end(), {a,a+1,a+segs+2, a,a+segs+2,a+segs+1});
        }
}

void Viewport3D::buildCylinder(std::vector<float>& v, std::vector<uint32_t>& idx,
                                float radius, float height, int slices)
{
    float half = height * 0.5f;
    for (int i = 0; i <= slices; ++i) {
        float a = 2.f*(float)M_PI*i/slices;
        float ca = cosf(a), sa = sinf(a);
        v.insert(v.end(), { radius*ca, -half, radius*sa,  ca, 0.f, sa });
        v.insert(v.end(), { radius*ca,  half, radius*sa,  ca, 0.f, sa });
    }
    for (int i = 0; i < slices; ++i) {
        uint32_t b = i*2;
        idx.insert(idx.end(), { b, b+1, b+3,  b, b+3, b+2 });
    }
    uint32_t topCenter = (uint32_t)(v.size()/6);
    v.insert(v.end(), { 0.f, half, 0.f,  0.f, 1.f, 0.f });
    uint32_t topRing = topCenter + 1;
    for (int i = 0; i <= slices; ++i) {
        float a = 2.f*(float)M_PI*i/slices;
        v.insert(v.end(), { radius*cosf(a), half, radius*sinf(a),  0.f, 1.f, 0.f });
    }
    for (int i = 0; i < slices; ++i)
        idx.insert(idx.end(), { topCenter, topRing+i, topRing+i+1 });

    uint32_t botCenter = (uint32_t)(v.size()/6);
    v.insert(v.end(), { 0.f, -half, 0.f,  0.f, -1.f, 0.f });
    uint32_t botRing = botCenter + 1;
    for (int i = 0; i <= slices; ++i) {
        float a = 2.f*(float)M_PI*i/slices;
        v.insert(v.end(), { radius*cosf(a), -half, radius*sinf(a),  0.f, -1.f, 0.f });
    }
    for (int i = 0; i < slices; ++i)
        idx.insert(idx.end(), { botCenter, botRing+i+1, botRing+i });
}

void Viewport3D::buildCone(std::vector<float>& v, std::vector<uint32_t>& idx,
                            float radius, float height, int slices)
{
    float half = height * 0.5f;
    float sn   = radius / sqrtf(radius*radius + height*height);
    float cn   = height / sqrtf(radius*radius + height*height);
    uint32_t apex = (uint32_t)(v.size()/6);
    for (int i = 0; i <= slices; ++i) {
        float a = 2.f*(float)M_PI*i/slices;
        float ca = cosf(a), sa = sinf(a);
        v.insert(v.end(), { 0.f, half, 0.f,  ca*cn, sn, sa*cn });
        v.insert(v.end(), { radius*ca, -half, radius*sa,  ca*cn, sn, sa*cn });
    }
    for (int i = 0; i < slices; ++i) {
        uint32_t b = apex + i*2;
        idx.insert(idx.end(), { b, b+1, b+3 });
    }
    uint32_t botCenter = (uint32_t)(v.size()/6);
    v.insert(v.end(), { 0.f, -half, 0.f,  0.f, -1.f, 0.f });
    uint32_t botRing = botCenter + 1;
    for (int i = 0; i <= slices; ++i) {
        float a = 2.f*(float)M_PI*i/slices;
        v.insert(v.end(), { radius*cosf(a), -half, radius*sinf(a),  0.f, -1.f, 0.f });
    }
    for (int i = 0; i < slices; ++i)
        idx.insert(idx.end(), { botCenter, botRing+i+1, botRing+i });
}

// ═══════════════════════════════════════════════════════════════
//  GPU 上传
// ═══════════════════════════════════════════════════════════════
void Viewport3D::uploadObject(SceneObject& obj,
                               const std::vector<float>&    verts,
                               const std::vector<uint32_t>& indices)
{
    makeCurrent();
    if (!obj.vao.isCreated()) obj.vao.create();
    obj.vao.bind();

    if (!obj.vbo.isCreated()) obj.vbo.create();
    obj.vbo.bind();
    obj.vbo.allocate(verts.data(), (int)(verts.size()*sizeof(float)));

    if (!obj.ebo.isCreated()) obj.ebo.create();
    obj.ebo.bind();
    obj.ebo.allocate(indices.data(), (int)(indices.size()*sizeof(uint32_t)));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float),
                          (void*)(3*sizeof(float)));
    obj.vao.release();
    obj.indexCount = (int)indices.size();
    doneCurrent();
}

// ─── 统一收尾：上传、入列、发信号 ────────────────────────────
void Viewport3D::finalizeAdd(std::unique_ptr<SceneObject>   obj,
                              const std::vector<float>&      verts,
                              const std::vector<uint32_t>&   indices)
{
    ObjectKind kind = obj->kind;
    QString    name = obj->name;
    // ── 保存 CPU 端缓存，供 SceneRunnerWidget 跨 context 重新上传 ──
    obj->vertices = verts;
    obj->indices  = indices;
    uploadObject(*obj, verts, indices);
    m_objects.push_back(std::move(obj));
    m_statsLabel->setText(QString("%1 object(s)").arg(m_objects.size()));
    emit objectAdded(name, kind);
    update();
}

// ═══════════════════════════════════════════════════════════════
//  公开场景 API
// ═══════════════════════════════════════════════════════════════
void Viewport3D::addBox(const QString& name, QVector3D pos, QVector3D size, QColor color)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name; obj->position = pos; obj->color = color;
    obj->kind = ObjectKind::Mesh;
    std::vector<float> v; std::vector<uint32_t> i;
    buildBox(v, i, size * 0.5f);
    finalizeAdd(std::move(obj), v, i);
}

void Viewport3D::addSphere(const QString& name, QVector3D pos, float radius, QColor color)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name; obj->position = pos; obj->color = color;
    obj->kind = ObjectKind::Mesh;
    std::vector<float> v; std::vector<uint32_t> i;
    buildSphere(v, i, radius);
    finalizeAdd(std::move(obj), v, i);
}

void Viewport3D::addCylinder(const QString& name, QVector3D pos,
                              float radius, float height, QColor color)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name; obj->position = pos; obj->color = color;
    obj->kind = ObjectKind::Mesh;
    std::vector<float> v; std::vector<uint32_t> i;
    buildCylinder(v, i, radius, height);
    finalizeAdd(std::move(obj), v, i);
}

void Viewport3D::addCone(const QString& name, QVector3D pos,
                          float radius, float height, QColor color)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name; obj->position = pos; obj->color = color;
    obj->kind = ObjectKind::Mesh;
    std::vector<float> v; std::vector<uint32_t> i;
    buildCone(v, i, radius, height);
    finalizeAdd(std::move(obj), v, i);
}

void Viewport3D::addPlane(const QString& name, QVector3D pos, float size, QColor color)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name; obj->position = pos; obj->color = color;
    obj->kind = ObjectKind::Mesh;
    std::vector<float> v; std::vector<uint32_t> i;
    buildPlane(v, i, size * 0.5f);
    finalizeAdd(std::move(obj), v, i);
}

void Viewport3D::addLight(const QString& name, QVector3D pos, QColor color)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name; obj->position = pos; obj->color = color;
    obj->kind = ObjectKind::Light;
    std::vector<float> v; std::vector<uint32_t> i;
    buildSphere(v, i, 0.15f);
    finalizeAdd(std::move(obj), v, i);
}

void Viewport3D::clearScene()
{
    makeCurrent();
    for (auto& o : m_objects) { o->vao.destroy(); o->vbo.destroy(); o->ebo.destroy(); }
    m_objects.clear();
    m_selectedIdx = -1;
    m_statsLabel->setText("0 objects");
    doneCurrent();
    emit sceneCleared();
    update();
}

void Viewport3D::setRenderMode(RenderMode mode) { m_renderMode = mode; update(); }

// ═══════════════════════════════════════════════════════════════
//  属性读写接口（Property Editor 双向绑定）
// ═══════════════════════════════════════════════════════════════
bool Viewport3D::getObjectProps(const QString& name, ObjectProps& out) const
{
    for (auto& obj : m_objects) {
        if (obj->name == name) {
            out.position = obj->position;
            out.rotation = obj->rotation;
            out.scale    = obj->scale;
            out.visible  = obj->visible;
            out.layer    = obj->layer;
            return true;
        }
    }
    return false;
}

bool Viewport3D::setObjectProps(const QString& name, const ObjectProps& props)
{
    for (auto& obj : m_objects) {
        if (obj->name == name) {
            obj->position = props.position;
            obj->rotation = props.rotation;
            obj->scale    = props.scale;
            obj->visible  = props.visible;
            obj->layer    = props.layer;
            update();
            emit objectPropsChanged(name);
            return true;
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════
//  鼠标 / 键盘
// ═══════════════════════════════════════════════════════════════
void Viewport3D::mousePressEvent(QMouseEvent* e)
{
    m_lastMouse = e->pos();
    if (e->button() == Qt::MiddleButton) {
        m_rotating = !(e->modifiers() & Qt::ShiftModifier);
        m_panning  =  (e->modifiers() & Qt::ShiftModifier);
        e->accept(); return;
    }
    if (e->button() == Qt::RightButton) {
        showContextMenu(e->pos());
        e->accept(); return;
    }
    if (e->button() == Qt::LeftButton) {
        // 先尝试拾取 Gizmo 轴
        if (m_selectedIdx >= 0 && m_gizmoMode != GizmoMode::None) {
            GizmoAxis ax = pickGizmoAxis(e->pos());
            if (ax != GizmoAxis::None) {
                m_gizmoAxis      = ax;
                m_gizmoDragging  = true;
                m_gizmoDragStart = e->pos();
                auto& obj = *m_objects[m_selectedIdx];
                m_gizmoDragOrigPos = obj.position;
                m_gizmoDragOrigRot = obj.rotation;
                m_gizmoDragOrigSca = obj.scale;
                e->accept(); return;
            }
        }
        // 否则普通对象拾取
        m_gizmoDragging = false;
        m_gizmoAxis     = GizmoAxis::None;
        pickObject(e->pos());
        e->accept();
    }
}

void Viewport3D::mouseMoveEvent(QMouseEvent* e)
{
    const QPoint d = e->pos() - m_lastMouse;

    // Gizmo 拖拽优先
    if (m_gizmoDragging && (e->buttons() & Qt::LeftButton)) {
        applyGizmoDrag(e->pos());
        e->accept();
        return;
    }

    m_lastMouse = e->pos();
    if (m_rotating) {
        m_camera.yaw   -= d.x() * 0.45f;
        m_camera.pitch += d.y() * 0.45f;
        m_camera.pitch  = qBound(-89.f, m_camera.pitch, 89.f);
        update();
    } else if (m_panning) {
        float spd = m_camera.dist * 0.0012f;
        QVector3D fwd  = (m_camera.target - m_camera.position()).normalized();
        QVector3D right = QVector3D::crossProduct(fwd, {0,1,0}).normalized();
        QVector3D up   = QVector3D::crossProduct(right, fwd).normalized();
        m_camera.target -= right * d.x() * spd * 10.f;
        m_camera.target += up    * d.y() * spd * 10.f;
        update();
    } else if (m_gizmoMode != GizmoMode::None && m_selectedIdx >= 0) {
        // 鼠标悬停时高亮轴
        GizmoAxis hover = pickGizmoAxis(e->pos());
        if (hover != m_gizmoAxis) {
            m_gizmoAxis = hover;
            update();
        }
    }
    e->accept();
}

void Viewport3D::mouseReleaseEvent(QMouseEvent* e)
{
    m_rotating      = false;
    m_panning       = false;
    if (m_gizmoDragging) {
        m_gizmoDragging = false;
        // 保留 m_gizmoAxis 用于悬停高亮（移动鼠标会重新检测）
    }
}

void Viewport3D::wheelEvent(QWheelEvent* e)
{
    float delta = e->angleDelta().y() / 120.f;
    m_camera.dist = qBound(0.3f, m_camera.dist - delta * m_camera.dist * 0.13f, 500.f);
    update();
    e->accept();
}

void Viewport3D::keyPressEvent(QKeyEvent* e)
{
    // ── Gizmo 模式切换 (3ds Max 风格) ─────────────────────────
    if (e->key() == Qt::Key_W) {
        m_gizmoMode = GizmoMode::Move;
        m_gizmoAxis = GizmoAxis::None;
        update();
        e->accept(); return;
    }
    if (e->key() == Qt::Key_E) {
        m_gizmoMode = GizmoMode::Rotate;
        m_gizmoAxis = GizmoAxis::None;
        update();
        e->accept(); return;
    }
    if (e->key() == Qt::Key_R) {
        m_gizmoMode = GizmoMode::Scale;
        m_gizmoAxis = GizmoAxis::None;
        update();
        e->accept(); return;
    }
    if (e->key() == Qt::Key_Q) {
        m_gizmoMode = GizmoMode::None;
        m_gizmoAxis = GizmoAxis::None;
        update();
        e->accept(); return;
    }

    if (e->key() == Qt::Key_F && m_selectedIdx >= 0) {
        m_camera.target = m_objects[m_selectedIdx]->position;
        m_camera.dist   = 8.f;
        update();
    }
    if (e->key() == Qt::Key_Delete && m_selectedIdx >= 0) {
        makeCurrent();
        QString removedName = m_objects[m_selectedIdx]->name;
        m_objects[m_selectedIdx]->vao.destroy();
        m_objects[m_selectedIdx]->vbo.destroy();
        m_objects[m_selectedIdx]->ebo.destroy();
        m_objects.erase(m_objects.begin() + m_selectedIdx);
        m_selectedIdx = -1;
        m_gizmoAxis   = GizmoAxis::None;
        m_statsLabel->setText(QString("%1 object(s)").arg(m_objects.size()));
        doneCurrent();
        emit objectRemoved(removedName);
        update();
    }
    QOpenGLWidget::keyPressEvent(e);
}

// ═══════════════════════════════════════════════════════════════
//  射线拾取（球体粗略 AABB）
// ═══════════════════════════════════════════════════════════════
void Viewport3D::pickObject(const QPoint& sp)
{
    float nx =  2.f * sp.x() / m_vpW - 1.f;
    float ny = -2.f * sp.y() / m_vpH + 1.f;
    float asp = (float)m_vpW / m_vpH;

    QMatrix4x4 inv = (m_camera.projMatrix(asp, m_useOrtho) * m_camera.viewMatrix()).inverted();
    QVector4D np = inv * QVector4D(nx, ny, -1, 1);
    QVector4D fp = inv * QVector4D(nx, ny,  1, 1);
    np /= np.w(); fp /= fp.w();

    QVector3D ori = np.toVector3D();
    QVector3D dir = (fp.toVector3D() - ori).normalized();

    int   best  = -1;
    float bestT = 1e9f;
    for (int i = 0; i < (int)m_objects.size(); ++i) {
        if (!m_objects[i]->visible) continue;   // 隐藏物体不可拾取
        QVector3D oc = ori - m_objects[i]->position;
        float b = QVector3D::dotProduct(oc, dir);
        float c = QVector3D::dotProduct(oc, oc) - 4.f;
        float disc = b*b - c;
        if (disc >= 0.f) {
            float t = -b - sqrtf(disc);
            if (t > 0.f && t < bestT) { bestT = t; best = i; }
        }
    }
    if (best != m_selectedIdx) {
        m_selectedIdx = best;
        if (best >= 0) emit objectSelected(m_objects[best]->name);
        update();
    }
}

// ═══════════════════════════════════════════════════════════════
//  右键上下文菜单
// ═══════════════════════════════════════════════════════════════
void Viewport3D::showContextMenu(const QPoint& pos)
{
    QMenu menu(this);
    menu.setStyleSheet(R"(
        QMenu {
            background: #2a2a2e; color: #d0d0d0;
            border: 1px solid #444; font-size: 12px;
        }
        QMenu::item { padding: 5px 20px 5px 12px; }
        QMenu::item:selected { background: #0e639c; }
        QMenu::separator { height: 1px; background: #444; margin: 3px 0; }
        QMenu::item:disabled { color: #555; }
    )");

    QMenu* createMenu = menu.addMenu(tr("创建"));
    createMenu->setStyleSheet(menu.styleSheet());

    QAction* actBox      = createMenu->addAction(tr("Box（立方体）"));
    QAction* actSphere   = createMenu->addAction(tr("Sphere（球体）"));
    QAction* actCylinder = createMenu->addAction(tr("Cylinder（圆柱）"));
    QAction* actCone     = createMenu->addAction(tr("Cone（圆锥）"));
    QAction* actPlane    = createMenu->addAction(tr("Plane（平面）"));

    menu.addSeparator();
    QAction* actFocus  = menu.addAction(tr("聚焦选中  [F]"));
    actFocus->setEnabled(m_selectedIdx >= 0);
    QAction* actDelete = menu.addAction(tr("删除选中  [Del]"));
    actDelete->setEnabled(m_selectedIdx >= 0);
    menu.addSeparator();
    QAction* actClear  = menu.addAction(tr("清空场景"));

    QAction* chosen = menu.exec(mapToGlobal(pos));
    if (!chosen) return;

    static int s_counter = 1;
    if (chosen == actBox) {
        addBox(QString("Box_%1").arg(s_counter++), {0,0,0}, {1,1,1});
    } else if (chosen == actSphere) {
        addSphere(QString("Sphere_%1").arg(s_counter++), {0,0,0}, 0.5f);
    } else if (chosen == actCylinder) {
        addCylinder(QString("Cylinder_%1").arg(s_counter++), {0,0,0});
    } else if (chosen == actCone) {
        addCone(QString("Cone_%1").arg(s_counter++), {0,0,0});
    } else if (chosen == actPlane) {
        addPlane(QString("Plane_%1").arg(s_counter++), {0,0,0}, 4.f);
    } else if (chosen == actFocus && m_selectedIdx >= 0) {
        m_camera.target = m_objects[m_selectedIdx]->position;
        m_camera.dist   = 8.f;
        update();
    } else if (chosen == actDelete && m_selectedIdx >= 0) {
        makeCurrent();
        QString removedName = m_objects[m_selectedIdx]->name;
        m_objects[m_selectedIdx]->vao.destroy();
        m_objects[m_selectedIdx]->vbo.destroy();
        m_objects[m_selectedIdx]->ebo.destroy();
        m_objects.erase(m_objects.begin() + m_selectedIdx);
        m_selectedIdx = -1;
        m_statsLabel->setText(QString("%1 object(s)").arg(m_objects.size()));
        doneCurrent();
        emit objectRemoved(removedName);
        update();
    } else if (chosen == actClear) {
        clearScene();
    }
}

// ═══════════════════════════════════════════════════════════════
//  拖拽放置
// ═══════════════════════════════════════════════════════════════
void Viewport3D::dragEnterEvent(QDragEnterEvent* e)
{
    if (e->mimeData()->hasFormat("application/x-primitive"))
        e->acceptProposedAction();
    else
        e->ignore();
}

void Viewport3D::dropEvent(QDropEvent* e)
{
    if (!e->mimeData()->hasFormat("application/x-primitive")) { e->ignore(); return; }

    QString type = QString::fromUtf8(e->mimeData()->data("application/x-primitive"));

    QPointF sp = e->position();
    float nx =  2.f * (float)sp.x() / m_vpW - 1.f;
    float ny = -2.f * (float)sp.y() / m_vpH + 1.f;
    float asp = (float)m_vpW / m_vpH;

    QMatrix4x4 inv = (m_camera.projMatrix(asp, m_useOrtho) * m_camera.viewMatrix()).inverted();
    QVector4D np4 = inv * QVector4D(nx, ny, -1.f, 1.f);
    QVector4D fp4 = inv * QVector4D(nx, ny,  1.f, 1.f);
    np4 /= np4.w(); fp4 /= fp4.w();

    QVector3D ori = np4.toVector3D();
    QVector3D dir = (fp4.toVector3D() - ori).normalized();

    // 与 Y=0 平面（世界原点所在水平面）求交
    QVector3D worldPos(0, 0, 0);
    if (qAbs(dir.y()) > 1e-4f) {
        float t = -ori.y() / dir.y();
        worldPos = ori + dir * t;
        worldPos.setY(0.f);
    }

    static int s_dropCounter = 1;
    QString uniqueName = type + "_" + QString::number(s_dropCounter++);

    if      (type == "Box")       addBox     (uniqueName, worldPos, {1,1,1});
    else if (type == "Sphere")    addSphere  (uniqueName, worldPos, 0.5f);
    else if (type == "Cylinder")  addCylinder(uniqueName, worldPos);
    else if (type == "Cone")      addCone    (uniqueName, worldPos);
    else if (type == "Plane")     addPlane   (uniqueName, worldPos, 4.f);
    else if (type.contains("Light"))
        addLight(uniqueName, worldPos, QColor(255, 240, 100));

    e->acceptProposedAction();
}

// ═══════════════════════════════════════════════════════════════
//  Gizmo — 3ds Max 风格变换控件
//  Move  (W)  : 三条带箭头的轴线  X=红 Y=绿 Z=蓝
//  Rotate(E)  : 三个经纬旋转圆环  X=红 Y=绿 Z=蓝
//  Scale (R)  : 三条带方块头的轴线 X=红 Y=绿 Z=蓝
// ═══════════════════════════════════════════════════════════════

// ─── 辅助：世界坐标 → 屏幕像素 ─────────────────────────────
QPointF Viewport3D::worldToScreen(const QVector3D& wp) const
{
    float asp = (float)m_vpW / m_vpH;
    QVector4D clip = m_camera.projMatrix(asp, m_useOrtho)
                     * m_camera.viewMatrix()
                     * QVector4D(wp, 1.f);
    if (qAbs(clip.w()) < 1e-6f) return {-9999, -9999};
    float nx = clip.x() / clip.w();
    float ny = clip.y() / clip.w();
    return { (nx + 1.f) * 0.5f * m_vpW,
             (1.f - ny) * 0.5f * m_vpH };
}

// ─── 单根轴箭头（线段 + 锥头 / 方块头）──────────────────────
// 格式：pos(3) + col(3)，适配 flatShader
void Viewport3D::buildArrow(std::vector<float>& v,
                              const QVector3D& dir,
                              const QVector3D& col,
                              float len, bool scaleCube)
{
    QVector3D tip = dir * len;

    // 轴杆线段（两顶点）
    v.insert(v.end(), { 0.f, 0.f, 0.f, col.x(), col.y(), col.z() });
    v.insert(v.end(), { tip.x(), tip.y(), tip.z(), col.x(), col.y(), col.z() });

    if (scaleCube) {
        // Scale 模式：方块头（6条边 = 12顶点线段，形成正方体轮廓）
        float s = len * 0.08f;
        // 先找两个与 dir 垂直的轴
        QVector3D u = (qAbs(dir.x()) < 0.9f)
                      ? QVector3D::crossProduct(dir, {1,0,0}).normalized()
                      : QVector3D::crossProduct(dir, {0,1,0}).normalized();
        QVector3D w2 = QVector3D::crossProduct(dir, u).normalized();

        QVector3D corners[8];
        for (int si = 0; si < 8; ++si) {
            float sx = (si & 1) ? s : -s;
            float sy = (si & 2) ? s : -s;
            float sz = (si & 4) ? s : -s;
            corners[si] = tip + u * sx + w2 * sy + dir * sz;
        }
        // 12条棱
        int edges[12][2] = {
            {0,1},{2,3},{4,5},{6,7},
            {0,2},{1,3},{4,6},{5,7},
            {0,4},{1,5},{2,6},{3,7}
        };
        for (auto& e : edges) {
            for (int k = 0; k < 2; ++k) {
                QVector3D& c = corners[e[k]];
                v.insert(v.end(), { c.x(), c.y(), c.z(), col.x(), col.y(), col.z() });
            }
        }
    } else {
        // Move 模式：三角形锥头（3条边从锥尖到圆底，简化为4条线）
        float coneLen = len * 0.18f;
        float coneR   = len * 0.045f;
        QVector3D base = tip - dir * coneLen;

        QVector3D u = (qAbs(dir.x()) < 0.9f)
                      ? QVector3D::crossProduct(dir, {1,0,0}).normalized()
                      : QVector3D::crossProduct(dir, {0,1,0}).normalized();
        QVector3D w2 = QVector3D::crossProduct(dir, u).normalized();

        for (int k = 0; k < 4; ++k) {
            float a = k * (float)M_PI * 0.5f;
            QVector3D ring = base + (u * cosf(a) + w2 * sinf(a)) * coneR;
            v.insert(v.end(), { tip.x(), tip.y(), tip.z(), col.x(), col.y(), col.z() });
            v.insert(v.end(), { ring.x(), ring.y(), ring.z(), col.x(), col.y(), col.z() });
        }
    }
}

// ─── 旋转圆环（经度线）──────────────────────────────────────
void Viewport3D::buildRing(std::vector<float>& v,
                             const QVector3D& axis,
                             const QVector3D& col,
                             float r, int segs)
{
    // 找两个与 axis 垂直的基向量
    QVector3D u = (qAbs(axis.x()) < 0.9f)
                  ? QVector3D::crossProduct(axis, {1,0,0}).normalized()
                  : QVector3D::crossProduct(axis, {0,1,0}).normalized();
    QVector3D w2 = QVector3D::crossProduct(axis, u).normalized();

    for (int i = 0; i < segs; ++i) {
        float a0 = 2.f * (float)M_PI * i       / segs;
        float a1 = 2.f * (float)M_PI * (i + 1) / segs;
        QVector3D p0 = (u * cosf(a0) + w2 * sinf(a0)) * r;
        QVector3D p1 = (u * cosf(a1) + w2 * sinf(a1)) * r;
        v.insert(v.end(), { p0.x(), p0.y(), p0.z(), col.x(), col.y(), col.z() });
        v.insert(v.end(), { p1.x(), p1.y(), p1.z(), col.x(), col.y(), col.z() });
    }
}

// ─── Gizmo 渲染 ─────────────────────────────────────────────
void Viewport3D::renderGizmo()
{
    if (!m_gizmoGPUReady) return;
    if (m_selectedIdx < 0 || m_selectedIdx >= (int)m_objects.size()) return;

    auto& obj  = *m_objects[m_selectedIdx];
    QVector3D center = obj.position;

    // Gizmo 固定屏幕大小：根据到摄像机距离缩放
    float dist  = (center - m_camera.position()).length();
    float scale = dist * 0.18f;

    // 三轴颜色
    static const QVector3D cX { 0.95f, 0.20f, 0.20f };
    static const QVector3D cY { 0.20f, 0.90f, 0.20f };
    static const QVector3D cZ { 0.25f, 0.50f, 0.95f };

    // 高亮选中轴（变亮/白）
    auto highlight = [&](QVector3D col, GizmoAxis ax) -> QVector3D {
        if (m_gizmoAxis == ax)
            return col * 0.4f + QVector3D(0.9f, 0.9f, 0.9f) * 0.6f;
        return col;
    };

    std::vector<float> verts;

    if (m_gizmoMode == GizmoMode::Rotate) {
        buildRing(verts, {1,0,0}, highlight(cX, GizmoAxis::X), scale);
        buildRing(verts, {0,1,0}, highlight(cY, GizmoAxis::Y), scale);
        buildRing(verts, {0,0,1}, highlight(cZ, GizmoAxis::Z), scale);
    } else {
        bool sc = (m_gizmoMode == GizmoMode::Scale);
        buildArrow(verts, {1,0,0}, highlight(cX, GizmoAxis::X), scale, sc);
        buildArrow(verts, {0,1,0}, highlight(cY, GizmoAxis::Y), scale, sc);
        buildArrow(verts, {0,0,1}, highlight(cZ, GizmoAxis::Z), scale, sc);
    }

    // 上传到 GPU
    m_gizmoVAO.bind();
    m_gizmoVBO.bind();
    m_gizmoVBO.allocate(verts.data(), (int)(verts.size() * sizeof(float)));
    // 重新绑定属性（因为是同一个 VAO，bind 时已记录）
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float),
                          (void*)(3*sizeof(float)));

    float asp = (float)m_vpW / m_vpH;
    QMatrix4x4 model;
    model.translate(center);
    QMatrix4x4 mvp = m_camera.projMatrix(asp, m_useOrtho)
                     * m_camera.viewMatrix()
                     * model;

    glDisable(GL_DEPTH_TEST);
    m_flatShader.bind();
    m_flatShader.setUniformValue("uMVP",   mvp);
    m_flatShader.setUniformValue("uAlpha", 1.0f);
    glLineWidth(2.5f);
    glDrawArrays(GL_LINES, 0, (GLsizei)(verts.size() / 6));
    m_flatShader.release();
    m_gizmoVAO.release();
    glEnable(GL_DEPTH_TEST);
}

// ─── 轴拾取：判断鼠标是否接近某条 Gizmo 轴 ─────────────────
GizmoAxis Viewport3D::pickGizmoAxis(const QPoint& sp)
{
    if (m_selectedIdx < 0) return GizmoAxis::None;
    auto& obj  = *m_objects[m_selectedIdx];
    QVector3D center = obj.position;

    float dist   = (center - m_camera.position()).length();
    float gScale = dist * 0.18f;

    // 轴端点（世界坐标）
    QVector3D tips[3] = {
        center + QVector3D(1,0,0) * gScale,
        center + QVector3D(0,1,0) * gScale,
        center + QVector3D(0,0,1) * gScale,
    };
    QPointF origin = worldToScreen(center);
    float threshold = 14.f;   // 像素距离阈值

    float bestDist = 1e9f;
    GizmoAxis best = GizmoAxis::None;

    if (m_gizmoMode == GizmoMode::Rotate) {
        // 环拾取：检查鼠标到环圆心的屏幕距离是否接近环半径
        QVector3D axes[3] = {{1,0,0},{0,1,0},{0,0,1}};
        GizmoAxis axEnum[3] = { GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z };
        for (int a = 0; a < 3; ++a) {
            // 在环平面上采样24个点，取最近屏幕距离
            QVector3D u = (qAbs(axes[a].x()) < 0.9f)
                          ? QVector3D::crossProduct(axes[a], {1,0,0}).normalized()
                          : QVector3D::crossProduct(axes[a], {0,1,0}).normalized();
            QVector3D w2 = QVector3D::crossProduct(axes[a], u).normalized();
            float minDist = 1e9f;
            for (int k = 0; k < 24; ++k) {
                float ang = 2.f * (float)M_PI * k / 24;
                QVector3D pt = center + (u * cosf(ang) + w2 * sinf(ang)) * gScale;
                QPointF ss = worldToScreen(pt);
                float dx = ss.x() - sp.x(), dy = ss.y() - sp.y();
                float d2 = dx*dx + dy*dy;
                if (d2 < minDist) minDist = d2;
            }
            minDist = sqrtf(minDist);
            if (minDist < threshold && minDist < bestDist) {
                bestDist = minDist;
                best = axEnum[a];
            }
        }
    } else {
        // 箭头/方块：线段最近距离
        GizmoAxis axEnum[3] = { GizmoAxis::X, GizmoAxis::Y, GizmoAxis::Z };
        for (int a = 0; a < 3; ++a) {
            QPointF ts = worldToScreen(tips[a]);
            // 线段 origin→ts，计算点 sp 到线段的屏幕距离
            float dx = ts.x() - origin.x(), dy = ts.y() - origin.y();
            float len2 = dx*dx + dy*dy;
            float t = 0.f;
            if (len2 > 1e-4f) {
                t = ((sp.x() - origin.x()) * dx + (sp.y() - origin.y()) * dy) / len2;
                t = qBound(0.f, t, 1.f);
            }
            float cx = origin.x() + t * dx - sp.x();
            float cy = origin.y() + t * dy - sp.y();
            float d  = sqrtf(cx*cx + cy*cy);
            if (d < threshold && d < bestDist) {
                bestDist = d;
                best = axEnum[a];
            }
        }
    }
    return best;
}

// ─── Gizmo 拖拽：将屏幕 delta 映射到变换增量 ────────────────
void Viewport3D::applyGizmoDrag(const QPoint& cur)
{
    if (m_selectedIdx < 0) return;
    auto& obj = *m_objects[m_selectedIdx];

    QPointF origin = worldToScreen(obj.position);
    float dx = cur.x() - m_gizmoDragStart.x();
    float dy = cur.y() - m_gizmoDragStart.y();

    // 将屏幕 delta 投影到对应世界轴的屏幕方向上
    // 这样沿轴方向拖是"有效分量"，垂直方向无效
    float dist   = (obj.position - m_camera.position()).length();
    float gScale = dist * 0.18f;

    // 每种轴的屏幕方向（归一化）
    auto axisScreenDir = [&](const QVector3D& axis) -> QPointF {
        QPointF a = worldToScreen(obj.position);
        QPointF b = worldToScreen(obj.position + axis * gScale);
        float adx = b.x() - a.x(), ady = b.y() - a.y();
        float len  = sqrtf(adx*adx + ady*ady);
        if (len < 1e-4f) return {1.f, 0.f};
        return { adx / len, ady / len };
    };

    QVector3D axisDir;
    switch (m_gizmoAxis) {
    case GizmoAxis::X: axisDir = {1,0,0}; break;
    case GizmoAxis::Y: axisDir = {0,1,0}; break;
    case GizmoAxis::Z: axisDir = {0,0,1}; break;
    default: return;
    }

    QPointF sd = axisScreenDir(axisDir);
    // 有效分量（屏幕像素投影到轴方向）
    float proj = dx * (float)sd.x() + dy * (float)sd.y();

    // 灵敏度：每像素对应多少世界单位
    float sens = gScale / (float)qMax(m_vpW, m_vpH) * 3.5f;

    if (m_gizmoMode == GizmoMode::Move) {
        QVector3D delta = axisDir * proj * sens;
        obj.position = m_gizmoDragOrigPos + delta;

    } else if (m_gizmoMode == GizmoMode::Rotate) {
        float angle = proj * 1.0f;    // 1px ≈ 1°
        QVector3D rot = m_gizmoDragOrigRot;
        switch (m_gizmoAxis) {
        case GizmoAxis::X: rot.setX(rot.x() + angle); break;
        case GizmoAxis::Y: rot.setY(rot.y() + angle); break;
        case GizmoAxis::Z: rot.setZ(rot.z() + angle); break;
        default: break;
        }
        obj.rotation = rot;

    } else if (m_gizmoMode == GizmoMode::Scale) {
        float factor = 1.f + proj * sens;
        QVector3D sca = m_gizmoDragOrigSca;
        switch (m_gizmoAxis) {
        case GizmoAxis::X: sca.setX(qMax(0.0001f, sca.x() * factor)); break;
        case GizmoAxis::Y: sca.setY(qMax(0.0001f, sca.y() * factor)); break;
        case GizmoAxis::Z: sca.setZ(qMax(0.0001f, sca.z() * factor)); break;
        default: break;
        }
        obj.scale = sca;
    }

    emit objectPropsChanged(obj.name);
    update();
}

// ═══════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════
void Viewport3D::deleteObjectByName(const QString& name)
{
    for (int i = 0; i < (int)m_objects.size(); ++i) {
        if (m_objects[i]->name == name) {
            makeCurrent();
            m_objects[i]->vao.destroy();
            m_objects[i]->vbo.destroy();
            m_objects[i]->ebo.destroy();
            m_objects.erase(m_objects.begin() + i);
            if (m_selectedIdx == i)      m_selectedIdx = -1;
            else if (m_selectedIdx > i)  m_selectedIdx--;
            m_statsLabel->setText(QString("%1 object(s)").arg(m_objects.size()));
            doneCurrent();
            update();
            return;
        }
    }
}
