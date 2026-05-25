#include "stdafx.h"
#include "SceneRunnerWidget.h"
#include "SceneShaders.h"
#include <QTimer>
#include <QKeyEvent>

SceneRunnerWidget::SceneRunnerWidget(
    const std::vector<std::unique_ptr<SceneObject>>& objects,
    QWidget* parent)
    : QOpenGLWidget(parent)
{
    // 深拷贝场景对象（只复制数据字段，VAO/VBO 在 initializeGL 里重新上传）
    for (auto& src : objects) {
        auto copy = std::make_shared<SceneObject>();
        // 只复制纯数据，不触碰 GL 对象（它们禁止拷贝）
        copy->name       = src->name;
        copy->kind       = src->kind;
        copy->position   = src->position;
        copy->rotation   = src->rotation;
        copy->scale      = src->scale;
        copy->color      = src->color;
        copy->visible    = src->visible;
        copy->layer      = src->layer;
        copy->vertices   = src->vertices;
        copy->indices    = src->indices;
        copy->indexCount = src->indexCount;
        // vao / vbo / ebo 保持默认构造，initializeGL 里 uploadObject 会创建
        m_objects.push_back(copy);
    }

    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSamples(4);
    fmt.setDepthBufferSize(24);
    setFormat(fmt);

    setFocusPolicy(Qt::StrongFocus);

    // 60fps 定时器驱动动画
    m_timer = new QTimer(this);
    m_timer->setInterval(16);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_time += 0.016f;
        update();
    });
    m_timer->start();
}

SceneRunnerWidget::~SceneRunnerWidget()
{
    stop();
    makeCurrent();
    for (auto& o : m_objects) {
        o->vao.destroy();
        o->vbo.destroy();
        o->ebo.destroy();
    }
    doneCurrent();
}

void SceneRunnerWidget::stop()
{
    if (m_timer) { m_timer->stop(); }
}

void SceneRunnerWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.11f, 0.11f, 0.13f, 1.f);

    m_meshProg.addShaderFromSourceCode(QOpenGLShader::Vertex,   kMeshVert);
    m_meshProg.addShaderFromSourceCode(QOpenGLShader::Fragment, kMeshFrag);
    m_meshProg.link();

    // 把拷贝的 mesh 数据上传到 GPU
    for (auto& o : m_objects)
        uploadObject(*o);
}

void SceneRunnerWidget::uploadObject(SceneObject& obj)
{
    obj.vao.create();
    obj.vao.bind();

    obj.vbo.create();
    obj.vbo.bind();
    obj.vbo.allocate(obj.vertices.data(),
        (int)(obj.vertices.size() * sizeof(float)));

    obj.ebo.create();
    obj.ebo.bind();
    obj.ebo.allocate(obj.indices.data(),
        (int)(obj.indices.size() * sizeof(unsigned int)));
    obj.indexCount = (int)obj.indices.size();

    int loc = m_meshProg.attributeLocation("aPos");
    m_meshProg.enableAttributeArray(loc);
    m_meshProg.setAttributeBuffer(loc, GL_FLOAT, 0, 3, 6 * sizeof(float));

    int nLoc = m_meshProg.attributeLocation("aNormal");
    m_meshProg.enableAttributeArray(nLoc);
    m_meshProg.setAttributeBuffer(nLoc, GL_FLOAT,
        3 * sizeof(float), 3, 6 * sizeof(float));
    obj.vao.release();
}

void SceneRunnerWidget::resizeGL(int w, int h)
{
    m_vpW = w; m_vpH = h;
    glViewport(0, 0, w, h);
}

void SceneRunnerWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (m_objects.empty()) return;

    float asp = (float)m_vpW / (float)m_vpH;
    QMatrix4x4 view = m_camera.viewMatrix();
    QMatrix4x4 proj = m_camera.projMatrix(asp, false);

    m_meshProg.bind();
    m_meshProg.setUniformValue("uLightDir",
        QVector3D(0.6f, 1.f, 0.8f).normalized());
    m_meshProg.setUniformValue("uViewPos", m_camera.position());
    m_meshProg.setUniformValue("uWireframe", false);

    for (auto& obj : m_objects) {
        if (!obj->visible) continue;

        QMatrix4x4 model;
        model.translate(obj->position);
        model.rotate(obj->rotation.x(), 1, 0, 0);
        model.rotate(obj->rotation.y(), 0, 1, 0);
        model.rotate(obj->rotation.z(), 0, 0, 1);
        model.scale(obj->scale);

        QMatrix4x4 mvp       = proj * view * model;
        QMatrix3x3 normalMat = model.normalMatrix();
        QVector3D  col(obj->color.redF(), obj->color.greenF(), obj->color.blueF());

        m_meshProg.setUniformValue("uMVP",       mvp);
        m_meshProg.setUniformValue("uModel",     model);
        m_meshProg.setUniformValue("uNormalMat", normalMat);
        m_meshProg.setUniformValue("uColor",     col);
        m_meshProg.setUniformValue("uSelected",  false);

        obj->vao.bind();
        glDrawElements(GL_TRIANGLES,
            (GLsizei)obj->indexCount,
            GL_UNSIGNED_INT,
            (const void*)nullptr);
        obj->vao.release();
    }
    m_meshProg.release();
}

void SceneRunnerWidget::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        if (window() != parentWidget())
            window()->close();
    }
    QOpenGLWidget::keyPressEvent(e);
}
