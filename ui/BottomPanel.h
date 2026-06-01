#pragma once
#include <QWidget>
#include <QSplitter>
#include <QTabBar>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QDrag>
#include <QMimeData>

// ─────────────────────────────────────────────────────────────
//  AssetBrowserPanel   左：文件夹树   右：文件列表
// ─────────────────────────────────────────────────────────────
#include <QDir>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QCoreApplication>
#include <QDateTime>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QPoint>

class AssetBrowserPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AssetBrowserPanel(QWidget* parent = nullptr);

    // 在外部 connect(logMessage) 之后调用，补发初始化日志
    void initLog();

signals:
    void assetSelected(const QString& path);
    // level: 0=info  1=warn  2=error
    void logMessage(const QString& msg, int level = 0);

private slots:
    void onFolderSelected(QTreeWidgetItem* cur, QTreeWidgetItem* prev);
    void onSearchChanged(const QString& text);
    void onFileClicked(QTableWidgetItem* item);
    void onFileDoubleClicked(QTableWidgetItem* item);
    void onFileListContextMenu(const QPoint& pos);
    void onFolderTreeContextMenu(const QPoint& pos);

    void fileRename();
    void fileCopy();
    void fileCut();
    void filePaste();
    void fileDelete();
    void fileNewFolder();

    // ── OBJ 导入 ──────────────────────────────────────────────
    void importObjFile();
    void importFbxFile();
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

    // ── 拖拽支持 ──────────────────────────────────────────────
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;

private:
    void applyStyle();
    void setupFolderTree();
    void setupFileList();

    // 返回 exe 同级的 Media 根目录，不存在则自动创建示例结构
    static QString mediaRoot();
    static void    ensureMediaStructure(const QString& mediaPath);

    // 把 dirPath 下内容填充到 m_fileList（含搜索过滤）
    void populateFileList(const QString& dirPath,
                          const QString& filter = {});

    // 递归构建目录树节点
    QTreeWidgetItem* buildTreeNode(QTreeWidgetItem* parent,
                                   const QString&   dirPath,
                                   bool             expanded = false);

    QSplitter*    m_splitter   = nullptr;
    QTreeWidget*  m_folderTree = nullptr;
    QTableWidget* m_fileList   = nullptr;
    QLineEdit*    m_searchBar  = nullptr;
    QLabel*       m_pathLabel  = nullptr;

    QString       m_currentDir;       // 当前显示的目录路径

    // 剪贴板状态（内部）
    QString       m_clipboardPath;    // 被复制/剪切的文件路径
    bool          m_clipboardIsCut = false;  // true=剪切，false=复制

    // ── 拖拽状态 ──────────────────────────────────────────────
    QPoint        m_dragStartPos;
    bool          m_dragPending = false;

    // 刷新目录树中某个节点
    void refreshTreeNode(const QString& dirPath);
};

// ─────────────────────────────────────────────────────────────
//  LogConsolePanel
// ─────────────────────────────────────────────────────────────
class LogConsolePanel : public QWidget
{
    Q_OBJECT
public:
    explicit LogConsolePanel(QWidget* parent = nullptr);

    // level: 0=info  1=warn  2=error
    void appendMessage(const QString& msg, int level = 0);
    void clear();

private:
    void applyStyle();

    QTextEdit*   m_output   = nullptr;
    QPushButton* m_clearBtn = nullptr;
};

// ─────────────────────────────────────────────────────────────
//  ObjMesh  —  轻量级 OBJ 解析结果（仅位置 + 法线）
// ─────────────────────────────────────────────────────────────
struct ObjMesh {
    std::vector<float>    vertices;  // layout: pos(3) + normal(3) per vertex
    std::vector<uint32_t> indices;
    QString               filePath;
    bool                  valid = false;

    // 归一化包围盒，供预览居中
    QVector3D bboxMin, bboxMax;

    static ObjMesh load(const QString& path);
};

// ─────────────────────────────────────────────────────────────
//  ObjPreviewWidget  —  内嵌 OpenGL 3D 预览（旋转/缩放）
// ─────────────────────────────────────────────────────────────
class ObjPreviewWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit ObjPreviewWidget(QWidget* parent = nullptr);
    ~ObjPreviewWidget() override;

    void loadMesh(const ObjMesh& mesh);
    void clearMesh();

protected:
    void initializeGL()  override;
    void resizeGL(int w, int h) override;
    void paintGL()       override;

    void mousePressEvent  (QMouseEvent* e) override;
    void mouseMoveEvent   (QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent       (QWheelEvent* e) override;

private:
    void buildShader();
    void uploadMesh();

    QOpenGLShaderProgram  m_shader;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer         m_vbo { QOpenGLBuffer::VertexBuffer };
    QOpenGLBuffer         m_ebo { QOpenGLBuffer::IndexBuffer  };

    std::vector<float>    m_verts;
    std::vector<uint32_t> m_idx;
    int                   m_indexCount = 0;
    bool                  m_meshReady  = false;
    bool                  m_gpuReady   = false;   // 等 initializeGL 后才上传

    // 归一化偏移 & 缩放
    QVector3D             m_center;
    float                 m_radius = 1.f;

    // 轨道相机
    float m_yaw   = 30.f;
    float m_pitch = 20.f;
    float m_dist  =  2.5f;
    QPoint    m_lastMouse;
    bool      m_rotating = false;

    // 中键平移（Pan）
    bool      m_panning  = false;
    QVector3D m_panOffset;           // 相机注视点相对于模型中心的平移量

    // 自动旋转
    QTimer* m_autoRotTimer = nullptr;

    // GLSL
    static const char* kVert;
    static const char* kFrag;
};

// ─────────────────────────────────────────────────────────────
//  AssetPreviewPanel  —  图片预览 + OBJ 3D 预览
// ─────────────────────────────────────────────────────────────
class AssetPreviewPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AssetPreviewPanel(QWidget* parent = nullptr);

public slots:
    void previewAsset(const QString& path);

private:
    void applyStyle();

    QLabel*           m_previewLabel = nullptr;
    QLabel*           m_infoLabel    = nullptr;
    QLabel*           m_noSelLabel   = nullptr;
    ObjPreviewWidget* m_objPreview   = nullptr;   // OBJ 3D 预览
};

// ─────────────────────────────────────────────────────────────
//  BottomPanel   顶层容器：左侧 TabBar + 右侧预览
// ─────────────────────────────────────────────────────────────
class BottomPanel : public QWidget
{
    Q_OBJECT
public:
    explicit BottomPanel(QWidget* parent = nullptr);

    LogConsolePanel*   logConsole()   const { return m_logConsole; }
    AssetBrowserPanel* assetBrowser() const { return m_assetBrowser; }

private:
    void applyStyle();

    QTabBar*         m_tabBar      = nullptr;
    QStackedWidget*  m_stack       = nullptr;
    QSplitter*       m_splitter    = nullptr;

    AssetBrowserPanel* m_assetBrowser = nullptr;
    LogConsolePanel*   m_logConsole   = nullptr;
    AssetPreviewPanel* m_assetPreview = nullptr;
};
