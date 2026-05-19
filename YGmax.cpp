#include "stdafx.h"
#include "YGmax.h"

YGmax::YGmax(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    // 去掉系统标题栏
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    setAttribute(Qt::WA_TranslucentBackground);  // ← 加这一行，背景透明才能显示圆角

    // 自定义标题栏
    m_titleBar = new CustomTitleBar(this);
    //m_titleBar->setTitle("YGmax");
    // 设置图标（替换成你自己的图标路径）
    m_titleBar->setIcon(QPixmap());

    setupMenuBar();

    // 窗口控制
    connect(m_titleBar, &CustomTitleBar::minimizeClicked, this, &QWidget::showMinimized);
    connect(m_titleBar, &CustomTitleBar::closeClicked, this, &QWidget::close);
    connect(m_titleBar, &CustomTitleBar::maximizeClicked, this, [this] {
        isMaximized() ? showNormal() : showMaximized();
        });

    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_titleBar);
    // 原 ui 内容区放这里，或直接 addStretch()
    mainLayout->addStretch();
    setLayout(mainLayout);
}

YGmax::~YGmax() {}

void YGmax::setupMenuBar()
{
    QMenuBar* mb = m_titleBar->menuBar();

    // 文件菜单
    QMenu* fileMenu = mb->addMenu(tr("文件(&F)"));
    QAction* newAct = new QAction(tr("新建"), this);
    newAct->setShortcut(QKeySequence::New);
    fileMenu->addAction(newAct);

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
    helpMenu->addAction(aboutAct);
}

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