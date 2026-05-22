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
    // 去掉系统标题栏，背景透明（圆角需要）
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(1280, 720);

    // ── 自定义标题栏 ──────────────────────────────────────────
    m_titleBar = new CustomTitleBar(this);
    m_titleBar->setIcon(QPixmap());

    // ── 工具栏 ────────────────────────────────────────────────
    m_toolBar = new ToolBar(this);

    // ── 文档标签栏 ────────────────────────────────────────────
    m_tabBar = new DocumentTabBar(this);

    // ── 菜单 / 托盘（要在 connect 之前建好 m_newDocAct）────────
    setupMenuBar();
    setupTray();

    // ── 内嵌 QMainWindow + QDockWidget ────────────────────────
    setupDockHost();

    // ── 主布局 ────────────────────────────────────────────────
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_titleBar);
    mainLayout->addWidget(m_toolBar);
    mainLayout->addWidget(m_tabBar);
    mainLayout->addWidget(m_dockHost, 1);   // dockHost 撑满剩余空间
    setLayout(mainLayout);

    // ── 窗口控制信号 ──────────────────────────────────────────
    connect(m_titleBar, &CustomTitleBar::minimizeClicked, this, &QWidget::showMinimized);
    connect(m_titleBar, &CustomTitleBar::closeClicked,    this, &QWidget::close);
    connect(m_titleBar, &CustomTitleBar::maximizeClicked, this, [this] {
        isMaximized() ? showNormal() : showMaximized();
    });

    // ── 工具栏信号 ────────────────────────────────────────────
    connect(m_toolBar, &ToolBar::selectModeActivated,  this, []() { /* TODO */ });
    connect(m_toolBar, &ToolBar::defaultModeActivated, this, []() { /* TODO */ });
    connect(m_toolBar, &ToolBar::actionTriggered,      this, [](int id) { Q_UNUSED(id) });

    // ── 文档标签信号 ──────────────────────────────────────────
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
        // TODO: 切换对应文档的场景上下文
    });

    // ── Explorer 选中 → Property Editor 联通 ─────────────────
    connect(m_explorerPanel, &ExplorerPanel::nodeSelected,
            m_propertyPanel, &PropertyEditorPanel::inspectNode);

    // ── 恢复上次布局，再触发默认新建文档 ─────────────────────
    restoreLayout();
    m_newDocAct->trigger();

    // ── 全局事件过滤器：捕获子控件上的鼠标移动，实现边缘 resize ──
    qApp->installEventFilter(this);
}

YGmax::~YGmax() {}

// ─────────────────────────────────────────────────────────────
//  内嵌 QMainWindow + 所有 QDockWidget
// ─────────────────────────────────────────────────────────────
void YGmax::setupDockHost()
{
    m_dockHost = new QMainWindow(this);
    m_dockHost->setWindowFlags(Qt::Widget);   // 作为普通子控件嵌入
    m_dockHost->setDockOptions(
        QMainWindow::AnimatedDocks    |
        QMainWindow::AllowNestedDocks |
        QMainWindow::AllowTabbedDocks
    );

    // ── 中央区域：3-D Viewport ────────────────────────────────
    m_viewport = new Viewport3D(m_dockHost);
    m_viewport->setMinimumSize(300, 200);
    m_viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_dockHost->setCentralWidget(m_viewport);

    // ── 创建底部面板内容 ──────────────────────────────────────
    m_assetPanel   = new AssetBrowserPanel(m_dockHost);
    m_logPanel     = new LogConsolePanel(m_dockHost);
    m_previewPanel = new AssetPreviewPanel(m_dockHost);

    connect(m_assetPanel,   &AssetBrowserPanel::assetSelected,
            m_previewPanel, &AssetPreviewPanel::previewAsset);

    // ── 创建右侧面板内容 ──────────────────────────────────────
    m_explorerPanel  = new ExplorerPanel(m_dockHost);
    m_createPanel    = new CreatePanel(m_dockHost);
    m_propertyPanel  = new PropertyEditorPanel(m_dockHost);

    // ════════════════════════════════════════════════════════
    //  底部 Dock：Asset Browser + Log Console（Tab 合并）
    // ════════════════════════════════════════════════════════

    // Dock: Asset Browser
    m_dockAsset = new QDockWidget(tr("Asset Browser"), m_dockHost);
    m_dockAsset->setObjectName("dockAssetBrowser");
    m_dockAsset->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dockAsset->setFeatures(QDockWidget::DockWidgetMovable   |
                              QDockWidget::DockWidgetFloatable |
                              QDockWidget::DockWidgetClosable);
    m_dockAsset->setWidget(m_assetPanel);
    m_dockHost->addDockWidget(Qt::BottomDockWidgetArea, m_dockAsset);

    // Dock: Log Console（与 Asset Browser Tab 合并）
    m_dockLog = new QDockWidget(tr("Log Console"), m_dockHost);
    m_dockLog->setObjectName("dockLogConsole");
    m_dockLog->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dockLog->setFeatures(QDockWidget::DockWidgetMovable   |
                            QDockWidget::DockWidgetFloatable |
                            QDockWidget::DockWidgetClosable);
    m_dockLog->setWidget(m_logPanel);
    m_dockHost->addDockWidget(Qt::BottomDockWidgetArea, m_dockLog);
    m_dockHost->tabifyDockWidget(m_dockAsset, m_dockLog);
    m_dockAsset->raise();   // 默认显示 Asset Browser

    // Dock: Asset Preview（底部右侧）
    m_dockPreview = new QDockWidget(tr("Asset Preview"), m_dockHost);
    m_dockPreview->setObjectName("dockAssetPreview");
    m_dockPreview->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dockPreview->setFeatures(QDockWidget::DockWidgetMovable   |
                                QDockWidget::DockWidgetFloatable |
                                QDockWidget::DockWidgetClosable);
    m_dockPreview->setWidget(m_previewPanel);
    m_dockHost->addDockWidget(Qt::BottomDockWidgetArea, m_dockPreview);
    // 与 Asset Browser 并排但不 tabify，保持独立列（可按需改为 tabify）

    // ════════════════════════════════════════════════════════
    //  右侧 Dock：Explorer + Create（Tab 合并）+ Property Editor
    // ════════════════════════════════════════════════════════

    // Dock: Explorer（场景树）
    m_dockExplorer = new QDockWidget(tr("Explorer"), m_dockHost);
    m_dockExplorer->setObjectName("dockExplorer");
    m_dockExplorer->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dockExplorer->setFeatures(QDockWidget::DockWidgetMovable   |
                                 QDockWidget::DockWidgetFloatable |
                                 QDockWidget::DockWidgetClosable);
    m_dockExplorer->setWidget(m_explorerPanel);
    m_dockHost->addDockWidget(Qt::RightDockWidgetArea, m_dockExplorer);

    // Dock: Create（与 Explorer Tab 合并）
    m_dockCreate = new QDockWidget(tr("Create"), m_dockHost);
    m_dockCreate->setObjectName("dockCreate");
    m_dockCreate->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dockCreate->setFeatures(QDockWidget::DockWidgetMovable   |
                               QDockWidget::DockWidgetFloatable |
                               QDockWidget::DockWidgetClosable);
    m_dockCreate->setWidget(m_createPanel);
    m_dockHost->addDockWidget(Qt::RightDockWidgetArea, m_dockCreate);
    m_dockHost->tabifyDockWidget(m_dockExplorer, m_dockCreate);
    m_dockExplorer->raise();   // 默认显示 Explorer

    // Dock: Property Editor（右侧，Explorer 下方）
    m_dockProperty = new QDockWidget(tr("Property Editor"), m_dockHost);
    m_dockProperty->setObjectName("dockPropertyEditor");
    m_dockProperty->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_dockProperty->setFeatures(QDockWidget::DockWidgetMovable   |
                                 QDockWidget::DockWidgetFloatable |
                                 QDockWidget::DockWidgetClosable);
    m_dockProperty->setWidget(m_propertyPanel);
    // 垂直分割：放在 Explorer 下方（同 RightDockWidgetArea，不 tabify）
    m_dockHost->addDockWidget(Qt::RightDockWidgetArea, m_dockProperty);
    m_dockHost->splitDockWidget(m_dockExplorer, m_dockProperty, Qt::Vertical);

    // ── 初始尺寸 ──────────────────────────────────────────────
    m_dockHost->resizeDocks({ m_dockAsset   },   { 220 }, Qt::Vertical);
    m_dockHost->resizeDocks({ m_dockPreview },   { 240 }, Qt::Horizontal);
    m_dockHost->resizeDocks({ m_dockExplorer },  { 260 }, Qt::Horizontal);
    m_dockHost->resizeDocks({ m_dockExplorer, m_dockProperty }, { 320, 280 }, Qt::Vertical);

    applyDockStyle();
}

// ─────────────────────────────────────────────────────────────
//  Dock 暗色样式（统一，右侧面板复用相同 QSS）
// ─────────────────────────────────────────────────────────────
void YGmax::applyDockStyle()
{
    m_dockHost->setStyleSheet(R"(
        QMainWindow {
            background: #1b1b1c;
        }
        /* 分隔条 */
        QMainWindow::separator {
            background: #1b1b1c;
            width: 4px;
            height: 4px;
        }
        QMainWindow::separator:hover {
            background: #0078d4;
        }
        /* Dock 标题栏 */
        QDockWidget {
            color: #cccccc;
            font-size: 12px;
            titlebar-close-icon: url(none);
        }
        QDockWidget::title {
            background: #2d2d2d;
            color: #cccccc;
            padding-left: 8px;
            border-bottom: 1px solid #1b1b1c;
            text-align: left;
            height: 24px;
        }
        QDockWidget::close-button,
        QDockWidget::float-button {
            background: transparent;
            border: none;
            padding: 0px;
        }
        QDockWidget::close-button:hover,
        QDockWidget::float-button:hover {
            background: #3f3f46;
            border-radius: 2px;
        }
        /* 多 Dock 合并时的 Tab 条 */
        QTabBar::tab {
            background: #2d2d2d;
            color: #888888;
            border: none;
            border-right: 1px solid #1b1b1c;
            padding: 0 14px;
            height: 26px;
            font-size: 12px;
        }
        QTabBar::tab:selected {
            background: #1e1e1e;
            color: #cccccc;
            border-top: 2px solid #0078d4;
        }
        QTabBar::tab:hover:!selected {
            background: #3f3f46;
            color: #cccccc;
        }
        QSplitter::handle {
            background: #1b1b1c;
        }
    )");
}

// ─────────────────────────────────────────────────────────────
//  保存 / 恢复面板布局
// ─────────────────────────────────────────────────────────────
void YGmax::saveLayout()
{
    QSettings s("Qizhiwoniu", "YGmax");
    s.setValue("windowGeometry", saveGeometry());
    s.setValue("dockLayout",     m_dockHost->saveState());
}

void YGmax::restoreLayout()
{
    QSettings s("Qizhiwoniu", "YGmax");
    const QByteArray geo   = s.value("windowGeometry").toByteArray();
    const QByteArray state = s.value("dockLayout").toByteArray();
    if (!geo.isEmpty())   restoreGeometry(geo);
    if (!state.isEmpty()) m_dockHost->restoreState(state);
}

// ─────────────────────────────────────────────────────────────
//  托盘
// ─────────────────────────────────────────────────────────────
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
            3000
        );
    });

    m_tray->show();
}

// ─────────────────────────────────────────────────────────────
//  关闭 → 先保存布局再最小化到托盘
// ─────────────────────────────────────────────────────────────
void YGmax::closeEvent(QCloseEvent* event)
{
    saveLayout();
    event->ignore();
    hide();
    m_tray->showMessage(
        VER_PRODUCT_NAME,
        tr("程序已最小化到托盘，双击图标可重新打开"),
        QSystemTrayIcon::Information,
        2000
    );
}

// ─────────────────────────────────────────────────────────────
//  菜单栏
// ─────────────────────────────────────────────────────────────
void YGmax::setupMenuBar()
{
    QMenuBar* mb = m_titleBar->menuBar();

    // 文件
    QMenu* fileMenu  = mb->addMenu(tr("文件(&F)"));
    m_newDocAct = new QAction(tr("新建"), this);
    m_newDocAct->setShortcut(QKeySequence::New);
    connect(m_newDocAct, &QAction::triggered, this, [this]() {
        static int n = 1;
        m_tabBar->setFixedHeight(34);
        m_tabBar->addTab(tr("未命名文档%1").arg(n == 1 ? "" : QString(" %1").arg(n)));
        n++;
        m_tabBar->show();
        m_viewport->setVisible(true);
    });
    fileMenu->addAction(m_newDocAct);

    QAction* openAct = new QAction(tr("打开"), this);
    openAct->setShortcut(QKeySequence::Open);
    fileMenu->addAction(openAct);

    fileMenu->addSeparator();

    QAction* quitAct = new QAction(tr("退出"), this);
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAct);

    // 编辑
    QMenu* editMenu = mb->addMenu(tr("编辑(&E)"));
    QAction* undoAct = new QAction(tr("撤销"), this);
    undoAct->setShortcut(QKeySequence::Undo);
    editMenu->addAction(undoAct);

    // 视图：快速显示/隐藏各面板
    // 注意：setupMenuBar() 在 setupDockHost() 之前调用，各 m_dock* 此时仍为
    // nullptr。lambda 必须通过 this 成员指针在触发时才解引用，不能直接捕获。
    QMenu* viewMenu = mb->addMenu(tr("视图(&V)"));

    // 用成员指针偏移量做延迟绑定：触发时再取实际 dock 指针
    auto makeViewAct = [&](const QString& label, QDockWidget* YGmax::* member) {
        QAction* act = new QAction(label, this);
        act->setCheckable(true);
        act->setChecked(true);
        connect(act, &QAction::toggled, this, [this, member](bool v) {
            if (QDockWidget* dock = this->*member)
                dock->setVisible(v);
        });
        // 打开菜单时同步勾选状态（面板可能被用户手动关闭过）
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
//  全局事件过滤器：让子控件上的鼠标事件也能触发边缘 resize
// ─────────────────────────────────────────────────────────────
bool YGmax::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched)

    // 只处理鼠标事件
    if (event->type() != QEvent::MouseMove       &&
        event->type() != QEvent::MouseButtonPress &&
        event->type() != QEvent::MouseButtonRelease)
        return false;

    auto* me = static_cast<QMouseEvent*>(event);

    // 把全局坐标转换到本窗口局部坐标
    const QPoint localPos = mapFromGlobal(me->globalPosition().toPoint());

    if (event->type() == QEvent::MouseMove) {
        if (m_resizing) {
            // 正在拖拽缩放 → 继续更新几何
            const QPoint delta = me->globalPosition().toPoint() - m_resizeStart;
            QRect g = m_resizeOrigGeom;
            const int minW = 400, minH = 300;

            if (m_resizeDir & Left) {
                int newLeft = g.left() + delta.x();
                if (g.right() - newLeft >= minW) g.setLeft(newLeft);
            }
            if (m_resizeDir & Right) {
                int newW = g.width() + delta.x();
                if (newW >= minW) g.setWidth(newW);
            }
            if (m_resizeDir & Top) {
                int newTop = g.top() + delta.y();
                if (g.bottom() - newTop >= minH) g.setTop(newTop);
            }
            if (m_resizeDir & Bottom) {
                int newH = g.height() + delta.y();
                if (newH >= minH) g.setHeight(newH);
            }
            setGeometry(g);
            return true;   // 吃掉事件，防止子控件误处理
        }

        // 未拖拽 → 根据边缘位置更新光标
        ResizeDir dir = hitTest(localPos);
        updateCursor(dir);
        // 不 return true，让子控件也收到 move 事件（悬停高亮等）
        return false;
    }

    if (event->type() == QEvent::MouseButtonPress &&
        me->button() == Qt::LeftButton)
    {
        ResizeDir dir = hitTest(localPos);
        if (dir != None) {
            m_resizing       = true;
            m_resizeDir      = dir;
            m_resizeStart    = me->globalPosition().toPoint();
            m_resizeOrigGeom = geometry();
            return true;   // 吃掉，防止触发子控件点击
        }
        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease &&
        me->button() == Qt::LeftButton && m_resizing)
    {
        m_resizing  = false;
        m_resizeDir = None;
        unsetCursor();
        return true;
    }

    return false;
}


YGmax::ResizeDir YGmax::hitTest(const QPoint& pos) const
{
    const int x = pos.x(), y = pos.y();
    const int w = width(),  h = height();
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
    case Left:  case Right:       setCursor(Qt::SizeHorCursor);  break;
    case Top:   case Bottom:      setCursor(Qt::SizeVerCursor);  break;
    case TopLeft: case BottomRight: setCursor(Qt::SizeFDiagCursor); break;
    case TopRight: case BottomLeft: setCursor(Qt::SizeBDiagCursor); break;
    default:                      unsetCursor();                 break;
    }
}

void YGmax::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        ResizeDir dir = hitTest(event->pos());
        if (dir != None) {
            m_resizing      = true;
            m_resizeDir     = dir;
            m_resizeStart   = event->globalPosition().toPoint();
            m_resizeOrigGeom = geometry();
            event->accept();
            return;
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

        if (m_resizeDir & Left) {
            int newLeft = g.left() + delta.x();
            if (g.right() - newLeft >= minW) g.setLeft(newLeft);
        }
        if (m_resizeDir & Right) {
            int newW = g.width() + delta.x();
            if (newW >= minW) g.setWidth(newW);
        }
        if (m_resizeDir & Top) {
            int newTop = g.top() + delta.y();
            if (g.bottom() - newTop >= minH) g.setTop(newTop);
        }
        if (m_resizeDir & Bottom) {
            int newH = g.height() + delta.y();
            if (newH >= minH) g.setHeight(newH);
        }
        setGeometry(g);
        event->accept();
        return;
    }

    // 悬停时更新光标
    updateCursor(hitTest(event->pos()));
    QWidget::mouseMoveEvent(event);
}

void YGmax::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_resizing && event->button() == Qt::LeftButton) {
        m_resizing  = false;
        m_resizeDir = None;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}
