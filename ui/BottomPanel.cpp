#include "stdafx.h"
#include "BottomPanel.h"
#include <QHeaderView>
#include <QScrollBar>
#include <QTimer>
#include <QMouseEvent>
#include <QDateTime>
#include <QTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QCoreApplication>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QFileIconProvider>
#include <QStyle>
#include <QFileDialog>
#include <QDrag>
#include <QMimeData>
#include <QSizePolicy>
#include <QStackedWidget>
#include <functional>
#include <cmath>

// ════════════════════════════════════════════════════════════════
//  ObjMesh  —  微型 OBJ 解析器（支持 v / vn / f，多边形三角化）
// ════════════════════════════════════════════════════════════════
ObjMesh ObjMesh::load(const QString& path)
{
    ObjMesh out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;

    // 原始数据
    std::vector<QVector3D> pos;
    std::vector<QVector3D> nrm;

    // 最终交错顶点 & 索引（简化：每个面角独立顶点，不共享）
    std::vector<float>&    verts = out.vertices;
    std::vector<uint32_t>& idx   = out.indices;

    QVector3D bmin( 1e9f, 1e9f, 1e9f);
    QVector3D bmax(-1e9f,-1e9f,-1e9f);

    auto updateBB = [&](const QVector3D& p) {
        bmin.setX(std::min(bmin.x(), p.x()));
        bmin.setY(std::min(bmin.y(), p.y()));
        bmin.setZ(std::min(bmin.z(), p.z()));
        bmax.setX(std::max(bmax.x(), p.x()));
        bmax.setY(std::max(bmax.y(), p.y()));
        bmax.setZ(std::max(bmax.z(), p.z()));
    };

    while (!f.atEnd()) {
        QString line = f.readLine().trimmed();
        if (line.startsWith('#') || line.isEmpty()) continue;

        QStringList tk = line.split(' ', Qt::SkipEmptyParts);
        if (tk.isEmpty()) continue;

        if (tk[0] == "v" && tk.size() >= 4) {
            pos.push_back({ tk[1].toFloat(), tk[2].toFloat(), tk[3].toFloat() });
        } else if (tk[0] == "vn" && tk.size() >= 4) {
            nrm.push_back({ tk[1].toFloat(), tk[2].toFloat(), tk[3].toFloat() });
        } else if (tk[0] == "f" && tk.size() >= 4) {
            // 解析每个面角 "v", "v/t", "v/t/n", "v//n"
            struct FVert { int vi = -1, ni = -1; };
            auto parseVert = [](const QString& s) -> FVert {
                FVert fv;
                QStringList p = s.split('/');
                if (!p.isEmpty()) fv.vi = p[0].toInt() - 1;
                if (p.size() >= 3 && !p[2].isEmpty()) fv.ni = p[2].toInt() - 1;
                return fv;
            };

            // 扇形三角化  (0,1,2), (0,2,3), …
            QVector<FVert> face;
            for (int i = 1; i < tk.size(); ++i)
                face.push_back(parseVert(tk[i]));

            for (int i = 1; i + 1 < face.size(); ++i) {
                FVert corners[3] = { face[0], face[i], face[i+1] };
                for (auto& fv : corners) {
                    if (fv.vi < 0 || fv.vi >= (int)pos.size()) continue;
                    QVector3D p = pos[fv.vi];
                    updateBB(p);

                    QVector3D n(0, 1, 0);
                    if (fv.ni >= 0 && fv.ni < (int)nrm.size())
                        n = nrm[fv.ni];

                    idx.push_back((uint32_t)verts.size() / 6);
                    verts.insert(verts.end(), { p.x(), p.y(), p.z(),
                                                n.x(), n.y(), n.z() });
                }
            }
        }
    }

    if (verts.empty()) return out;   // 无有效几何

    // 若文件没有法线，按三角形面法线补算
    if (nrm.empty()) {
        for (size_t i = 0; i + 2 < idx.size(); i += 3) {
            uint32_t a = idx[i]*6, b = idx[i+1]*6, c = idx[i+2]*6;
            QVector3D pa(verts[a],   verts[a+1], verts[a+2]);
            QVector3D pb(verts[b],   verts[b+1], verts[b+2]);
            QVector3D pc(verts[c],   verts[c+1], verts[c+2]);
            QVector3D fn = QVector3D::crossProduct(pb - pa, pc - pa).normalized();
            for (int k = 0; k < 3; ++k) {
                uint32_t base = idx[i+k]*6;
                verts[base+3] = fn.x();
                verts[base+4] = fn.y();
                verts[base+5] = fn.z();
            }
        }
    }

    out.bboxMin  = bmin;
    out.bboxMax  = bmax;
    out.filePath = path;
    out.valid    = true;
    return out;
}

// ════════════════════════════════════════════════════════════════
//  ObjPreviewWidget  —  迷你轨道相机 OpenGL 预览
// ════════════════════════════════════════════════════════════════

// ── GLSL ────────────────────────────────────────────────────────
const char* ObjPreviewWidget::kVert = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNorm;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vNorm;
out vec3 vPos;
void main(){
    vNorm = mat3(transpose(inverse(uModel))) * aNorm;
    vPos  = vec3(uModel * vec4(aPos, 1.0));
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)GLSL";

const char* ObjPreviewWidget::kFrag = R"GLSL(
#version 330 core
in  vec3 vNorm;
in  vec3 vPos;
out vec4 fragColor;
uniform vec3 uCamPos;
void main(){
    vec3 N = normalize(vNorm);
    vec3 L = normalize(vec3(1.2, 2.0, 1.5));
    vec3 V = normalize(uCamPos - vPos);
    vec3 H = normalize(L + V);
    vec3 baseCol = vec3(0.55, 0.72, 0.90);
    float diff  = max(dot(N, L), 0.0) * 0.8 + 0.18;
    float spec  = pow(max(dot(N, H), 0.0), 32.0) * 0.4;
    float rim   = pow(1.0 - max(dot(N, V), 0.0), 2.5) * 0.18;
    fragColor = vec4(baseCol * diff + vec3(spec) + vec3(rim), 1.0);
}
)GLSL";

ObjPreviewWidget::ObjPreviewWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSamples(4);
    fmt.setDepthBufferSize(24);
    setFormat(fmt);

    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);

    // 自动旋转定时器（每帧 ~60fps，慢速自转）
    m_autoRotTimer = new QTimer(this);
    m_autoRotTimer->setInterval(16);
    connect(m_autoRotTimer, &QTimer::timeout, this, [this]() {
        m_yaw += 0.4f;
        update();
    });
    m_autoRotTimer->start();
}

ObjPreviewWidget::~ObjPreviewWidget()
{
    makeCurrent();
    m_vao.destroy();
    m_vbo.destroy();
    m_ebo.destroy();
    doneCurrent();
}

void ObjPreviewWidget::loadMesh(const ObjMesh& mesh)
{
    // ── 计算包围盒中心与最大半径 ──────────────────────────────
    m_center = (mesh.bboxMin + mesh.bboxMax) * 0.5f;
    float dx = mesh.bboxMax.x() - mesh.bboxMin.x();
    float dy = mesh.bboxMax.y() - mesh.bboxMin.y();
    float dz = mesh.bboxMax.z() - mesh.bboxMin.z();
    m_radius = std::max({ dx, dy, dz }) * 0.5f;
    if (m_radius < 1e-4f) m_radius = 1.f;

    // ── 在 CPU 端做归一化：平移到中心，缩放到半径 = 1 ────────
    // 这样 GPU 端不需要 model.scale，避免 model/camera 单位不一致
    m_verts.resize(mesh.vertices.size());
    float invR = 1.f / m_radius;
    for (size_t i = 0; i < mesh.vertices.size(); i += 6) {
        m_verts[i]   = (mesh.vertices[i]   - m_center.x()) * invR;
        m_verts[i+1] = (mesh.vertices[i+1] - m_center.y()) * invR;
        m_verts[i+2] = (mesh.vertices[i+2] - m_center.z()) * invR;
        // 法线不受平移影响，缩放均匀所以方向不变，直接复制
        m_verts[i+3] = mesh.vertices[i+3];
        m_verts[i+4] = mesh.vertices[i+4];
        m_verts[i+5] = mesh.vertices[i+5];
    }
    m_idx = mesh.indices;

    // ── 归一化后模型半径固定为 1.0，相机距离统一按此设定 ─────
    m_dist      = 2.2f;   // 距离 2.2 个单位，FOV=45°，模型正好充满约 80% 视口
    m_panOffset = QVector3D(0, 0, 0);

    m_meshReady = true;
    if (isValid()) {
        makeCurrent();
        uploadMesh();
        doneCurrent();
    }
    update();
    m_autoRotTimer->start();
}

void ObjPreviewWidget::clearMesh()
{
    m_meshReady  = false;
    m_gpuReady   = false;
    m_indexCount = 0;
    update();
    m_autoRotTimer->stop();
}

void ObjPreviewWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.13f, 0.13f, 0.14f, 1.f);

    buildShader();

    m_vao.create();
    m_vbo.create();
    m_ebo.create();

    if (m_meshReady)
        uploadMesh();
}

void ObjPreviewWidget::buildShader()
{
    m_shader.addShaderFromSourceCode(QOpenGLShader::Vertex,   kVert);
    m_shader.addShaderFromSourceCode(QOpenGLShader::Fragment, kFrag);
    m_shader.link();
}

void ObjPreviewWidget::uploadMesh()
{
    m_vao.bind();
    m_vbo.bind();
    m_vbo.allocate(m_verts.data(), (int)(m_verts.size() * sizeof(float)));
    m_ebo.bind();
    m_ebo.allocate(m_idx.data(),   (int)(m_idx.size()   * sizeof(uint32_t)));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float),
                          (void*)(3*sizeof(float)));
    m_vao.release();

    m_indexCount = (int)m_idx.size();
    m_gpuReady   = true;
}

void ObjPreviewWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, qMax(h, 1));
}

void ObjPreviewWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_gpuReady || m_indexCount == 0) return;

    float asp = (float)width() / qMax(height(), 1);
    float yr  = qDegreesToRadians(m_yaw);
    float pr  = qDegreesToRadians(m_pitch);

    // 归一化后模型在原点，半径=1；相机绕原点轨道，+ panOffset 平移注视点
    QVector3D lookAt = m_panOffset;   // 模型已归一化到原点，无需 m_center 偏移

    QVector3D eye(
        m_dist * cosf(pr) * sinf(yr),
        m_dist * sinf(pr),
        m_dist * cosf(pr) * cosf(yr)
    );
    eye += lookAt;

    QMatrix4x4 view, proj;
    view.lookAt(eye, lookAt, {0, 1, 0});
    // near/far 基于归一化后的单位（半径=1）
    proj.perspective(45.f, asp, 0.01f, 100.f);

    // model 矩阵为单位矩阵（CPU 端已做归一化，GPU 不需要再缩放）
    QMatrix4x4 model;   // 默认 identity

    m_shader.bind();
    m_shader.setUniformValue("uMVP",    proj * view * model);
    m_shader.setUniformValue("uModel",  model);
    m_shader.setUniformValue("uCamPos", eye);

    m_vao.bind();
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    m_vao.release();
    m_shader.release();
}

void ObjPreviewWidget::mousePressEvent(QMouseEvent* e)
{
    m_lastMouse = e->pos();

    if (e->button() == Qt::LeftButton) {
        m_rotating = true;
        m_panning  = false;
        m_autoRotTimer->stop();
        e->accept();
    } else if (e->button() == Qt::MiddleButton) {
        m_panning  = true;
        m_rotating = false;
        m_autoRotTimer->stop();
        e->accept();
    } else if (e->button() == Qt::RightButton) {
        // 右键：重置相机（平移归零、恢复默认距离与角度）
        m_panOffset = QVector3D(0, 0, 0);
        m_dist  = 2.2f;
        m_yaw   = 30.f;
        m_pitch = 20.f;
        m_autoRotTimer->start();
        update();
        e->accept();
    }
}

void ObjPreviewWidget::mouseMoveEvent(QMouseEvent* e)
{
    QPoint delta = e->pos() - m_lastMouse;
    m_lastMouse  = e->pos();

    if (m_rotating && (e->buttons() & Qt::LeftButton)) {
        m_yaw   -= delta.x() * 0.6f;
        m_pitch += delta.y() * 0.6f;
        m_pitch  = qBound(-85.f, m_pitch, 85.f);
        update();
        e->accept();
    } else if (m_panning && (e->buttons() & Qt::MiddleButton)) {
        // 在相机的右/上方向平移注视点
        float yr = qDegreesToRadians(m_yaw);
        float pr = qDegreesToRadians(m_pitch);
        QVector3D forward(cosf(pr) * sinf(yr), sinf(pr), cosf(pr) * cosf(yr));
        QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 1, 0)).normalized();
        QVector3D up    = QVector3D::crossProduct(right, forward).normalized();

        // 平移灵敏度：与距离和模型半径成比例
        float sens = m_dist * 0.0015f;
        m_panOffset -= right * (float)delta.x() * sens;
        m_panOffset += up    * (float)delta.y() * sens;
        update();
        e->accept();
    }
}

void ObjPreviewWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && m_rotating) {
        m_rotating = false;
        m_autoRotTimer->start();
        e->accept();
    } else if (e->button() == Qt::MiddleButton && m_panning) {
        m_panning = false;
        m_autoRotTimer->start();
        e->accept();
    }
}

void ObjPreviewWidget::wheelEvent(QWheelEvent* e)
{
    float delta = e->angleDelta().y() / 120.f;
    // 归一化后半径=1，限制在 [0.5, 8.0] 单位内缩放
    m_dist = qBound(0.5f, m_dist - delta * m_dist * 0.15f, 8.f);
    update();
    e->accept();
}

// ════════════════════════════════════════════════════════════════
//  AssetBrowserPanel  —  真实文件系统版
// ════════════════════════════════════════════════════════════════

// ── 示例文件列表（每个子目录写入一批空文件作为占位资产）──────
static const QMap<QString, QStringList> kSampleFiles = {
    { "audio",     { "ambient_wind.wav", "footstep_grass.wav",
                     "explosion_01.wav", "music_main_theme.ogg",
                     "ui_click.wav"    } },
    { "levels",    { "level_01.lvl",    "level_02.lvl",
                     "tutorial.lvl",    "sandbox.lvl"          } },
    { "models",    { "player_hero.fbx", "enemy_goblin.fbx",
                     "tree_pine.fbx",   "rock_large.fbx",
                     "chest.fbx",       "barrel.fbx"           } },
    { "materials", { "stone_wall.mat",  "wood_floor.mat",
                     "metal_rust.mat",  "grass_ground.mat",
                     "water_surface.mat"                        } },
};

// ── 根目录：exe 同级的 Media 文件夹 ─────────────────────────
QString AssetBrowserPanel::mediaRoot()
{
    return QCoreApplication::applicationDirPath() + "/Media";
}

void AssetBrowserPanel::ensureMediaStructure(const QString& mediaPath)
{
    QDir root(mediaPath);
    if (!root.exists()) root.mkpath(".");

    for (auto it = kSampleFiles.cbegin(); it != kSampleFiles.cend(); ++it) {
        QDir sub(mediaPath + "/" + it.key());
        if (!sub.exists()) sub.mkpath(".");
        for (const QString& fname : it.value()) {
            QFile f(sub.filePath(fname));
            if (!f.exists()) {f.open(QIODevice::WriteOnly);f.close();}
        }
    }
}

// ── 构造 ─────────────────────────────────────────────────────
AssetBrowserPanel::AssetBrowserPanel(QWidget* parent)
    : QWidget(parent)
{
    // 确保 Media 目录及示例文件存在
    ensureMediaStructure(mediaRoot());
    m_currentDir = mediaRoot();

    // ── 顶部工具栏 ────────────────────────────────────────────
    m_pathLabel = new QLabel("Media", this);
    m_pathLabel->setObjectName("pathLabel");

    m_searchBar = new QLineEdit(this);
    m_searchBar->setObjectName("assetSearch");
    m_searchBar->setPlaceholderText(tr("搜索"));
    m_searchBar->setFixedWidth(180);

    auto* btnBack = new QPushButton("←", this);
    auto* btnFwd  = new QPushButton("→", this);
    btnBack->setObjectName("navBtn");
    btnFwd->setObjectName("navBtn");
    btnBack->setFixedSize(20, 20);
    btnFwd->setFixedSize(20, 20);

    auto* topBar = new QHBoxLayout;
    topBar->setContentsMargins(6, 2, 6, 2);
    topBar->setSpacing(6);
    topBar->addWidget(btnBack);
    topBar->addWidget(btnFwd);
    topBar->addWidget(m_pathLabel, 1);
    topBar->addWidget(m_searchBar);

    // 导航按钮暂作上一级
    connect(btnBack, &QPushButton::clicked, this, [this]() {
        QDir d(m_currentDir);
        if (d.cdUp() && d.absolutePath().length() >=
                        mediaRoot().length()) {
            m_currentDir = d.absolutePath();
            QString rel = QDir(mediaRoot()).relativeFilePath(m_currentDir);
            m_pathLabel->setText(rel.isEmpty() ? "Media" : "Media / " + rel);
            populateFileList(m_currentDir, m_searchBar->text());
        }
    });

    // ── 内容区 ────────────────────────────────────────────────
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(1);

    setupFolderTree();
    setupFileList();

    m_splitter->addWidget(m_folderTree);
    m_splitter->addWidget(m_fileList);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({ 160, 600 });

    auto* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);
    vlay->addLayout(topBar);
    vlay->addWidget(m_splitter, 1);
    setLayout(vlay);

    // ── 信号连接 ──────────────────────────────────────────────
    connect(m_searchBar, &QLineEdit::textChanged,
            this, &AssetBrowserPanel::onSearchChanged);

    applyStyle();

    // 默认展示 Media 根目录（只有子目录，无文件）
    populateFileList(m_currentDir);
}

// 在 BottomPanel 完成 connect 后调用，补发初始化日志
void AssetBrowserPanel::initLog()
{
    emit logMessage(tr("[AssetBrowser] 初始化完成，根目录: %1").arg(m_currentDir), 0);

    QDir dir(m_currentDir);
    int dirs  = dir.entryList(QDir::Dirs  | QDir::NoDotAndDotDot).count();
    int files = dir.entryList(QDir::Files).count();
    emit logMessage(tr("[AssetBrowser] 当前目录包含 %1 个文件夹，%2 个文件")
                        .arg(dirs).arg(files), 0);
}

// ── 构建左侧目录树 ────────────────────────────────────────────
void AssetBrowserPanel::setupFolderTree()
{
    m_folderTree = new QTreeWidget(this);
    m_folderTree->setObjectName("folderTree");
    m_folderTree->setHeaderHidden(true);
    m_folderTree->setIndentation(16);
    m_folderTree->setRootIsDecorated(true);
    m_folderTree->setAnimated(true);
    m_folderTree->setIconSize(QSize(16, 16));
    m_folderTree->setContextMenuPolicy(Qt::NoContextMenu);
    m_folderTree->viewport()->installEventFilter(this);

    QFileIconProvider iconProvider;
    QString exeDir  = QCoreApplication::applicationDirPath();
    QString dirName = QDir(exeDir).dirName();

    auto* root = new QTreeWidgetItem(m_folderTree, { dirName });
    root->setData(0, Qt::UserRole, exeDir);
    root->setIcon(0, iconProvider.icon(QFileIconProvider::Computer));
    root->setExpanded(true);

    QTreeWidgetItem* mediaNode = buildTreeNode(root, mediaRoot(), true);
    Q_UNUSED(mediaNode)

    m_folderTree->setCurrentItem(root);

    connect(m_folderTree, &QTreeWidget::currentItemChanged,
            this, &AssetBrowserPanel::onFolderSelected);
}

// 递归构建目录树
QTreeWidgetItem* AssetBrowserPanel::buildTreeNode(
        QTreeWidgetItem* parent,
        const QString&   dirPath,
        bool             expanded)
{
    QFileIconProvider iconProvider;
    QDir dir(dirPath);
    auto* node = new QTreeWidgetItem(parent, { dir.dirName() });
    node->setData(0, Qt::UserRole, dirPath);
    node->setExpanded(expanded);
    node->setIcon(0, iconProvider.icon(QFileInfo(dirPath)));

    for (const QFileInfo& fi :
         dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    {
        buildTreeNode(node, fi.absoluteFilePath(), false);
    }
    return node;
}

// ── eventFilter：右键菜单 + OBJ 拖拽（从 fileList viewport 触发）────
bool AssetBrowserPanel::eventFilter(QObject* obj, QEvent* event)
{
    // ── 右键菜单 ──────────────────────────────────────────────
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::RightButton) {
            if (obj == m_folderTree->viewport())
                onFolderTreeContextMenu(me->pos());
            else if (obj == m_fileList->viewport())
                onFileListContextMenu(me->pos());
            return true;
        }
        // 左键按下：记录拖拽起点（在 fileList viewport 内）
        if (me->button() == Qt::LeftButton &&
            obj == m_fileList->viewport())
        {
            m_dragStartPos = me->pos();
            m_dragPending  = true;
            // 不 return true，让 QTableWidget 正常处理选中
        }
    }

    // ── OBJ 拖拽：在 fileList viewport 的 MouseMove 里发起 ───
    if (event->type() == QEvent::MouseMove &&
        obj == m_fileList->viewport() &&
        m_dragPending)
    {
        auto* me = static_cast<QMouseEvent*>(event);
        if (!(me->buttons() & Qt::LeftButton)) {
            m_dragPending = false;
            return QWidget::eventFilter(obj, event);
        }
        if ((me->pos() - m_dragStartPos).manhattanLength()
                < QApplication::startDragDistance())
            return QWidget::eventFilter(obj, event);

        // 取当前选中行（此时 QTableWidget 已处理过 Press，选中已更新）
        auto selected = m_fileList->selectedItems();
        if (selected.isEmpty()) { m_dragPending = false; return QWidget::eventFilter(obj, event); }

        int row = selected.first()->row();
        auto* nameItem = m_fileList->item(row, 0);
        if (!nameItem) { m_dragPending = false; return QWidget::eventFilter(obj, event); }

        QString path = nameItem->data(Qt::UserRole).toString();
        bool isObj = path.endsWith(".obj", Qt::CaseInsensitive);
        bool isFbx = path.endsWith(".fbx", Qt::CaseInsensitive);

        if (!isObj && !isFbx) {
            m_dragPending = false;
            return QWidget::eventFilter(obj, event);
        }

        m_dragPending = false;

        // 构造 MimeData
        QDrag*     drag = new QDrag(m_fileList);
        QMimeData* mime = new QMimeData;
        if (isObj)
        {
            mime->setData("application/x-obj-asset", path.toUtf8());
            mime->setData("application/x-primitive",
                QByteArray("OBJ:" + path.toUtf8()));
        }
        else if (isFbx)
        {
            mime->setData("application/x-fbx-asset", path.toUtf8());
            mime->setData("application/x-primitive",
                QByteArray("FBX:" + path.toUtf8()));
        }
        drag->setMimeData(mime);

        // 拖拽缩略图
        QPixmap pm(48, 48);
        pm.fill(QColor(40, 80, 140, 200));
        QPainter painter(&pm);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 8));
        painter.drawText(pm.rect(), Qt::AlignCenter, isObj ? "OBJ" : "FBX");
        painter.end();
        drag->setPixmap(pm);
        drag->setHotSpot(QPoint(24, 24));

        emit logMessage(
            tr("[AssetBrowser] 开始拖拽 %1: %2")
            .arg(isObj ? "OBJ" : "FBX")
            .arg(QFileInfo(path).fileName()),
            0);

        drag->exec(Qt::CopyAction);
        return true;   // 已处理，不再传递
    }

    // ── 鼠标释放：重置拖拽标志 ───────────────────────────────
    if (event->type() == QEvent::MouseButtonRelease &&
        obj == m_fileList->viewport())
    {
        m_dragPending = false;
    }

    return QWidget::eventFilter(obj, event);
}

// ── 鼠标按下 / 移动：拖拽逻辑已迁移至 eventFilter ─────────────
void AssetBrowserPanel::mousePressEvent(QMouseEvent* e)
{
    QWidget::mousePressEvent(e);
}

// ── 鼠标移动：拖拽逻辑已迁移至 eventFilter ─────────────────────
void AssetBrowserPanel::mouseMoveEvent(QMouseEvent* e)
{
    QWidget::mouseMoveEvent(e);
}

// ── 构建右侧文件表格 ──────────────────────────────────────────
void AssetBrowserPanel::setupFileList()
{
    m_fileList = new QTableWidget(this);
    m_fileList->setObjectName("fileList");
    m_fileList->setColumnCount(4);
    m_fileList->setHorizontalHeaderLabels(
        { tr("名称"), tr("类型"), tr("大小"), tr("修改日期") });
    m_fileList->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    m_fileList->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    m_fileList->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    m_fileList->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    m_fileList->verticalHeader()->setVisible(false);
    m_fileList->verticalHeader()->setDefaultSectionSize(22);
    m_fileList->setShowGrid(false);
    m_fileList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileList->setAlternatingRowColors(false);
    m_fileList->setContextMenuPolicy(Qt::NoContextMenu);
    m_fileList->viewport()->installEventFilter(this);

    // 禁用 Qt 内置拖拽，改由 eventFilter 统一处理 OBJ 拖拽
    m_fileList->setDragEnabled(false);
    m_fileList->setDragDropMode(QAbstractItemView::NoDragDrop);

    connect(m_fileList, &QTableWidget::itemClicked,
            this, &AssetBrowserPanel::onFileClicked);
    connect(m_fileList, &QTableWidget::itemDoubleClicked,
            this, &AssetBrowserPanel::onFileDoubleClicked);
}

// ── 填充右侧文件列表 ──────────────────────────────────────────
void AssetBrowserPanel::populateFileList(const QString& dirPath,
                                          const QString& filter)
{
    m_fileList->setRowCount(0);
    QDir dir(dirPath);
    QFileIconProvider iconProvider;

    QFileInfoList entries =
        dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    entries += dir.entryInfoList(QDir::Files, QDir::Name);

    for (const QFileInfo& fi : entries) {
        if (!filter.isEmpty() &&
            !fi.fileName().contains(filter, Qt::CaseInsensitive))
            continue;

        int row = m_fileList->rowCount();
        m_fileList->insertRow(row);

        QString typeName = fi.isDir()
            ? tr("[文件夹]")
            : fi.suffix().isEmpty() ? tr("文件") : fi.suffix().toUpper();

        QString sizeStr;
        if (fi.isFile()) {
            qint64 sz = fi.size();
            sizeStr = sz < 1024 ? QString("%1 B").arg(sz)
                    : sz < 1024*1024 ? QString("%1 KB").arg(sz/1024)
                    : QString("%1 MB").arg(sz/(1024*1024));
        }

        QString dateStr = fi.lastModified().toString("yyyy/MM/dd HH:mm");

        auto* nameItem = new QTableWidgetItem(fi.fileName());
        nameItem->setData(Qt::UserRole, fi.absoluteFilePath());
        nameItem->setIcon(iconProvider.icon(fi));
        if (fi.isDir())
            nameItem->setForeground(QColor("#6cb6ff"));
        // OBJ 文件用特殊高亮色
        else if (fi.suffix().compare("obj", Qt::CaseInsensitive) == 0)
            nameItem->setForeground(QColor("#e8b930"));
        // FBX 文件用另一种高亮色
        else if (fi.suffix().compare("fbx", Qt::CaseInsensitive) == 0)
            nameItem->setForeground(QColor("#569cd6"));

        m_fileList->setItem(row, 0, nameItem);
        m_fileList->setItem(row, 1, new QTableWidgetItem(typeName));
        m_fileList->setItem(row, 2, new QTableWidgetItem(sizeStr));
        m_fileList->setItem(row, 3, new QTableWidgetItem(dateStr));
    }
}

// ── Slots ─────────────────────────────────────────────────────
void AssetBrowserPanel::onFolderSelected(QTreeWidgetItem* cur,
                                          QTreeWidgetItem* /*prev*/)
{
    if (!cur) return;
    QString path = cur->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) return;

    m_currentDir = path;
    QString rel = QDir(QCoreApplication::applicationDirPath())
                      .relativeFilePath(path);
    m_pathLabel->setText(rel.replace("/", " / "));
    populateFileList(path, m_searchBar->text());
    emit logMessage(tr("[AssetBrowser] 浏览目录: %1").arg(path), 0);
}

void AssetBrowserPanel::onSearchChanged(const QString& text)
{
    populateFileList(m_currentDir, text);
    if (!text.isEmpty())
        emit logMessage(tr("[AssetBrowser] 搜索: \"%1\" 在 %2").arg(text, m_currentDir), 0);
}

void AssetBrowserPanel::onFileClicked(QTableWidgetItem* item)
{
    if (!item || item->column() != 0) return;
    QString path = item->data(Qt::UserRole).toString();
    if (QFileInfo(path).isFile()) {
        emit assetSelected(path);
        emit logMessage(tr("[AssetBrowser] 选中文件: %1").arg(path), 0);
    }
}

void AssetBrowserPanel::onFileDoubleClicked(QTableWidgetItem* item)
{
    if (!item || item->column() != 0) return;
    QString path = item->data(Qt::UserRole).toString();
    if (QFileInfo(path).isDir()) {
        m_currentDir = path;
        QString rel = QDir(QCoreApplication::applicationDirPath())
                          .relativeFilePath(path);
        m_pathLabel->setText(rel.replace("/", " / "));
        populateFileList(path, m_searchBar->text());
        emit logMessage(tr("[AssetBrowser] 进入目录: %1").arg(path), 0);
    }
}

// ── 文件 导入 ─────────────────────────────────────────────────
void AssetBrowserPanel::importObjFile()
{
    // 弹出文件选择框，只允许 OBJ
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("导入 OBJ 模型"),
        QDir::homePath(),
        tr("OBJ 模型 (*.obj);;所有文件 (*)"));

    if (files.isEmpty()) return;

    // 确保目标子目录 models 存在
    QString modelsDir = m_currentDir;   // 导入到当前浏览目录
    int importedCount = 0;

    for (const QString& src : files) {
        QFileInfo fi(src);
        QString dest = modelsDir + "/" + fi.fileName();

        // 同名则自动重命名
        if (QFileInfo::exists(dest)) {
            QString base = fi.completeBaseName();
            QString ext  = ".obj";
            int n = 1;
            do { dest = modelsDir + "/" + base + QString("_%1").arg(n++) + ext; }
            while (QFileInfo::exists(dest));
        }

        if (QFile::copy(src, dest)) {
            ++importedCount;
            emit logMessage(tr("[AssetBrowser] 导入 OBJ: %1").arg(dest), 0);
        } else {
            emit logMessage(tr("[AssetBrowser] 导入失败: %1").arg(src), 2);
        }
    }

    if (importedCount > 0) {
        populateFileList(m_currentDir, m_searchBar->text());
        refreshTreeNode(m_currentDir);
        emit logMessage(tr("[AssetBrowser] 共导入 %1 个 OBJ 文件").arg(importedCount), 0);
    }
}
void AssetBrowserPanel::importFbxFile()
{
    // 弹出文件选择框，只允许 fbx
    QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("导入 fbx 模型"),
        QDir::homePath(),
        tr("FBX 模型 (*.fbx);;所有文件 (*)"));

    if (files.isEmpty()) return;

    // 确保目标子目录 models 存在
    QString modelsDir = m_currentDir;   // 导入到当前浏览目录
    int importedCount = 0;

    for (const QString& src : files) {
        QFileInfo fi(src);
        QString dest = modelsDir + "/" + fi.fileName();

        // 同名则自动重命名
        if (QFileInfo::exists(dest)) {
            QString base = fi.completeBaseName();
            QString ext = ".fbx";
            int n = 1;
            do { dest = modelsDir + "/" + base + QString("_%1").arg(n++) + ext; } while (QFileInfo::exists(dest));
        }

        if (QFile::copy(src, dest)) {
            ++importedCount;
            emit logMessage(tr("[AssetBrowser] 导入 FBX: %1").arg(dest), 0);
        }
        else {
            emit logMessage(tr("[AssetBrowser] 导入失败: %1").arg(src), 2);
        }
    }

    if (importedCount > 0) {
        populateFileList(m_currentDir, m_searchBar->text());
        refreshTreeNode(m_currentDir);
        emit logMessage(tr("[AssetBrowser] 共导入 %1 个 FBX 文件").arg(importedCount), 0);
    }
}
// ── 右侧文件列表右键菜单 ──────────────────────────────────────
void AssetBrowserPanel::onFileListContextMenu(const QPoint& pos)
{
    QString selectedPath;
    QTableWidgetItem* hitItem = m_fileList->itemAt(pos);
    int row = -1;
    if (hitItem) {
        row = hitItem->row();
    } else {
        auto selected = m_fileList->selectedItems();
        if (!selected.isEmpty())
            row = selected.first()->row();
    }
    if (row >= 0) {
        auto* nameItem = m_fileList->item(row, 0);
        if (nameItem) {
            selectedPath = nameItem->data(Qt::UserRole).toString();
            m_fileList->selectRow(row);
        }
    }

    QMenu menu(this);
    menu.setObjectName("contextMenu");

    // ── 导入 OBJ（总是显示，无需选中项）─────────────────────
    QAction* actImport = menu.addAction(tr("📥  导入 OBJ 模型..."));
    QAction* actImportFBX = menu.addAction(tr("📥  导入 FBX 模型..."));
    actImport->setObjectName("importAction");
    actImportFBX->setObjectName("importFBXAction");
    connect(actImport, &QAction::triggered, this, &AssetBrowserPanel::importObjFile);
    connect(actImportFBX, &QAction::triggered, this, &AssetBrowserPanel::importFbxFile);
    menu.addSeparator();

    // 有选中项时才显示文件操作
    if (!selectedPath.isEmpty()) {
        QAction* actRename = menu.addAction(tr("重命名"));
        QAction* actCopy   = menu.addAction(tr("复制"));
        QAction* actCut    = menu.addAction(tr("剪切"));
        menu.addSeparator();
        QAction* actDelete = menu.addAction(tr("删除"));
        menu.addSeparator();

        connect(actRename, &QAction::triggered, this, [this, selectedPath]() {
            QFileInfo fi(selectedPath);
            bool ok;
            QString newName = QInputDialog::getText(
                this, tr("重命名"), tr("新名称:"),
                QLineEdit::Normal, fi.fileName(), &ok);
            if (!ok || newName.isEmpty() || newName == fi.fileName()) return;
            QString newPath = fi.dir().filePath(newName);
            if (QFile::rename(selectedPath, newPath)) {
                populateFileList(m_currentDir, m_searchBar->text());
                refreshTreeNode(m_currentDir);
                emit logMessage(tr("[AssetBrowser] 重命名: %1  →  %2")
                    .arg(fi.fileName(), newName), 0);
            } else {
                emit logMessage(tr("[AssetBrowser] 重命名失败: %1").arg(fi.fileName()), 2);
                QMessageBox::warning(this, tr("重命名失败"),
                    tr("无法重命名: %1").arg(fi.fileName()));
            }
        });

        connect(actCopy, &QAction::triggered, this, [this, selectedPath]() {
            m_clipboardPath  = selectedPath;
            m_clipboardIsCut = false;
            emit logMessage(tr("[AssetBrowser] 已复制: %1")
                .arg(QFileInfo(selectedPath).fileName()), 0);
        });

        connect(actCut, &QAction::triggered, this, [this, selectedPath]() {
            m_clipboardPath  = selectedPath;
            m_clipboardIsCut = true;
            emit logMessage(tr("[AssetBrowser] 已剪切: %1")
                .arg(QFileInfo(selectedPath).fileName()), 1);
        });

        connect(actDelete, &QAction::triggered, this, [this, selectedPath]() {
            QFileInfo fi(selectedPath);
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("确认删除"));
            msgBox.setText(tr("确定要删除 \"%1\" 吗？").arg(fi.fileName()));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::No);
            if (msgBox.exec() != QMessageBox::Yes) {
                emit logMessage(tr("[AssetBrowser] 取消删除: %1").arg(fi.fileName()), 0);
                return;
            }
            bool ok = fi.isDir()
                ? QDir(selectedPath).removeRecursively()
                : QFile::remove(selectedPath);
            if (ok) {
                populateFileList(m_currentDir, m_searchBar->text());
                refreshTreeNode(m_currentDir);
                emit logMessage(tr("[AssetBrowser] 已删除: %1").arg(selectedPath), 1);
            } else {
                emit logMessage(tr("[AssetBrowser] 删除失败: %1").arg(selectedPath), 2);
                QMessageBox::warning(this, tr("删除失败"),
                    tr("无法删除: %1").arg(fi.fileName()));
            }
        });
    }

    // 粘贴
    if (!m_clipboardPath.isEmpty()) {
        QAction* actPaste = menu.addAction(tr("粘贴"));
        connect(actPaste, &QAction::triggered, this, &AssetBrowserPanel::filePaste);
    }

    menu.addSeparator();
    QAction* actNewFolder = menu.addAction(tr("新建文件夹"));
    connect(actNewFolder, &QAction::triggered, this, &AssetBrowserPanel::fileNewFolder);

    menu.setStyleSheet(R"(
        QMenu {
            background: #2d2d2d;
            border: 1px solid #3f3f46;
            color: #cccccc;
            font-size: 12px;
            padding: 2px 0;
        }
        QMenu::item {
            padding: 4px 24px 4px 16px;
        }
        QMenu::item:selected {
            background: #094771;
            color: #ffffff;
        }
        QMenu::item#importAction {
            color: #e8b930;
            font-weight: bold;
        }
        QMenu::separator {
            height: 1px;
            background: #3f3f46;
            margin: 2px 8px;
        }
    )");

    menu.exec(m_fileList->viewport()->mapToGlobal(pos));
}

// ── 左侧目录树右键菜单 ────────────────────────────────────────
void AssetBrowserPanel::onFolderTreeContextMenu(const QPoint& pos)
{
    QModelIndex idx = m_folderTree->indexAt(pos);
    QTreeWidgetItem* item = idx.isValid()
        ? m_folderTree->itemFromIndex(idx)
        : m_folderTree->currentItem();
    if (!item) return;
    QString dirPath = item->data(0, Qt::UserRole).toString();
    if (dirPath.isEmpty()) return;

    QMenu menu(this);

    QAction* actRename    = menu.addAction(tr("重命名文件夹"));
    QAction* actNewFolder = menu.addAction(tr("新建子文件夹"));
    menu.addSeparator();
    QAction* actDelete = menu.addAction(tr("删除文件夹"));

    connect(actRename, &QAction::triggered, this, [this, item, dirPath]() {
        QFileInfo fi(dirPath);
        bool ok;
        QString newName = QInputDialog::getText(
            this, tr("重命名文件夹"),
            tr("新名称:"), QLineEdit::Normal, fi.fileName(), &ok);
        if (!ok || newName.isEmpty() || newName == fi.fileName()) return;
        QString newPath = fi.dir().filePath(newName);
        if (QFile::rename(dirPath, newPath)) {
            item->setText(0, newName);
            item->setData(0, Qt::UserRole, newPath);
            if (m_currentDir == dirPath) {
                m_currentDir = newPath;
                populateFileList(newPath, m_searchBar->text());
            }
            emit logMessage(tr("[AssetBrowser] 文件夹重命名: %1  →  %2")
                .arg(fi.fileName(), newName), 0);
        } else {
            emit logMessage(tr("[AssetBrowser] 文件夹重命名失败: %1").arg(fi.fileName()), 2);
            QMessageBox::warning(this, tr("重命名失败"),
                tr("无法重命名文件夹: %1").arg(fi.fileName()));
        }
    });

    connect(actNewFolder, &QAction::triggered, this, [this, item, dirPath]() {
        bool ok;
        QString name = QInputDialog::getText(
            this, tr("新建文件夹"), tr("文件夹名称:"),
            QLineEdit::Normal, tr("新建文件夹"), &ok);
        if (!ok || name.isEmpty()) return;
        QString newPath = dirPath + "/" + name;
        if (QDir(dirPath).mkdir(name)) {
            QFileIconProvider iconProvider;
            auto* child = new QTreeWidgetItem(item, { name });
            child->setData(0, Qt::UserRole, newPath);
            child->setIcon(0, iconProvider.icon(QFileInfo(newPath)));
            item->setExpanded(true);
            populateFileList(m_currentDir, m_searchBar->text());
            emit logMessage(tr("[AssetBrowser] 新建文件夹: %1").arg(newPath), 0);
        } else {
            emit logMessage(tr("[AssetBrowser] 新建文件夹失败: %1").arg(newPath), 2);
        }
    });

    connect(actDelete, &QAction::triggered, this, [this, item, dirPath]() {
        if (!item) return;
        QFileInfo fi(dirPath);
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("确认删除"));
        msgBox.setText(tr("确定要删除文件夹 \"%1\" 及其所有内容吗？").arg(fi.fileName()));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        if (msgBox.exec() != QMessageBox::Yes) {
            emit logMessage(tr("[AssetBrowser] 取消删除文件夹: %1").arg(fi.fileName()), 0);
            return;
        }
        if (QDir(dirPath).removeRecursively()) {
            QTreeWidgetItem* parentItem = item->parent();
            QString parentPath = parentItem
                ? parentItem->data(0, Qt::UserRole).toString()
                : mediaRoot();
            if (parentItem)
                parentItem->removeChild(item);
            else
                delete item;
            emit logMessage(tr("[AssetBrowser] 已删除文件夹: %1").arg(dirPath), 1);
            if (m_currentDir == dirPath || m_currentDir.startsWith(dirPath + "/")) {
                m_currentDir = parentPath;
                QString rel = QDir(QCoreApplication::applicationDirPath())
                                  .relativeFilePath(m_currentDir);
                m_pathLabel->setText(rel.replace("/", " / "));
                populateFileList(m_currentDir, m_searchBar->text());
            }
        } else {
            emit logMessage(tr("[AssetBrowser] 删除文件夹失败: %1").arg(dirPath), 2);
            QMessageBox::warning(this, tr("删除失败"),
                tr("无法删除文件夹: %1").arg(fi.fileName()));
        }
    });

    menu.setStyleSheet(R"(
        QMenu {
            background: #2d2d2d;
            border: 1px solid #3f3f46;
            color: #cccccc;
            font-size: 12px;
            padding: 2px 0;
        }
        QMenu::item { padding: 4px 24px 4px 16px; }
        QMenu::item:selected { background: #094771; color: #ffffff; }
        QMenu::separator { height: 1px; background: #3f3f46; margin: 2px 8px; }
    )");

    menu.exec(m_folderTree->viewport()->mapToGlobal(pos));
}

// ── 文件操作辅助 ──────────────────────────────────────────────
void AssetBrowserPanel::fileCopy()
{
    auto* item = m_fileList->currentItem();
    if (!item) return;
    int row = item->row();
    auto* nameItem = m_fileList->item(row, 0);
    if (nameItem) {
        m_clipboardPath  = nameItem->data(Qt::UserRole).toString();
        m_clipboardIsCut = false;
    }
}

void AssetBrowserPanel::fileCut()
{
    fileCopy();
    m_clipboardIsCut = true;
}

void AssetBrowserPanel::filePaste()
{
    if (m_clipboardPath.isEmpty()) return;
    QFileInfo fi(m_clipboardPath);
    if (!fi.exists()) { m_clipboardPath.clear(); return; }

    QString dest = m_currentDir + "/" + fi.fileName();
    if (QFileInfo::exists(dest)) {
        QString base = fi.completeBaseName();
        QString ext  = fi.suffix().isEmpty() ? "" : "." + fi.suffix();
        int n = 1;
        do { dest = m_currentDir + "/" + base + QString("_%1").arg(n++) + ext; }
        while (QFileInfo::exists(dest));
    }

    bool ok = false;
    if (fi.isDir()) {
        std::function<bool(const QString&, const QString&)> copyDir;
        copyDir = [&](const QString& src, const QString& dst) -> bool {
            QDir().mkpath(dst);
            for (auto& e : QDir(src).entryInfoList(
                     QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
                QString d = dst + "/" + e.fileName();
                if (e.isDir()) { if (!copyDir(e.absoluteFilePath(), d)) return false; }
                else           { if (!QFile::copy(e.absoluteFilePath(), d)) return false; }
            }
            return true;
        };
        ok = copyDir(m_clipboardPath, dest);
        if (ok && m_clipboardIsCut) QDir(m_clipboardPath).removeRecursively();
    } else {
        ok = QFile::copy(m_clipboardPath, dest);
        if (ok && m_clipboardIsCut) QFile::remove(m_clipboardPath);
    }

    if (ok) {
        QString op = m_clipboardIsCut ? tr("移动") : tr("复制");
        emit logMessage(tr("[AssetBrowser] %1: %2  →  %3")
            .arg(op, m_clipboardPath, dest), 0);
        if (m_clipboardIsCut) m_clipboardPath.clear();
        populateFileList(m_currentDir, m_searchBar->text());
        refreshTreeNode(m_currentDir);
    } else {
        emit logMessage(tr("[AssetBrowser] 粘贴失败: %1").arg(fi.fileName()), 2);
        QMessageBox::warning(this, tr("粘贴失败"),
            tr("无法粘贴: %1").arg(fi.fileName()));
    }
}

void AssetBrowserPanel::fileRename()
{
    auto* item = m_fileList->currentItem();
    if (!item) return;
    int row = item->row();
    auto* nameItem = m_fileList->item(row, 0);
    if (!nameItem) return;
    QString path = nameItem->data(Qt::UserRole).toString();
    QFileInfo fi(path);
    bool ok;
    QString newName = QInputDialog::getText(
        this, tr("重命名"), tr("新名称:"),
        QLineEdit::Normal, fi.fileName(), &ok);
    if (!ok || newName.isEmpty() || newName == fi.fileName()) return;
    if (QFile::rename(path, fi.dir().filePath(newName))) {
        populateFileList(m_currentDir, m_searchBar->text());
        refreshTreeNode(m_currentDir);
        emit logMessage(tr("[AssetBrowser] 重命名: %1  →  %2")
            .arg(fi.fileName(), newName), 0);
    } else {
        emit logMessage(tr("[AssetBrowser] 重命名失败: %1").arg(fi.fileName()), 2);
    }
}

void AssetBrowserPanel::fileDelete()
{
    auto* item = m_fileList->currentItem();
    if (!item) return;
    int row = item->row();
    auto* nameItem = m_fileList->item(row, 0);
    if (!nameItem) return;
    QString path = nameItem->data(Qt::UserRole).toString();
    QFileInfo fi(path);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("确认删除"));
    msgBox.setText(tr("确定要删除 \"%1\" 吗？").arg(fi.fileName()));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    if (msgBox.exec() != QMessageBox::Yes) {
        emit logMessage(tr("[AssetBrowser] 取消删除: %1").arg(fi.fileName()), 0);
        return;
    }
    bool ok = fi.isDir()
        ? QDir(path).removeRecursively()
        : QFile::remove(path);
    if (ok) {
        populateFileList(m_currentDir, m_searchBar->text());
        refreshTreeNode(m_currentDir);
        emit logMessage(tr("[AssetBrowser] 已删除: %1").arg(path), 1);
    } else {
        emit logMessage(tr("[AssetBrowser] 删除失败: %1").arg(path), 2);
        QMessageBox::warning(this, tr("删除失败"),
            tr("无法删除: %1").arg(fi.fileName()));
    }
}

void AssetBrowserPanel::fileNewFolder()
{
    bool ok;
    QString name = QInputDialog::getText(
        this, tr("新建文件夹"), tr("文件夹名称:"),
        QLineEdit::Normal, tr("新建文件夹"), &ok);
    if (!ok || name.isEmpty()) return;
    QString newPath = m_currentDir + "/" + name;
    if (QDir(m_currentDir).mkdir(name)) {
        populateFileList(m_currentDir, m_searchBar->text());
        refreshTreeNode(m_currentDir);
        emit logMessage(tr("[AssetBrowser] 新建文件夹: %1").arg(newPath), 0);
    } else {
        emit logMessage(tr("[AssetBrowser] 新建文件夹失败: %1").arg(newPath), 2);
    }
}

void AssetBrowserPanel::refreshTreeNode(const QString& dirPath)
{
    QFileIconProvider iconProvider;
    QList<QTreeWidgetItem*> items = m_folderTree->findItems(
        QDir(dirPath).dirName(), Qt::MatchExactly | Qt::MatchRecursive);
    for (auto* it : items) {
        if (it->data(0, Qt::UserRole).toString() == dirPath) {
            while (it->childCount()) delete it->takeChild(0);
            for (const QFileInfo& fi :
                 QDir(dirPath).entryInfoList(
                     QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
            {
                buildTreeNode(it, fi.absoluteFilePath(), false);
            }
            break;
        }
    }
}

void AssetBrowserPanel::applyStyle()
{
    setStyleSheet(R"(
        AssetBrowserPanel {
            background: #252526;
        }
        QLabel#pathLabel {
            color: #cccccc;
            font-size: 11px;
            background: transparent;
        }
        QLineEdit#assetSearch {
            background: #3c3c3c;
            border: 1px solid #555555;
            border-radius: 3px;
            color: #cccccc;
            font-size: 11px;
            padding: 1px 6px;
            height: 20px;
        }
        QLineEdit#assetSearch:focus {
            border-color: #0078d4;
        }
        QPushButton#navBtn {
            background: transparent;
            border: none;
            color: #888888;
            font-size: 13px;
            border-radius: 3px;
        }
        QPushButton#navBtn:hover {
            background: #3f3f46;
            color: #cccccc;
        }
        QTreeWidget#folderTree {
            background: #252526;
            border: none;
            border-right: 1px solid #1b1b1c;
            color: #cccccc;
            font-size: 12px;
            outline: none;
        }
        QTreeWidget#folderTree::item {
            height: 22px;
            padding-left: 2px;
        }
        QTreeWidget#folderTree::item:hover {
            background: #2a2d2e;
        }
        QTreeWidget#folderTree::item:selected {
            background: #094771;
            color: #ffffff;
        }
        QTreeWidget#folderTree::branch {
            background: #252526;
        }
        QTreeWidget#folderTree::branch:has-children:!has-siblings:closed,
        QTreeWidget#folderTree::branch:closed:has-children:has-siblings {
            border-image: none;
            image: url(:/qt-project.org/styles/commonstyle/images/right-arrow.png);
        }
        QTreeWidget#folderTree::branch:open:has-children:!has-siblings,
        QTreeWidget#folderTree::branch:open:has-children:has-siblings {
            border-image: none;
            image: url(:/qt-project.org/styles/commonstyle/images/down-arrow.png);
        }
        QTableWidget#fileList {
            background: #1e1e1e;
            gridline-color: transparent;
            border: none;
            color: #cccccc;
            font-size: 12px;
            outline: none;
        }
        QTableWidget#fileList::item {
            padding: 0 6px;
            border: none;
        }
        QTableWidget#fileList::item:hover {
            background: #2a2d2e;
        }
        QTableWidget#fileList::item:selected {
            background: #094771;
            color: #ffffff;
        }
        QHeaderView::section {
            background: #2d2d2d;
            color: #888888;
            font-size: 11px;
            border: none;
            border-right: 1px solid #1b1b1c;
            border-bottom: 1px solid #1b1b1c;
            padding: 2px 6px;
            height: 20px;
        }
        QScrollBar:vertical, QScrollBar:horizontal {
            background: #252526;
            width: 8px; height: 8px;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #555555;
            border-radius: 4px;
            min-height: 20px; min-width: 20px;
        }
        QScrollBar::handle:hover { background: #777777; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
        QSplitter::handle { background: #1b1b1c; }
    )");
}


// ═══════════════════════════════════════════════════════════════
//  LogConsolePanel
// ═══════════════════════════════════════════════════════════════
LogConsolePanel::LogConsolePanel(QWidget* parent)
    : QWidget(parent)
{
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setObjectName("logOutput");
    m_output->setAcceptRichText(true);
    m_output->document()->setMaximumBlockCount(2000);

    m_clearBtn = new QPushButton(tr("清空"), this);
    m_clearBtn->setObjectName("logClearBtn");
    m_clearBtn->setFixedHeight(22);
    connect(m_clearBtn, &QPushButton::clicked, this, &LogConsolePanel::clear);

    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(6, 2, 6, 2);
    toolbar->setSpacing(4);
    toolbar->addStretch();
    toolbar->addWidget(m_clearBtn);

    auto* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);
    vlay->addLayout(toolbar);
    vlay->addWidget(m_output, 1);
    setLayout(vlay);

    applyStyle();

    appendMessage(tr("[INFO]  Level 'content/levels/basic.level' loaded."), 0);
    appendMessage(tr("[INFO]  Engine initialized.  OpenGL 4.6"), 0);
}

void LogConsolePanel::appendMessage(const QString& msg, int level)
{
    static const char* colors[] = { "#cccccc", "#e8b930", "#f44747" };
    const char* color = (level >= 0 && level <= 2) ? colors[level] : colors[0];
    QString timestamp = QTime::currentTime().toString("HH:mm:ss");
    QTextCursor cursor(m_output->document());
    cursor.movePosition(QTextCursor::End);
    if (!m_output->document()->isEmpty())
        cursor.insertBlock();
    cursor.insertHtml(
        QString("<span style='color:#555555;font-size:11px;'>%1</span>"
                "&nbsp;<span style='color:%2;font-size:12px;'>%3</span>")
            .arg(timestamp, color, msg.toHtmlEscaped())
    );
    m_output->moveCursor(QTextCursor::End);
}

void LogConsolePanel::clear()
{
    m_output->clear();
}

void LogConsolePanel::applyStyle()
{
    setStyleSheet(R"(
        LogConsolePanel {
            background: #1e1e1e;
        }
        QTextEdit#logOutput {
            background: #1e1e1e;
            border: none;
            color: #cccccc;
            font-family: Consolas, monospace;
            font-size: 12px;
        }
        QPushButton#logClearBtn {
            background: #3c3c3c;
            border: 1px solid #555;
            border-radius: 3px;
            color: #aaaaaa;
            font-size: 11px;
            padding: 0 10px;
        }
        QPushButton#logClearBtn:hover {
            background: #4f4f4f;
            color: #ffffff;
        }
        QScrollBar:vertical {
            background: #1e1e1e;
            width: 8px;
        }
        QScrollBar::handle:vertical {
            background: #555555;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover { background: #777777; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
    )");
}


// ═══════════════════════════════════════════════════════════════
//  AssetPreviewPanel  —  图片/OBJ 双模式预览
// ═══════════════════════════════════════════════════════════════
AssetPreviewPanel::AssetPreviewPanel(QWidget* parent)
    : QWidget(parent)
{
    // 不再 setFixedWidth，让 BottomPanel 的 rightWidget 控制宽度
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_noSelLabel = new QLabel(tr("No previewable\nasset selected"), this);
    m_noSelLabel->setObjectName("noSelLabel");
    m_noSelLabel->setAlignment(Qt::AlignCenter);
    m_noSelLabel->setWordWrap(true);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setObjectName("previewLabel");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setScaledContents(false);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewLabel->setVisible(false);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setObjectName("infoLabel");
    m_infoLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setFixedHeight(48);
    m_infoLabel->setVisible(false);

    // OBJ 3D 预览：占满可用高度（伸展填充）
    m_objPreview = new ObjPreviewWidget(this);
    m_objPreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_objPreview->setMinimumHeight(120);
    m_objPreview->setVisible(false);

    auto* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(4, 4, 4, 4);
    vlay->setSpacing(4);
    // noSelLabel 居中占满
    vlay->addStretch(1);
    vlay->addWidget(m_noSelLabel,  0, Qt::AlignCenter);
    // 图片预览拉伸填满
    vlay->addWidget(m_previewLabel, 1);
    // OBJ 预览拉伸填满（stretch=1 与 previewLabel 互斥，同时只显示其一）
    vlay->addWidget(m_objPreview,  1);
    vlay->addWidget(m_infoLabel,   0);
    vlay->addStretch(1);
    setLayout(vlay);

    applyStyle();
}

void AssetPreviewPanel::previewAsset(const QString& path)
{
    if (path.isEmpty()) {
        m_noSelLabel->setVisible(true);
        m_previewLabel->setVisible(false);
        m_infoLabel->setVisible(false);
        m_objPreview->clearMesh();
        m_objPreview->setVisible(false);
        return;
    }

    QFileInfo fi(path);

    // ── OBJ 文件 → 3D 预览 ──────────────────────────────────
    if (fi.suffix().compare("obj", Qt::CaseInsensitive) == 0) {
        m_noSelLabel->setVisible(false);
        m_previewLabel->setVisible(false);
        m_objPreview->setVisible(true);

        ObjMesh mesh = ObjMesh::load(path);
        if (mesh.valid) {
            m_objPreview->loadMesh(mesh);
            qint64 sz = fi.size();
            QString sizeStr = sz < 1024 ? QString("%1 B").arg(sz)
                            : sz < 1024*1024 ? QString("%1 KB").arg(sz/1024)
                            : QString("%1 MB").arg(sz/(1024*1024));
            int triCount = (int)mesh.indices.size() / 3;
            m_infoLabel->setText(
                QString("%1\n%2 三角面\n%3")
                    .arg(fi.fileName())
                    .arg(triCount)
                    .arg(sizeStr));
            m_infoLabel->setVisible(true);
        } else {
            m_objPreview->clearMesh();
            m_infoLabel->setText(tr("OBJ 解析失败\n%1").arg(fi.fileName()));
            m_infoLabel->setVisible(true);
        }
        return;
    }
    // ── FBX 文件 → 3D 预览 ──────────────────────────────────
    if (fi.suffix().compare("fbx", Qt::CaseInsensitive) == 0) {
        m_noSelLabel->setVisible(false);
        m_previewLabel->setVisible(false);
        m_objPreview->setVisible(true);

        ObjMesh mesh = ObjMesh::load(path);
        if (mesh.valid) {
            m_objPreview->loadMesh(mesh);
            qint64 sz = fi.size();
            QString sizeStr = sz < 1024 ? QString("%1 B").arg(sz)
                : sz < 1024 * 1024 ? QString("%1 KB").arg(sz / 1024)
                : QString("%1 MB").arg(sz / (1024 * 1024));
            int triCount = (int)mesh.indices.size() / 3;
            m_infoLabel->setText(
                QString("%1\n%2 三角面\n%3")
                .arg(fi.fileName())
                .arg(triCount)
                .arg(sizeStr));
            m_infoLabel->setVisible(true);
        }
        else {
            m_objPreview->clearMesh();
            m_infoLabel->setText(tr("FBX 解析失败\n%1").arg(fi.fileName()));
            m_infoLabel->setVisible(true);
        }
        return;
    }

    // ── 图片文件 ─────────────────────────────────────────────
    m_objPreview->clearMesh();
    m_objPreview->setVisible(false);

    QPixmap px(path);
    if (!px.isNull()) {
        // 动态按面板实际可用区域缩放（扣掉 infoLabel 固定高度和边距）
        int availW = qMax(width()  - 8,  80);
        int availH = qMax(height() - 60, 60);
        QPixmap scaled = px.scaled(availW, availH,
                                   Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_previewLabel->setPixmap(scaled);
        m_previewLabel->setVisible(true);
        m_infoLabel->setText(QString("%1\n%2 x %3")
            .arg(fi.fileName())
            .arg(px.width()).arg(px.height()));
        m_infoLabel->setVisible(true);
        m_noSelLabel->setVisible(false);
    } else {
        // 其他不可预览资产
        m_noSelLabel->setText(fi.fileName());
        m_noSelLabel->setVisible(true);
        m_previewLabel->setVisible(false);
        m_infoLabel->setVisible(false);
    }
}

void AssetPreviewPanel::applyStyle()
{
    setStyleSheet(R"(
        AssetPreviewPanel {
            background: #252526;
            border-left: 1px solid #1b1b1c;
        }
        QLabel#noSelLabel {
            color: #666666;
            font-size: 11px;
        }
        QLabel#infoLabel {
            color: #888888;
            font-size: 11px;
        }
    )");
}


// ═══════════════════════════════════════════════════════════════
//  BottomPanel
// ═══════════════════════════════════════════════════════════════
BottomPanel::BottomPanel(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(220);

    m_assetBrowser = new AssetBrowserPanel(this);
    m_logConsole   = new LogConsolePanel(this);
    m_assetPreview = new AssetPreviewPanel(this);

    m_tabBar = new QTabBar(this);
    m_tabBar->setObjectName("bottomTabBar");
    m_tabBar->setExpanding(false);
    m_tabBar->addTab(tr("Asset Browser"));
    m_tabBar->addTab(tr("Log Console"));

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_assetBrowser);
    m_stack->addWidget(m_logConsole);

    connect(m_tabBar, &QTabBar::currentChanged,
            m_stack, &QStackedWidget::setCurrentIndex);

    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int idx) {
        if (idx == 1)
            QTimer::singleShot(0, this, [this]() {
                m_logConsole->findChild<QTextEdit*>()->ensureCursorVisible();
            });
    });

    auto* leftWidget = new QWidget(this);
    auto* leftVlay   = new QVBoxLayout(leftWidget);
    leftVlay->setContentsMargins(0, 0, 0, 0);
    leftVlay->setSpacing(0);
    leftVlay->addWidget(m_tabBar);
    leftVlay->addWidget(m_stack, 1);
    leftWidget->setLayout(leftVlay);

    auto* previewHeader = new QLabel(tr("Asset Preview"), this);
    previewHeader->setObjectName("previewHeader");
    previewHeader->setFixedHeight(24);
    previewHeader->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    previewHeader->setContentsMargins(8, 0, 0, 0);

    auto* rightWidget = new QWidget(this);
    auto* rightVlay   = new QVBoxLayout(rightWidget);
    rightVlay->setContentsMargins(0, 0, 0, 0);
    rightVlay->setSpacing(0);
    rightVlay->addWidget(previewHeader);
    rightVlay->addWidget(m_assetPreview, 1);
    rightWidget->setLayout(rightVlay);
    rightWidget->setFixedWidth(260);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(1);
    m_splitter->addWidget(leftWidget);
    m_splitter->addWidget(rightWidget);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);

    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);
    mainLay->addWidget(m_splitter, 1);
    setLayout(mainLay);

    connect(m_assetBrowser, &AssetBrowserPanel::assetSelected,
            m_assetPreview, &AssetPreviewPanel::previewAsset);
    connect(m_assetBrowser, &AssetBrowserPanel::logMessage,
            m_logConsole,   &LogConsolePanel::appendMessage);

    m_assetBrowser->initLog();
    applyStyle();
}

void BottomPanel::applyStyle()
{
    setStyleSheet(R"(
        BottomPanel {
            background: #252526;
            border-top: 1px solid #1b1b1c;
        }
        QTabBar#bottomTabBar {
            background: #2d2d2d;
        }
        QTabBar#bottomTabBar::tab {
            background: #2d2d2d;
            color: #888888;
            border: none;
            border-right: 1px solid #1b1b1c;
            padding: 0 14px;
            height: 26px;
            font-size: 12px;
        }
        QTabBar#bottomTabBar::tab:selected {
            background: #1e1e1e;
            color: #cccccc;
            border-top: 2px solid #0078d4;
        }
        QTabBar#bottomTabBar::tab:hover:!selected {
            background: #3f3f46;
            color: #cccccc;
        }
        QLabel#previewHeader {
            background: #2d2d2d;
            color: #cccccc;
            font-size: 12px;
            border-bottom: 1px solid #1b1b1c;
        }
        QSplitter::handle {
            background: #1b1b1c;
        }
    )");
}
