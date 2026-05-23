#include "stdafx.h"
#include "Viewport3D.h"
#include <QtMath>
#include <cmath>

// ═══════════════════════════════════════════════════════════════
//  GLSL — Mesh Shader（Blinn-Phong + 半球天光 + 选中边缘光）
// ═══════════════════════════════════════════════════════════════
static const char* kMeshVert = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMat;

out vec3 vNormal;
out vec3 vFragPos;

void main()
{
    vFragPos    = vec3(uModel * vec4(aPos, 1.0));
    vNormal     = normalize(uNormalMat * aNormal);
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

static const char* kMeshFrag = R"GLSL(
#version 330 core
in  vec3 vNormal;
in  vec3 vFragPos;
out vec4 fragColor;

uniform vec3 uColor;
uniform vec3 uLightDir;
uniform vec3 uViewPos;
uniform bool uWireframe;
uniform bool uSelected;

void main()
{
    if (uWireframe) {
        vec3 c = uSelected ? vec3(1.0,0.65,0.0) : vec3(0.55,0.75,0.95);
        fragColor = vec4(c, 1.0);
        return;
    }

    vec3 ambient  = 0.20 * uColor;
    float diff    = max(dot(vNormal, uLightDir), 0.0);
    vec3 diffuse  = diff * uColor;

    vec3 viewDir  = normalize(uViewPos - vFragPos);
    vec3 halfDir  = normalize(uLightDir + viewDir);
    float spec    = pow(max(dot(vNormal, halfDir), 0.0), 48.0);
    vec3 specular = spec * vec3(0.30);

    // 半球天光（模拟环境光）
    float hemi = 0.5 + 0.5 * vNormal.y;
    vec3  sky  = mix(vec3(0.07,0.05,0.04), vec3(0.16,0.20,0.28), hemi) * 0.35;

    vec3 result = ambient + diffuse + specular + sky;

    // 选中对象：橙色 rim light
    if (uSelected) {
        float rim  = 1.0 - max(dot(viewDir, vNormal), 0.0);
        result    += pow(rim, 2.8) * vec3(1.0, 0.55, 0.05) * 0.9;
    }

    fragColor = vec4(result, 1.0);
}
)GLSL";

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

QMatrix4x4 OrbitCamera::projMatrix(float aspect) const
{
    QMatrix4x4 m;
    m.perspective(45.f, aspect, 0.05f, 1000.f);
    return m;
}

// ═══════════════════════════════════════════════════════════════
//  构造 / 析构
// ═══════════════════════════════════════════════════════════════
Viewport3D::Viewport3D(QWidget* parent)
    : QOpenGLWidget(parent)
{
    // ── OpenGL 3.3 Core + MSAA x4 ────────────────────────────
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

    buildOverlayToolBar();
}

Viewport3D::~Viewport3D()
{
    makeCurrent();
    m_gridVAO.destroy(); m_gridVBO.destroy();
    m_axisVAO.destroy(); m_axisVBO.destroy();
    for (auto& o : m_objects) {
        o->vao.destroy();
        o->vbo.destroy();
        o->ebo.destroy();
    }
    doneCurrent();
}

// ═══════════════════════════════════════════════════════════════
//  Overlay 工具栏（去掉 Play / 全屏，保留视角 + 渲染模式）
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
    m_viewCombo->addItems({ tr("Persp"), tr("Top"), tr("Front"), tr("Right"), tr("Left"), tr("Bottom") });
    m_viewCombo->setFixedWidth(72);
    lay->addWidget(m_viewCombo);

    // 分隔
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

    // 物体数量统计（右侧）
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

    // 深蓝灰背景（3ds Max 风格）
    glClearColor(0.172f, 0.172f, 0.180f, 1.f);

    initShaders();
    initGrid();
    initAxes();
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

// ─── 地面网格（XZ 平面，20×20 格，X轴红 Z轴蓝）──────────────
void Viewport3D::initGrid()
{
    const int   half = 10;
    const float step = 1.f;
    std::vector<float> v;   // x,y,z,r,g,b

    auto line = [&](float x0,float z0,float x1,float z1,float r,float g,float b){
        v.insert(v.end(), {x0,0.f,z0, r,g,b,  x1,0.f,z1, r,g,b});
    };

    for (int i = -half; i <= half; ++i) {
        float fi = (float)i;
        float br = 0.27f, bg = 0.27f, bb = 0.27f;   // 普通格线
        line(fi*step, -(float)half*step, fi*step, (float)half*step, br,bg,bb);
        line(-(float)half*step, fi*step, (float)half*step, fi*step, br,bg,bb);
    }
    // 主轴覆盖（更亮）
    line(-(float)half,0, (float)half,0,  0.72f,0.18f,0.18f);  // X 红
    line(0,-(float)half, 0,(float)half,  0.18f,0.42f,0.72f);  // Z 蓝

    m_gridVertCount = (int)v.size() / 6;
    m_gridVAO.create(); m_gridVAO.bind();
    m_gridVBO.create(); m_gridVBO.bind();
    m_gridVBO.allocate(v.data(), (int)(v.size()*sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    m_gridVAO.release();
}

// ─── 右下角坐标轴图示（XYZ = RGB）───────────────────────────
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
    m_axisVBO.allocate(v.data(), (int)(v.size()*sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
    m_axisVAO.release();
}

// ═══════════════════════════════════════════════════════════════
//  resizeGL
// ═══════════════════════════════════════════════════════════════
void Viewport3D::resizeGL(int w, int h)
{
    m_vpW = qMax(w,1);
    m_vpH = qMax(h,1);
    glViewport(0,0,m_vpW,m_vpH);
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
    m_overlayBar->raise();
}

// ─── 地面网格渲染 ─────────────────────────────────────────────
void Viewport3D::renderGrid()
{
    float asp = (float)m_vpW / m_vpH;
    QMatrix4x4 mvp = m_camera.projMatrix(asp) * m_camera.viewMatrix();

    m_flatShader.bind();
    m_flatShader.setUniformValue("uMVP",   mvp);
    m_flatShader.setUniformValue("uAlpha", 1.0f);
    m_gridVAO.bind();
    glLineWidth(1.f);
    glDrawArrays(GL_LINES, 0, m_gridVertCount);
    m_gridVAO.release();
    m_flatShader.release();
}

// ─── 右下角坐标轴图示（独立小视口）──────────────────────────
void Viewport3D::renderAxes()
{
    const int sz = 80, mg = 12;
    glViewport(m_vpW - sz - mg, mg, sz, sz);
    glClear(GL_DEPTH_BUFFER_BIT);

    // 只保留旋转部分的 view 矩阵
    float yr = qDegreesToRadians(m_camera.yaw);
    float pr = qDegreesToRadians(m_camera.pitch);
    QVector3D eye(cosf(pr)*sinf(yr), sinf(pr), cosf(pr)*cosf(yr));
    QMatrix4x4 rv; rv.lookAt(eye, {0,0,0}, {0,1,0});
    QMatrix4x4 ortho; ortho.ortho(-1.4f,1.4f,-1.4f,1.4f,-5.f,5.f);

    m_flatShader.bind();
    m_flatShader.setUniformValue("uMVP",   ortho * rv);
    m_flatShader.setUniformValue("uAlpha", 1.0f);
    m_axisVAO.bind();
    glLineWidth(2.5f);
    glDrawArrays(GL_LINES, 0, 6);
    m_axisVAO.release();
    m_flatShader.release();

    glViewport(0, 0, m_vpW, m_vpH);   // 恢复主视口
}

// ─── 场景物体渲染 ─────────────────────────────────────────────
void Viewport3D::renderObjects()
{
    if (m_objects.empty()) return;

    float asp = (float)m_vpW / m_vpH;
    QMatrix4x4 proj = m_camera.projMatrix(asp);
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
        bool sel  = (i == m_selectedIdx);

        QMatrix4x4 model;
        model.translate(obj.position);
        model.rotate(obj.rotation.x(), 1,0,0);
        model.rotate(obj.rotation.y(), 0,1,0);
        model.rotate(obj.rotation.z(), 0,0,1);
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
            v.insert(v.end(), {x,0.f,z, 0.f,1.f,0.f});
        }
    for (int i=0; i<segs; ++i)
        for (int j=0; j<segs; ++j) {
            uint32_t a=i*(segs+1)+j;
            idx.insert(idx.end(), {a,a+1,a+segs+2, a,a+segs+2,a+segs+1});
        }
}

// ─── GPU 上传（顶点布局：pos3 + normal3，stride=6）───────────
void Viewport3D::uploadObject(SceneObject& obj,
                               const std::vector<float>& verts,
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
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));

    obj.vao.release();
    obj.indexCount = (int)indices.size();
    doneCurrent();
}

// ═══════════════════════════════════════════════════════════════
//  公开场景 API
// ═══════════════════════════════════════════════════════════════
void Viewport3D::addBox(const QString& name, QVector3D pos, QVector3D size, QColor color)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name; obj->position = pos; obj->color = color;
    std::vector<float> v; std::vector<uint32_t> i;
    buildBox(v, i, size * 0.5f);
    uploadObject(*obj, v, i);
    m_objects.push_back(std::move(obj));
    m_statsLabel->setText(QString("%1 object(s)").arg(m_objects.size()));
    update();
}

void Viewport3D::addSphere(const QString& name, QVector3D pos, float radius, QColor color)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name; obj->position = pos; obj->color = color;
    std::vector<float> v; std::vector<uint32_t> i;
    buildSphere(v, i, radius);
    uploadObject(*obj, v, i);
    m_objects.push_back(std::move(obj));
    m_statsLabel->setText(QString("%1 object(s)").arg(m_objects.size()));
    update();
}

void Viewport3D::addPlane(const QString& name, QVector3D pos, float size, QColor color)
{
    auto obj = std::make_unique<SceneObject>();
    obj->name = name; obj->position = pos; obj->color = color;
    std::vector<float> v; std::vector<uint32_t> i;
    buildPlane(v, i, size * 0.5f);
    uploadObject(*obj, v, i);
    m_objects.push_back(std::move(obj));
    m_statsLabel->setText(QString("%1 object(s)").arg(m_objects.size()));
    update();
}

void Viewport3D::clearScene()
{
    makeCurrent();
    for (auto& o : m_objects) { o->vao.destroy(); o->vbo.destroy(); o->ebo.destroy(); }
    m_objects.clear();
    m_selectedIdx = -1;
    m_statsLabel->setText("0 objects");
    doneCurrent();
    update();
}

void Viewport3D::setRenderMode(RenderMode mode) { m_renderMode = mode; update(); }

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
        m_rotating = !(e->modifiers() & Qt::AltModifier);
        m_panning  =  (e->modifiers() & Qt::AltModifier);
        e->accept(); return;
    }
    if (e->button() == Qt::LeftButton) {
        pickObject(e->pos());
        e->accept();
    }
}

void Viewport3D::mouseMoveEvent(QMouseEvent* e)
{
    const QPoint d = e->pos() - m_lastMouse;
    m_lastMouse = e->pos();

    if (m_rotating) {
        m_camera.yaw   += d.x() * 0.45f;
        m_camera.pitch -= d.y() * 0.45f;
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
    }
    e->accept();
}

void Viewport3D::mouseReleaseEvent(QMouseEvent*)
{
    m_rotating = false;
    m_panning  = false;
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
    // F 键：聚焦选中物体
    if (e->key() == Qt::Key_F && m_selectedIdx >= 0) {
        m_camera.target = m_objects[m_selectedIdx]->position;
        m_camera.dist   = 8.f;
        update();
    }
    // Delete 键：删除选中物体
    if (e->key() == Qt::Key_Delete && m_selectedIdx >= 0) {
        makeCurrent();
        m_objects[m_selectedIdx]->vao.destroy();
        m_objects[m_selectedIdx]->vbo.destroy();
        m_objects[m_selectedIdx]->ebo.destroy();
        m_objects.erase(m_objects.begin() + m_selectedIdx);
        m_selectedIdx = -1;
        m_statsLabel->setText(QString("%1 object(s)").arg(m_objects.size()));
        doneCurrent();
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

    QMatrix4x4 inv = (m_camera.projMatrix(asp) * m_camera.viewMatrix()).inverted();
    QVector4D np = inv * QVector4D(nx, ny, -1, 1);
    QVector4D fp = inv * QVector4D(nx, ny,  1, 1);
    np /= np.w(); fp /= fp.w();

    QVector3D ori = np.toVector3D();
    QVector3D dir = (fp.toVector3D() - ori).normalized();

    int   best = -1;
    float bestT = 1e9f;
    for (int i=0; i<(int)m_objects.size(); ++i) {
        QVector3D oc = ori - m_objects[i]->position;
        float b = QVector3D::dotProduct(oc, dir);
        float c = QVector3D::dotProduct(oc, oc) - 4.f;
        float disc = b*b - c;
        if (disc >= 0.f) {
            float t = -b - sqrtf(disc);
            if (t > 0.f && t < bestT) { bestT=t; best=i; }
        }
    }
    if (best != m_selectedIdx) {
        m_selectedIdx = best;
        if (best >= 0) emit objectSelected(m_objects[best]->name);
        update();
    }
}
