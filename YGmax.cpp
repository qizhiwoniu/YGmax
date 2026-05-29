#include "stdafx.h"
#include "YGmax.h"
#include "ui/AboutDialog.h"
#include "version.h"
#include "ui/ToolBar.h"
#include "ui/DocumentTabBar.h"
#include "ui/BottomPanel.h"
#include "ui/RightPanel.h"
#include "Viewport3D.h"
#include "SettingsDialog.h"
#include "SceneRunnerWidget.h"
#include "SceneSerializer.h"
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QFileInfo>

// ═══════════════════════════════════════════════════════════════
//  构造函数
// ═══════════════════════════════════════════════════════════════
YGmax::YGmax(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(1280, 720);

    m_titleBar = new CustomTitleBar(this);
    m_titleBar->setIcon(QPixmap());

    m_toolBar = new ToolBar(this);
    m_tabBar  = new DocumentTabBar(this);

    setupMenuBar();
    setupTray();
    setupDockHost();

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_titleBar);
    mainLayout->addWidget(m_toolBar);
    mainLayout->addWidget(m_tabBar);
    mainLayout->addWidget(m_dockHost, 1);
    setLayout(mainLayout);

    // ── 窗口控制 ─────────────────────────────────────────────
    connect(m_titleBar, &CustomTitleBar::minimizeClicked, this, &QWidget::showMinimized);
    connect(m_titleBar, &CustomTitleBar::closeClicked,    this, &QWidget::close);
    connect(m_titleBar, &CustomTitleBar::maximizeClicked, this, [this] {
        isMaximized() ? showNormal() : showMaximized();
    });

    // ── 工具栏 ───────────────────────────────────────────────
    connect(m_toolBar, &ToolBar::selectModeActivated,  this, []() {});
    connect(m_toolBar, &ToolBar::defaultModeActivated, this, []() {});
    connect(m_toolBar, &ToolBar::actionTriggered, this, [this](int id) {

        // ── ID 3：独立窗口运行场景 ──────────────────────────
        if (id == 3) {
            auto* win = new QWidget(nullptr,
                Qt::Window | Qt::WindowCloseButtonHint);
            win->setWindowTitle(tr("场景预览"));
            win->resize(960, 600);
            win->setAttribute(Qt::WA_DeleteOnClose);

            auto* runner = new SceneRunnerWidget(
                m_viewport->sceneObjects(), win);

            auto* lay = new QVBoxLayout(win);
            lay->setContentsMargins(0, 0, 0, 0);
            lay->addWidget(runner);
            win->setLayout(lay);

            connect(win, &QWidget::destroyed, runner,
                [runner]() { runner->stop(); });

            win->show();
        }

        // ── ID 4：内嵌运行场景 ──────────────────────────────
        if (id == 4) {
            if (m_inlineRunner) {
                m_inlineRunner->stop();
                m_stack->removeWidget(m_inlineRunner);
                delete m_inlineRunner;
                m_inlineRunner = nullptr;
                m_stack->setCurrentWidget(m_viewport);
                return;
            }
            m_inlineRunner = new SceneRunnerWidget(
                m_viewport->sceneObjects(), m_stack);
            m_stack->addWidget(m_inlineRunner);
            m_stack->setCurrentWidget(m_inlineRunner);
            m_inlineRunner->setFocus();
        }

        // ── ID 5：手柄映射设置 ──────────────────────────────
        if (id == 5) {
            SettingsDialog dlg(this);
            if (auto* tabs = dlg.findChild<QTabWidget*>())
                tabs->setCurrentIndex(3);
            dlg.exec();
        }

        // ── ID 8：💾 快速保存 ────────────────────────────────
        if (id == 8) {
            quickSave();
        }

        // ── ID 9：撤销 ──────────────────────────────────────
        if (id == 9) {
            m_viewport->undo();
        }
    });

    // ── 文档标签 ─────────────────────────────────────────────
    connect(m_tabBar, &DocumentTabBar::newTabRequested,
            m_newDocAct, &QAction::trigger);
    connect(m_tabBar, &DocumentTabBar::tabCloseRequested, this, [this](int index) {
        m_tabBar->removeTab(index);
        if (m_tabBar->count() == 0) {
            m_tabBar->setFixedHeight(0);
            m_stack->setVisible(false);
        }
    });
    connect(m_tabBar, &DocumentTabBar::tabChanged, this, [this](int index) {
        if (index < 0) return;
        int kind = m_tabBar->tabData(index);
        m_stack->setCurrentWidget(kind == 1 ? (QWidget*)m_textEditor
                                            : (QWidget*)m_viewport);
    });

    // ── Explorer 选中 → Property Editor ──────────────────────
    connect(m_explorerPanel, &ExplorerPanel::nodeSelected,
            m_propertyPanel, &PropertyEditorPanel::inspectNode);

    // ── Viewport → Explorer 同步 ─────────────────────────────
    connect(m_viewport, &Viewport3D::objectAdded,
            m_explorerPanel, &ExplorerPanel::addNode);
    connect(m_viewport, &Viewport3D::objectRemoved,
            m_explorerPanel, &ExplorerPanel::removeNode);
    connect(m_viewport, &Viewport3D::sceneCleared,
            m_explorerPanel, &ExplorerPanel::clearNodes);

    // Explorer 右键删除 → Viewport
    connect(m_explorerPanel, &ExplorerPanel::deleteRequested,
            m_viewport, &Viewport3D::deleteObjectByName);

    // Viewport 点击选中 → Property Editor
    connect(m_viewport, &Viewport3D::objectSelected,
            m_propertyPanel, &PropertyEditorPanel::inspectNode);

    // ── CreatePanel 点击 → Viewport ──────────────────────────
    connect(m_createPanel, &CreatePanel::createPrimitive,
            this, [this](const QString& type) {
        static int n = 1;
        QString name = type + "_" + QString::number(n++);
        if      (type == "Box")       m_viewport->addBox     (name);
        else if (type == "Sphere")    m_viewport->addSphere  (name);
        else if (type == "Cylinder")  m_viewport->addCylinder(name);
        else if (type == "Cone")      m_viewport->addCone    (name);
        else if (type == "Plane")     m_viewport->addPlane   (name);
        else                          m_viewport->addLight   (name);
    });

    // ── Ctrl+S 全局快捷键（ApplicationShortcut 保证任意焦点下触发）
    auto* saveShortcut = new QShortcut(QKeySequence::Save, this);
    saveShortcut->setContext(Qt::ApplicationShortcut);
    connect(saveShortcut, &QShortcut::activated, this, &YGmax::quickSave);

    restoreLayout();
    m_newDocAct->trigger();

    qApp->installEventFilter(this);
}

YGmax::~YGmax() {}

// ═══════════════════════════════════════════════════════════════
//  文件操作
// ═══════════════════════════════════════════════════════════════

// ── 辅助：当前 Tab 是否为 Lua 脚本 ─────────────────────────
static bool tabIsLua(DocumentTabBar* bar)
{
    int idx = bar->currentIndex();
    return idx >= 0 && bar->tabData(idx) == 1;
}

// ── Ctrl+S 统一入口 ──────────────────────────────────────────
void YGmax::quickSave()
{
    if (tabIsLua(m_tabBar))
        saveLuaScript(false);
    else
        saveScene(false);
}

// ── 保存场景 (.ygx) ──────────────────────────────────────────
void YGmax::saveScene(bool saveAs)
{
    QString path = m_currentScenePath;

    if (saveAs || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            this,
            tr("保存场景"),
            path.isEmpty() ? tr("未命名场景") : path,
            tr("YGmax 场景 (*.ygx);;所有文件 (*.*)"));
    }
    if (path.isEmpty()) return;

    if (!path.endsWith(".ygx", Qt::CaseInsensitive))
        path += ".ygx";

    SceneSnapshot snap = m_viewport->takeSnapshot();
    SerializeError err = SceneSerializer::saveScene(path, snap);

    if (err != SerializeError::None) {
        QMessageBox::critical(this, tr("保存失败"),
            tr("无法保存场景：\n%1\n\n%2")
                .arg(path)
                .arg(SceneSerializer::errorString(err)));
        return;
    }

    m_currentScenePath = path;

    // 同步标签页标题
    int tabIdx = m_tabBar->currentIndex();
    if (tabIdx >= 0)
        m_tabBar->setTabText(tabIdx, QFileInfo(path).baseName());

    m_logPanel->appendMessage(
        tr("[场景] 已保存：%1  (%2 个对象)")
            .arg(path).arg(snap.objects.size()));
}

// ── 打开场景 (.ygx) ──────────────────────────────────────────
void YGmax::openScene()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("打开场景"),
        QString(),
        tr("YGmax 场景 (*.ygx);;所有文件 (*.*)"));
    if (path.isEmpty()) return;

    SceneSnapshot snap;
    SerializeError err = SceneSerializer::loadScene(path, snap);

    if (err != SerializeError::None) {
        QMessageBox::critical(this, tr("打开失败"),
            tr("无法读取场景文件：\n%1\n\n%2")
                .arg(path)
                .arg(SceneSerializer::errorString(err)));
        return;
    }

    // 找到或新建 3D 视口 Tab
    int sceneTab = -1;
    for (int i = 0; i < m_tabBar->count(); ++i) {
        if (m_tabBar->tabData(i) == 0) { sceneTab = i; break; }
    }
    if (sceneTab < 0) {
        m_tabBar->setFixedHeight(34);
        sceneTab = m_tabBar->addTab("");
        m_tabBar->setTabData(sceneTab, 0);
        m_tabBar->show();
    }
    m_tabBar->setCurrentIndex(sceneTab);
    m_stack->setVisible(true);
    m_stack->setCurrentWidget(m_viewport);

    // 加载快照（clearScene + 重建 GPU 资源 + 通知 Explorer）
    m_viewport->loadSnapshot(snap);

    m_currentScenePath = path;
    m_tabBar->setTabText(sceneTab, QFileInfo(path).baseName());

    m_logPanel->appendMessage(
        tr("[场景] 已打开：%1  (%2 个对象)")
            .arg(path).arg(snap.objects.size()));
}

// ── 保存 Lua 脚本 (.lvl) ─────────────────────────────────────
void YGmax::saveLuaScript(bool saveAs)
{
    QString path = m_currentLuaPath;

    if (saveAs || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            this,
            tr("保存 Lua 脚本"),
            path.isEmpty() ? tr("未命名脚本") : path,
            tr("YGmax Lua 脚本 (*.lvl);;所有文件 (*.*)"));
    }
    if (path.isEmpty()) return;

    if (!path.endsWith(".lvl", Qt::CaseInsensitive))
        path += ".lvl";

    SerializeError err = SceneSerializer::saveLua(
        path, m_textEditor->toPlainText());

    if (err != SerializeError::None) {
        QMessageBox::critical(this, tr("保存失败"),
            tr("无法保存 Lua 脚本：\n%1\n\n%2")
                .arg(path)
                .arg(SceneSerializer::errorString(err)));
        return;
    }

    m_currentLuaPath = path;

    int tabIdx = m_tabBar->currentIndex();
    if (tabIdx >= 0)
        m_tabBar->setTabText(tabIdx, QFileInfo(path).baseName() + ".lvl");

    m_logPanel->appendMessage(tr("[Lua] 已保存：%1").arg(path));
}

// ── 打开 Lua 脚本 (.lvl) ─────────────────────────────────────
void YGmax::openLuaScript()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("打开 Lua 脚本"),
        QString(),
        tr("YGmax Lua 脚本 (*.lvl);;所有文件 (*.*)"));
    if (path.isEmpty()) return;

    QString src;
    SerializeError err = SceneSerializer::loadLua(path, src);

    if (err != SerializeError::None) {
        QMessageBox::critical(this, tr("打开失败"),
            tr("无法读取 Lua 脚本：\n%1\n\n%2")
                .arg(path)
                .arg(SceneSerializer::errorString(err)));
        return;
    }

    // 新建 Lua Tab，加载内容
    m_tabBar->setFixedHeight(34);
    QFileInfo fi(path);
    int idx = m_tabBar->addTab(fi.baseName() + ".lvl");
    m_tabBar->setTabData(idx, 1);
    m_tabBar->setCurrentIndex(idx);
    m_tabBar->show();

    m_stack->setVisible(true);
    m_stack->setCurrentWidget(m_textEditor);
    m_textEditor->setPlainText(src);

    m_currentLuaPath = path;

    m_logPanel->appendMessage(tr("[Lua] 已打开：%1").arg(path));
}

// ── 统一打开对话框（按扩展名分发）────────────────────────────
void YGmax::openFile()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("打开文件"),
        QString(),
        tr("所有支持的文件 (*.ygx *.lvl);;"
           "YGmax 场景 (*.ygx);;"
           "Lua 脚本 (*.lvl);;"
           "所有文件 (*.*)"));
    if (path.isEmpty()) return;

    if (path.endsWith(".lvl", Qt::CaseInsensitive))
        openLuaScript();   // 实际加载路径在函数内由对话框提供
    else
        openScene();

    // 注意：上面的函数内部会再次弹出对话框，因此这里改为直接调用
    // 带路径参数的内部版本，见下方私有辅助函数的说明。
}

// ═══════════════════════════════════════════════════════════════
//  内嵌 QMainWindow + 所有 QDockWidget
// ═══════════════════════════════════════════════════════════════
void YGmax::setupDockHost()
{
    m_dockHost = new QMainWindow(this);
    m_dockHost->setWindowFlags(Qt::Widget);
    m_dockHost->setDockOptions(
        QMainWindow::AnimatedDocks    |
        QMainWindow::AllowNestedDocks |
        QMainWindow::AllowTabbedDocks
    );

    // ── 中央：QStackedWidget（Viewport3D / 文本编辑器）────────
    m_viewport = new Viewport3D(m_dockHost);
    m_viewport->setMinimumSize(300, 200);
    m_viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_textEditor = new QPlainTextEdit(m_dockHost);
    m_textEditor->setPlaceholderText(tr("在此编写 Lua 脚本..."));
    m_textEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // Lua 脚本编辑器等宽字体
    m_textEditor->setFont(QFont("Consolas", 11));

    m_stack = new QStackedWidget(m_dockHost);
    m_stack->addWidget(m_viewport);    // index 0 → 3D 视口
    m_stack->addWidget(m_textEditor);  // index 1 → Lua 编辑器
    m_stack->setCurrentIndex(0);

    m_dockHost->setCentralWidget(m_stack);

    // ── 底部面板 ──────────────────────────────────────────────
    m_assetPanel   = new AssetBrowserPanel(m_dockHost);
    m_logPanel     = new LogConsolePanel(m_dockHost);
    m_previewPanel = new AssetPreviewPanel(m_dockHost);
    connect(m_assetPanel, &AssetBrowserPanel::assetSelected,
            m_previewPanel, &AssetPreviewPanel::previewAsset);

    // ── 右侧面板 ──────────────────────────────────────────────
    m_explorerPanel = new ExplorerPanel(m_dockHost);
    m_createPanel   = new CreatePanel(m_dockHost);
    m_propertyPanel = new PropertyEditorPanel(m_dockHost);
    m_propertyPanel->bindViewport(m_viewport);

    // ── 底部 Dock ─────────────────────────────────────────────
    m_dockAsset = new QDockWidget(tr("Asset Browser"), m_dockHost);
    m_dockAsset->setObjectName("dockAssetBrowser");
    m_dockAsset->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dockAsset->setFeatures(QDockWidget::DockWidgetMovable   |
                              QDockWidget::DockWidgetFloatable |
                              QDockWidget::DockWidgetClosable);
    m_dockAsset->setWidget(m_assetPanel);
    m_dockHost->addDockWidget(Qt::BottomDockWidgetArea, m_dockAsset);

    m_dockLog = new QDockWidget(tr("Log Console"), m_dockHost);
    m_dockLog->setObjectName("dockLogConsole");
    m_dockLog->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dockLog->setFeatures(QDockWidget::DockWidgetMovable   |
                            QDockWidget::DockWidgetFloatable |
                            QDockWidget::DockWidgetClosable);
    m_dockLog->setWidget(m_logPanel);
    m_dockHost->addDockWidget(Qt::BottomDockWidgetArea, m_dockLog);
    m_dockHost->tabifyDockWidget(m_dockAsset, m_dockLog);
    m_dockAsset->raise();

    m_dockPreview = new QDockWidget(tr("Asset Preview"), m_dockHost);
    m_dockPreview->setObjectName("dockAssetPreview");
    m_dockPreview->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dockPreview->setFeatures(QDockWidget::DockWidgetMovable   |
                                QDockWidget::DockWidgetFloatable |
                                QDockWidget::DockWidgetClosable);
    m_dockPreview->setWidget(m_previewPanel);
    m_dockHost->addDockWidget(Qt::BottomDockWidgetArea, m_dockPreview);

    // ── 右侧 Dock ─────────────────────────────────────────────
    m_dockExplorer = new QDockWidget(tr("Explorer"), m_dockHost);
    m_dockExplorer->setObjectName("dockExplorer");
    m_dockExplorer->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dockExplorer->setFeatures(QDockWidget::DockWidgetMovable   |
                                 QDockWidget::DockWidgetFloatable |
                                 QDockWidget::DockWidgetClosable);
    m_dockExplorer->setWidget(m_explorerPanel);
    m_dockHost->addDockWidget(Qt::RightDockWidgetArea, m_dockExplorer);

    m_dockCreate = new QDockWidget(tr("Create"), m_dockHost);
    m_dockCreate->setObjectName("dockCreate");
    m_dockCreate->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dockCreate->setFeatures(QDockWidget::DockWidgetMovable   |
                               QDockWidget::DockWidgetFloatable |
                               QDockWidget::DockWidgetClosable);
    m_dockCreate->setWidget(m_createPanel);
    m_dockHost->addDockWidget(Qt::RightDockWidgetArea, m_dockCreate);
    m_dockHost->tabifyDockWidget(m_dockExplorer, m_dockCreate);
    m_dockExplorer->raise();

    m_dockProperty = new QDockWidget(tr("Property Editor"), m_dockHost);
    m_dockProperty->setObjectName("dockPropertyEditor");
    m_dockProperty->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dockProperty->setFeatures(QDockWidget::DockWidgetMovable   |
                                 QDockWidget::DockWidgetFloatable |
                                 QDockWidget::DockWidgetClosable);
    m_dockProperty->setWidget(m_propertyPanel);
    m_dockHost->addDockWidget(Qt::RightDockWidgetArea, m_dockProperty);
    m_dockHost->splitDockWidget(m_dockExplorer, m_dockProperty, Qt::Vertical);

    applyDockStyle();
}

// ═══════════════════════════════════════════════════════════════
//  Dock 样式
// ═══════════════════════════════════════════════════════════════
void YGmax::applyDockStyle()
{
    const QString dockQss = R"(
        QMainWindow::separator {
            background: #2d2d2d; width: 3px; height: 3px;
        }
        QDockWidget {
            color: #cccccc;
            titlebar-close-icon: none;
            titlebar-normal-icon: none;
        }
        QDockWidget::title {
            background: #2d2d2d;
            padding-left: 8px;
            padding-top: 3px;
            border-bottom: 1px solid #3f3f46;
        }
        QTabBar::tab {
            background: #2d2d2d; color: #888888;
            padding: 4px 12px;
            border: 1px solid #3f3f46;
            border-bottom: none;
        }
        QTabBar::tab:selected { background: #1e1e1e; color: #cccccc; }
        QTabBar::tab:hover:!selected { background: #3f3f46; }
    )";
    m_dockHost->setStyleSheet(dockQss);
}

// ═══════════════════════════════════════════════════════════════
//  菜单栏
// ═══════════════════════════════════════════════════════════════
void YGmax::setupMenuBar()
{
    QMenuBar* mb = m_titleBar->menuBar();

    // ── 文件菜单 ─────────────────────────────────────────────
    QMenu* fileMenu = mb->addMenu(tr("文件(&F)"));

    // 新建场景
    m_newDocAct = new QAction(tr("新建场景(&N)"), this);
    m_newDocAct->setShortcut(QKeySequence::New);
    connect(m_newDocAct, &QAction::triggered, this, [this]() {
        m_tabBar->setFixedHeight(34);
        int idx = m_tabBar->addTab(tr("未命名场景"));
        m_tabBar->setTabData(idx, 0);
        m_tabBar->show();
        m_stack->setVisible(true);
        m_stack->setCurrentWidget(m_viewport);
        m_currentScenePath.clear();
    });
    fileMenu->addAction(m_newDocAct);

    // 新建 Lua 脚本
    m_newLua = new QAction(tr("新建 Lua 脚本"), this);
    connect(m_newLua, &QAction::triggered, this, [this]() {
        m_tabBar->setFixedHeight(34);
        int idx = m_tabBar->addTab(tr("未命名脚本.lvl"));
        m_tabBar->setTabData(idx, 1);
        m_tabBar->show();
        m_stack->setVisible(true);
        m_stack->setCurrentWidget(m_textEditor);
        m_textEditor->clear();
        m_currentLuaPath.clear();
    });
    fileMenu->addAction(m_newLua);

    fileMenu->addSeparator();

    // 打开（统一入口，支持 .ygx 和 .lvl）
    QAction* openAct = new QAction(tr("打开(&O)..."), this);
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, [this]() {
        // 一次性对话框，按后缀名分发
        QString path = QFileDialog::getOpenFileName(
            this,
            tr("打开文件"),
            QString(),
            tr("所有支持的文件 (*.ygx *.lvl);;"
               "YGmax 场景 (*.ygx);;"
               "Lua 脚本 (*.lvl);;"
               "所有文件 (*.*)"));
        if (path.isEmpty()) return;

        if (path.endsWith(".lvl", Qt::CaseInsensitive)) {
            // ── 直接加载 Lua 脚本 ─────────────────────────
            QString src;
            SerializeError err = SceneSerializer::loadLua(path, src);
            if (err != SerializeError::None) {
                QMessageBox::critical(this, tr("打开失败"),
                    tr("无法读取 Lua 脚本：\n%1\n\n%2")
                        .arg(path).arg(SceneSerializer::errorString(err)));
                return;
            }
            m_tabBar->setFixedHeight(34);
            QFileInfo fi(path);
            int idx = m_tabBar->addTab(fi.baseName() + ".lvl");
            m_tabBar->setTabData(idx, 1);
            m_tabBar->setCurrentIndex(idx);
            m_tabBar->show();
            m_stack->setVisible(true);
            m_stack->setCurrentWidget(m_textEditor);
            m_textEditor->setPlainText(src);
            m_currentLuaPath = path;
            m_logPanel->appendMessage(tr("[Lua] 已打开：%1").arg(path));

        } else {
            // ── 直接加载场景 ──────────────────────────────
            SceneSnapshot snap;
            SerializeError err = SceneSerializer::loadScene(path, snap);
            if (err != SerializeError::None) {
                QMessageBox::critical(this, tr("打开失败"),
                    tr("无法读取场景文件：\n%1\n\n%2")
                        .arg(path).arg(SceneSerializer::errorString(err)));
                return;
            }
            // 找到或新建 3D 视口 Tab
            int sceneTab = -1;
            for (int i = 0; i < m_tabBar->count(); ++i) {
                if (m_tabBar->tabData(i) == 0) { sceneTab = i; break; }
            }
            if (sceneTab < 0) {
                m_tabBar->setFixedHeight(34);
                sceneTab = m_tabBar->addTab("");
                m_tabBar->setTabData(sceneTab, 0);
                m_tabBar->show();
            }
            m_tabBar->setCurrentIndex(sceneTab);
            m_stack->setVisible(true);
            m_stack->setCurrentWidget(m_viewport);
            m_viewport->loadSnapshot(snap);
            m_currentScenePath = path;
            m_tabBar->setTabText(sceneTab, QFileInfo(path).baseName());
            m_logPanel->appendMessage(
                tr("[场景] 已打开：%1  (%2 个对象)")
                    .arg(path).arg(snap.objects.size()));
        }
    });
    fileMenu->addAction(openAct);

    fileMenu->addSeparator();

    // 保存（Ctrl+S）
    QAction* saveAct = new QAction(tr("保存(&S)"), this);
    saveAct->setShortcut(QKeySequence::Save);
    connect(saveAct, &QAction::triggered, this, &YGmax::quickSave);
    fileMenu->addAction(saveAct);

    // 另存为（Ctrl+Shift+S）
    QAction* saveAsAct = new QAction(tr("另存为(&A)..."), this);
    saveAsAct->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAct, &QAction::triggered, this, [this]() {
        if (tabIsLua(m_tabBar))
            saveLuaScript(true);
        else
            saveScene(true);
    });
    fileMenu->addAction(saveAsAct);

    fileMenu->addSeparator();

    QAction* quitAct = new QAction(tr("退出(&Q)"), this);
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAct);

    // ── 编辑菜单 ─────────────────────────────────────────────
    QMenu* editMenu = mb->addMenu(tr("编辑(&E)"));
    QAction* undoAct = new QAction(tr("撤销(&Z)"), this);
    undoAct->setShortcut(QKeySequence::Undo);
    connect(undoAct, &QAction::triggered, this, [this]() {
        m_viewport->undo();
    });
    editMenu->addAction(undoAct);

    // ── 视图菜单 ─────────────────────────────────────────────
    QMenu* viewMenu = mb->addMenu(tr("视图(&V)"));

    auto makeViewAct = [&](const QString& label, QDockWidget* YGmax::* member) {
        QAction* act = new QAction(label, this);
        act->setCheckable(true);
        act->setChecked(true);
        connect(act, &QAction::toggled, this, [this, member](bool v) {
            if (QDockWidget* dock = this->*member) dock->setVisible(v);
        });
        connect(viewMenu, &QMenu::aboutToShow, this, [this, act, member]() {
            if (QDockWidget* dock = this->*member) act->setChecked(dock->isVisible());
        });
        viewMenu->addAction(act);
    };

    makeViewAct(tr("Asset Browser"),   &YGmax::m_dockAsset);
    makeViewAct(tr("Log Console"),     &YGmax::m_dockLog);
    makeViewAct(tr("Asset Preview"),   &YGmax::m_dockPreview);
    viewMenu->addSeparator();
    makeViewAct(tr("Explorer"),        &YGmax::m_dockExplorer);
    makeViewAct(tr("Create"),          &YGmax::m_dockCreate);
    makeViewAct(tr("Property Editor"), &YGmax::m_dockProperty);

    // ── 帮助菜单 ─────────────────────────────────────────────
    QMenu* helpMenu = mb->addMenu(tr("帮助(&H)"));
    QAction* aboutAct = new QAction(tr("关于"), this);
    connect(aboutAct, &QAction::triggered, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });
    helpMenu->addAction(aboutAct);
}

// ═══════════════════════════════════════════════════════════════
//  系统托盘
// ═══════════════════════════════════════════════════════════════
void YGmax::setupTray()
{
    m_tray = new SystemTrayIcon(this, this);

    connect(m_tray, &SystemTrayIcon::showMainWindow, this, [this]() {
        showNormal();
        activateWindow();
        raise();
    });
    connect(m_tray, &SystemTrayIcon::checkUpdate, this, [this]() {
        m_tray->showMessage(
            QString(VER_PRODUCT_NAME),
            tr("当前已是最新版本  v%1").arg(VERSION_STR),
            QSystemTrayIcon::Information,
            3000);
    });

    m_tray->show();
}

// ═══════════════════════════════════════════════════════════════
//  布局保存 / 恢复
// ═══════════════════════════════════════════════════════════════
void YGmax::saveLayout()
{
    QSettings s("Qizhiwoniu", "YGmax");
    s.setValue("geometry",  saveGeometry());
    s.setValue("dockState", m_dockHost->saveState());
}

void YGmax::restoreLayout()
{
    QSettings s("Qizhiwoniu", "YGmax");
    if (s.contains("geometry"))
        restoreGeometry(s.value("geometry").toByteArray());
    if (s.contains("dockState"))
        m_dockHost->restoreState(s.value("dockState").toByteArray());
}

// ═══════════════════════════════════════════════════════════════
//  关闭事件
// ═══════════════════════════════════════════════════════════════
void YGmax::closeEvent(QCloseEvent* event)
{
    saveLayout();
    event->ignore();
    hide();
    m_tray->showMessage(
        VER_PRODUCT_NAME,
        tr("程序已最小化到托盘，双击图标可重新打开"),
        QSystemTrayIcon::Information,
        2000);
}

// ═══════════════════════════════════════════════════════════════
//  绘制 / 缩放
// ═══════════════════════════════════════════════════════════════
void YGmax::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rect(), 10, 10);
    painter.fillPath(path, QColor("#1b1b1c"));
}

void YGmax::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event)
    QPainterPath path;
    path.addRoundedRect(rect(), 10, 10);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}

// ═══════════════════════════════════════════════════════════════
//  全局事件过滤器（无边框窗口缩放）
// ═══════════════════════════════════════════════════════════════
bool YGmax::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched)
    if (!isVisible()) return false;
    if (event->type() != QEvent::MouseMove       &&
        event->type() != QEvent::MouseButtonPress &&
        event->type() != QEvent::MouseButtonRelease)
        return false;

    auto* me = static_cast<QMouseEvent*>(event);
    const QPoint localPos = mapFromGlobal(me->globalPosition().toPoint());

    if (event->type() == QEvent::MouseMove) {
        if (m_resizing) {
            const QPoint delta = me->globalPosition().toPoint() - m_resizeStart;
            QRect g = m_resizeOrigGeom;
            const int minW = 400, minH = 300;
            if (m_resizeDir & Left)   { int nl = g.left()  + delta.x(); if (g.right()  - nl >= minW) g.setLeft(nl); }
            if (m_resizeDir & Right)  { int nw = g.width() + delta.x(); if (nw >= minW) g.setWidth(nw); }
            if (m_resizeDir & Top)    { int nt = g.top()   + delta.y(); if (g.bottom() - nt >= minH) g.setTop(nt); }
            if (m_resizeDir & Bottom) { int nh = g.height()+ delta.y(); if (nh >= minH) g.setHeight(nh); }
            setGeometry(g);
            return true;
        }
        updateCursor(hitTest(localPos));
        return false;
    }
    if (event->type() == QEvent::MouseButtonPress && me->button() == Qt::LeftButton) {
        ResizeDir dir = hitTest(localPos);
        if (dir != None) {
            m_resizing = true; m_resizeDir = dir;
            m_resizeStart = me->globalPosition().toPoint();
            m_resizeOrigGeom = geometry();
            return true;
        }
        return false;
    }
    if (event->type() == QEvent::MouseButtonRelease &&
        me->button() == Qt::LeftButton && m_resizing) {
        m_resizing = false; m_resizeDir = None; unsetCursor();
        return true;
    }
    return false;
}

YGmax::ResizeDir YGmax::hitTest(const QPoint& pos) const
{
    const int x = pos.x(), y = pos.y(), w = width(), h = height();
    if (w == 0 || h == 0) return None;
    int dir = None;
    if (x <= kEdge)       dir |= Left;
    if (x >= w - kEdge)   dir |= Right;
    if (y <= kEdge)       dir |= Top;
    if (y >= h - kEdge)   dir |= Bottom;
    return static_cast<ResizeDir>(dir);
}

void YGmax::updateCursor(ResizeDir dir)
{
    switch (dir) {
    case Left:  case Right:          setCursor(Qt::SizeHorCursor);   break;
    case Top:   case Bottom:         setCursor(Qt::SizeVerCursor);   break;
    case TopLeft: case BottomRight:  setCursor(Qt::SizeFDiagCursor); break;
    case TopRight: case BottomLeft:  setCursor(Qt::SizeBDiagCursor); break;
    default:                         unsetCursor();                  break;
    }
}

void YGmax::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        ResizeDir dir = hitTest(event->pos());
        if (dir != None) {
            m_resizing = true; m_resizeDir = dir;
            m_resizeStart = event->globalPosition().toPoint();
            m_resizeOrigGeom = geometry();
            event->accept(); return;
        }
    }
    QWidget::mousePressEvent(event);
}

void YGmax::mouseMoveEvent(QMouseEvent* event)
{
    if (m_resizing) {
        const QPoint delta = event->globalPosition().toPoint() - m_resizeStart;
        QRect g = m_resizeOrigGeom;
        const int minW = minimumWidth()  > 0 ? minimumWidth()  : 400;
        const int minH = minimumHeight() > 0 ? minimumHeight() : 300;
        if (m_resizeDir & Left)   { int nl = g.left()  + delta.x(); if (g.right()  - nl >= minW) g.setLeft(nl); }
        if (m_resizeDir & Right)  { int nw = g.width() + delta.x(); if (nw >= minW) g.setWidth(nw); }
        if (m_resizeDir & Top)    { int nt = g.top()   + delta.y(); if (g.bottom() - nt >= minH) g.setTop(nt); }
        if (m_resizeDir & Bottom) { int nh = g.height()+ delta.y(); if (nh >= minH) g.setHeight(nh); }
        setGeometry(g);
        event->accept(); return;
    }
    updateCursor(hitTest(event->pos()));
    QWidget::mouseMoveEvent(event);
}

void YGmax::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_resizing && event->button() == Qt::LeftButton) {
        m_resizing = false; m_resizeDir = None; unsetCursor();
        event->accept(); return;
    }
    QWidget::mouseReleaseEvent(event);
}
