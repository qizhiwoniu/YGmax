#include "stdafx.h"
#include "YGmax.h"
#include "AboutDialog.h"
#include "version.h"
#include "ToolBar.h"
#include "DocumentTabBar.h"
#include "BottomPanel.h"
#include "RightPanel.h"
#include "Viewport3D.h"
#include <QCloseEvent>

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
    connect(m_toolBar, &ToolBar::actionTriggered,      this, [](int id) { Q_UNUSED(id) });

    // ── 文档标签 ─────────────────────────────────────────────
    connect(m_tabBar, &DocumentTabBar::newTabRequested,
            m_newDocAct, &QAction::trigger);
    connect(m_tabBar, &DocumentTabBar::tabCloseRequested, this, [this](int index) {
        m_tabBar->removeTab(index);
        if (m_tabBar->count() == 0) {
            m_tabBar->setFixedHeight(0);
            m_viewport->setVisible(false);
        }
    });
    connect(m_tabBar, &DocumentTabBar::tabChanged, this, [this](int index) {
        Q_UNUSED(index)
    });

    // ── Explorer 选中 → Property Editor ──────────────────────
    connect(m_explorerPanel, &ExplorerPanel::nodeSelected,
            m_propertyPanel, &PropertyEditorPanel::inspectNode);

    // ── Viewport → Explorer 同步 ─────────────────────────────
    // objectAdded 现在带 ObjectKind，Explorer 按 kind 分组
    connect(m_viewport, &Viewport3D::objectAdded,
            m_explorerPanel, &ExplorerPanel::addNode);

    connect(m_viewport, &Viewport3D::objectRemoved,
            m_explorerPanel, &ExplorerPanel::removeNode);

    connect(m_viewport, &Viewport3D::sceneCleared,
            m_explorerPanel, &ExplorerPanel::clearNodes);

    // Explorer 右键删除 → Viewport
    connect(m_explorerPanel, &ExplorerPanel::deleteRequested,
            m_viewport, &Viewport3D::deleteObjectByName);

    // Viewport 点击选中 → Property Editor（直接从 Viewport 选中时也更新）
    connect(m_viewport, &Viewport3D::objectSelected,
            m_propertyPanel, &PropertyEditorPanel::inspectNode);

    // ── CreatePanel 点击 → Viewport 在世界原点创建 ──────────
    connect(m_createPanel, &CreatePanel::createPrimitive,
            this, [this](const QString& type) {
        static int n = 1;
        QString name = type + "_" + QString::number(n++);
        if      (type == "Box")       m_viewport->addBox     (name);
        else if (type == "Sphere")    m_viewport->addSphere  (name);
        else if (type == "Cylinder")  m_viewport->addCylinder(name);
        else if (type == "Cone")      m_viewport->addCone    (name);
        else if (type == "Plane")     m_viewport->addPlane   (name);
        else /* lights / camera */    m_viewport->addLight   (name);
    });

    restoreLayout();
    m_newDocAct->trigger();

    qApp->installEventFilter(this);
}

YGmax::~YGmax() {}

// ─────────────────────────────────────────────────────────────
//  内嵌 QMainWindow + 所有 QDockWidget
// ─────────────────────────────────────────────────────────────
void YGmax::setupDockHost()
{
    m_dockHost = new QMainWindow(this);
    m_dockHost->setWindowFlags(Qt::Widget);
    m_dockHost->setDockOptions(
        QMainWindow::AnimatedDocks    |
        QMainWindow::AllowNestedDocks |
        QMainWindow::AllowTabbedDocks
    );

    // ── 中央：3-D Viewport ────────────────────────────────────
    m_viewport = new Viewport3D(m_dockHost);
    m_viewport->setMinimumSize(300, 200);
    m_viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_dockHost->setCentralWidget(m_viewport);

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

    // 绑定 Viewport → PropertyEditor 双向数据通道
    m_propertyPanel->bindViewport(m_viewport);

    // ════════════════════════════════════════════════════════
    //  底部 Dock
    // ════════════════════════════════════════════════════════
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

    // ════════════════════════════════════════════════════════
    //  右侧 Dock
    // ════════════════════════════════════════════════════════
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
    // Property Editor 放在 Explorer 下方（splitDockWidget）
    m_dockHost->splitDockWidget(m_dockExplorer, m_dockProperty,
                                 Qt::Vertical);

    applyDockStyle();
}

// ─────────────────────────────────────────────────────────────
//  其余成员函数（与原版完全相同，下方直接保留）
// ─────────────────────────────────────────────────────────────
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

void YGmax::saveLayout()
{
    QSettings s("YGmax", "Layout");
    s.setValue("geometry",  saveGeometry());
    s.setValue("dockState", m_dockHost->saveState());
}

void YGmax::restoreLayout()
{
    QSettings s("YGmax", "Layout");
    if (s.contains("geometry"))
        restoreGeometry(s.value("geometry").toByteArray());
    if (s.contains("dockState"))
        m_dockHost->restoreState(s.value("dockState").toByteArray());
}

void YGmax::closeEvent(QCloseEvent* event)
{
    saveLayout();
    event->accept();
}

// ─────────────────────────────────────────────────────────────
//  菜单栏
// ─────────────────────────────────────────────────────────────
void YGmax::setupMenuBar()
{
    QMenuBar* mb = new QMenuBar(m_titleBar);
    mb->setStyleSheet(R"(
        QMenuBar {
            background: transparent; color: #cccccc; font-size: 12px;
        }
        QMenuBar::item { padding: 4px 10px; background: transparent; }
        QMenuBar::item:selected { background: #3f3f46; border-radius: 3px; }
        QMenu {
            background: #2a2a2e; color: #d0d0d0;
            border: 1px solid #444; font-size: 12px;
        }
        QMenu::item { padding: 5px 20px 5px 12px; }
        QMenu::item:selected { background: #0e639c; }
        QMenu::separator { height: 1px; background: #444; margin: 3px 0; }
    )");

    // 文件
    QMenu* fileMenu = mb->addMenu(tr("文件(&F)"));
    m_newDocAct = new QAction(tr("新建文档"), this);
    m_newDocAct->setShortcut(QKeySequence::New);
    connect(m_newDocAct, &QAction::triggered, this, [this] {
        m_tabBar->addTab(tr("未命名文档"));
        m_viewport->setVisible(true);
        if (m_tabBar->height() == 0) m_tabBar->setFixedHeight(-1);
    });
    fileMenu->addAction(m_newDocAct);
    fileMenu->addSeparator();

    QAction* exitAct = new QAction(tr("退出"), this);
    connect(exitAct, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAct);

    // 视图
    QMenu* viewMenu = mb->addMenu(tr("视图(&V)"));
    auto makeViewAct = [&](const QString& title, QDockWidget* YGmax::*member) {
        auto* act = new QAction(title, this);
        act->setCheckable(true);
        act->setChecked(true);
        connect(act, &QAction::toggled, this, [this, member](bool checked) {
            if (QDockWidget* dock = this->*member)
                dock->setVisible(checked);
        });
        connect(viewMenu, &QMenu::aboutToShow, this, [this, act, member]() {
            if (QDockWidget* dock = this->*member)
                act->setChecked(dock->isVisible());
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

    // 帮助
    QMenu* helpMenu = mb->addMenu(tr("帮助(&H)"));
    QAction* aboutAct = new QAction(tr("关于"), this);
    connect(aboutAct, &QAction::triggered, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });
    helpMenu->addAction(aboutAct);
}

// ─────────────────────────────────────────────────────────────
//  系统托盘
// ─────────────────────────────────────────────────────────────
void YGmax::setupTray()
{
    m_tray = new SystemTrayIcon(this);
}

// ─────────────────────────────────────────────────────────────
//  圆角绘制
// ─────────────────────────────────────────────────────────────
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

// ─────────────────────────────────────────────────────────────
//  全局事件过滤器（无边框窗口缩放）
// ─────────────────────────────────────────────────────────────
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
