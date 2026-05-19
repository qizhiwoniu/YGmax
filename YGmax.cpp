#include "stdafx.h"
#include "YGmax.h"
#include "AboutDialog.h"
#include "version.h"  
#include "ToolBar.h"  
#include "DocumentTabBar.h" 
#include <QCloseEvent>  

YGmax::YGmax(QWidget* parent)
    : QWidget(parent)
{
    //ui.setupUi(this);

    // 去掉系统标题栏
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);  // ← 加这一行，背景透明才能显示圆角
    resize(1280, 720);
    // 自定义标题栏
    m_titleBar = new CustomTitleBar(this);
    //m_titleBar->setTitle("YGmax");
    // 设置图标（替换成你自己的图标路径）
    m_titleBar->setIcon(QPixmap());

    m_toolBar = new ToolBar(this);

    m_tabBar = new DocumentTabBar(this);
    // 默认打开一个文档
    //m_tabBar->addTab(tr("未命名文档"));

    setupMenuBar();
    setupTray();
    // 窗口控制
    connect(m_titleBar, &CustomTitleBar::minimizeClicked, this, &QWidget::showMinimized);
    connect(m_titleBar, &CustomTitleBar::closeClicked, this, &QWidget::close);
    connect(m_titleBar, &CustomTitleBar::maximizeClicked, this, [this] {
        isMaximized() ? showNormal() : showMaximized();
        });
    // 工具栏信号
    connect(m_toolBar, &ToolBar::selectModeActivated, this, []() { /* TODO */ });
    connect(m_toolBar, &ToolBar::defaultModeActivated, this, []() { /* TODO */ });
    connect(m_toolBar, &ToolBar::actionTriggered, this, [](int id) {
        // id 对应上面 addIconBtn 里的编号
        Q_UNUSED(id)
        });
    // 标签信号
    connect(m_tabBar, &DocumentTabBar::newTabRequested, m_newDocAct, &QAction::trigger);
    connect(m_tabBar, &DocumentTabBar::tabCloseRequested, this, [this](int index) {
        m_tabBar->removeTab(index);
        // 全部关完时隐藏内容区，露出窗口背景
        if (m_tabBar->count() == 0) {
            // 不hide，用透明占位保持布局
            m_tabBar->setFixedHeight(0);
            m_contentWidget->setVisible(false);
            // contentWidget 位置保留，用 spacer 撑开
            m_contentWidget->setMinimumHeight(0);
        }
        });
    connect(m_tabBar, &DocumentTabBar::tabChanged, this, [this](int index) {
        Q_UNUSED(index)
            // TODO: 切换对应内容区
        });
    // 内容区
    m_contentWidget = new QWidget(this);
    m_contentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_titleBar);
    mainLayout->addWidget(m_toolBar);       // ← 新增
    mainLayout->addWidget(m_tabBar);
    // 原 ui 内容区放这里，或直接 addStretch()
    mainLayout->addWidget(m_contentWidget);
    mainLayout->addStretch(1);
    setLayout(mainLayout);

    // ← 默认文档最后加，此时 m_contentWidget 已存在
    m_newDocAct->trigger();
}

YGmax::~YGmax() {}
// ── 托盘初始化 ──────────────────────────────────────
void YGmax::setupTray()
{
    m_tray = new SystemTrayIcon(this, this);

    connect(m_tray, &SystemTrayIcon::showMainWindow, this, [this]() {
        showNormal();
        activateWindow();
        raise();
        });

    connect(m_tray, &SystemTrayIcon::checkUpdate, this, [this]() {
        // TODO: 接入真实更新逻辑
        m_tray->showMessage(
            QString(VER_PRODUCT_NAME),
            tr("当前已是最新版本  v%1").arg(VERSION_STR),
            QSystemTrayIcon::Information,
            3000
        );
        });

    m_tray->show();
}

// ── 关闭按钮 → 隐藏到托盘 ────────────────────────────────
void YGmax::closeEvent(QCloseEvent* event)
{
    event->ignore();          // 拦截关闭
    hide();                   // 隐藏主窗口
    m_tray->showMessage(
        VER_PRODUCT_NAME,
        tr("程序已最小化到托盘，双击图标可重新打开"),
        QSystemTrayIcon::Information,
        2000
    );
}
// ── 菜单栏 ────────────────────────────────────────
void YGmax::setupMenuBar()
{
    QMenuBar* mb = m_titleBar->menuBar();

    // 文件菜单
    QMenu* fileMenu = mb->addMenu(tr("文件(&F)"));
    m_newDocAct = new QAction(tr("新建"), this);
    m_newDocAct->setShortcut(QKeySequence::New);
    connect(m_newDocAct, &QAction::triggered, this, [this]() {
        static int n = 1;
        m_tabBar->setFixedHeight(34);   // ← 恢复标签栏高度
        m_tabBar->addTab(tr("未命名文档%1").arg(n == 1 ? "" : QString(" %1").arg(n)));
        n++;
        // 确保标签栏和内容区可见
        m_tabBar->show();
        m_contentWidget->show();
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

    // 编辑菜单
    QMenu* editMenu = mb->addMenu(tr("编辑(&E)"));
    QAction* undoAct = new QAction(tr("撤销"), this);
    undoAct->setShortcut(QKeySequence::Undo);
    editMenu->addAction(undoAct);

    // 帮助菜单
    QMenu* helpMenu = mb->addMenu(tr("帮助(&H)"));
    QAction* aboutAct = new QAction(tr("关于"), this);
    connect(aboutAct, &QAction::triggered, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
        });
    helpMenu->addAction(aboutAct);
}
// ── 窗口圆角 ───────────────────────────────────────
void YGmax::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
        QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);  // 抗锯齿

    QPainterPath path;
    path.addRoundedRect(rect(), 10, 10);  // 10 是圆角半径，可自行调整

    // 填充窗口背景色
    painter.fillPath(path, QColor("#1b1b1c"));
}
void YGmax::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event)
        // 用 QRegion 裁剪窗口形状，防止圆角外区域响应鼠标
        QRegion region(rect(), QRegion::Rectangle);
    QRegion roundedRegion(rect().adjusted(0, 0, -1, -1), QRegion::Rectangle);

    QPainterPath path;
    path.addRoundedRect(rect(), 10, 10);
    setMask(QRegion(path.toFillPolygon().toPolygon()));
}