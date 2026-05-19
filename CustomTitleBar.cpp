#include "stdafx.h" 
#include "CustomTitleBar.h"

CustomTitleBar::CustomTitleBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(36);
    setMouseTracking(true);

    // --- 图标 ---
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(20, 20);
    m_iconLabel->setScaledContents(true);

    // --- 标题 ---
    //m_titleLabel //= new QLabel("YGmax", this);
    //m_titleLabel->setObjectName("titleLabel");

    // --- 菜单栏 ---
    m_menuBar = new QMenuBar(this);
    m_menuBar->setMaximumHeight(24);          // ← 限制高度贴近文字
    m_menuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // --- 窗口按钮 ---
    m_minBtn = new QPushButton("─", this);
    m_maxBtn = new QPushButton("□", this);
    m_closeBtn = new QPushButton("✕", this);
    setFixedHeight(32);
    for (auto* btn : { m_minBtn, m_maxBtn, m_closeBtn }) {
        btn->setFixedSize(46, 32);
        btn->setFlat(true);
    }
    m_minBtn->setObjectName("minBtn");
    m_maxBtn->setObjectName("maxBtn");
    m_closeBtn->setObjectName("closeBtn");
    

    // --- 布局 ---
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(m_iconLabel);
    //layout->addWidget(m_titleLabel);
    layout->addWidget(m_menuBar);
    layout->addStretch();
    layout->addWidget(m_minBtn);
    layout->addWidget(m_maxBtn);
    layout->addWidget(m_closeBtn);
    setLayout(layout);

    // --- 信号 ---
    connect(m_minBtn, &QPushButton::clicked, this, &CustomTitleBar::minimizeClicked);
    connect(m_maxBtn, &QPushButton::clicked, this, &CustomTitleBar::maximizeClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &CustomTitleBar::closeClicked);

    applyStyle();
}

void CustomTitleBar::setTitle(const QString& title)
{
    m_titleLabel->setText(title);
}

void CustomTitleBar::setIcon(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        // 直接加载资源，调试用
        QPixmap ico(":/YGmax/YGmax.ico");
        if (!ico.isNull())
            m_iconLabel->setPixmap(ico.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    else {
        m_iconLabel->setPixmap(pixmap.scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void CustomTitleBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragStartPos = event->globalPosition().toPoint() - window()->frameGeometry().topLeft();
    }
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        // 最大化状态下拖动先还原
        if (window()->isMaximized()) {
            window()->showNormal();
            m_dragStartPos = QPoint(window()->width() / 2, 10);
        }
        window()->move(event->globalPosition().toPoint() - m_dragStartPos);
    }
}

void CustomTitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
        m_dragging = false;
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event)
        emit maximizeClicked();
}

void CustomTitleBar::applyStyle()
{
    setStyleSheet(R"(
        CustomTitleBar {
            background-color: #1b1b1c;
            border-bottom: 1px solid #3f3f46;
        }
        QMenuBar {
            background: transparent; 
            color: #cccccc;
            font-size: 13px;
        }
        QMenuBar::item {
            padding: 4px 8px;
            background: transparent;
        }
        QMenuBar::item:selected {
            background: #3f3f46;
            color: #ffffff;
        }
        QMenu {
            background: #1b1b1c;
            color: #cccccc;
            border: 1px solid #3f3f46;
            padding: 2px;
        }
        QMenu::item {
            padding: 5px 28px 5px 12px;
        }
        QMenu::item:selected {
            background: #094771;
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background: #3f3f46;
            margin: 3px 6px;
        }
        /* 最小化、最大化 —— VS 风格：hover 深灰 */
        QPushButton#minBtn, QPushButton#maxBtn {
            color: #cccccc;
            background: transparent;
            border: none;
            font-size: 14px;
            font-family: "Segoe MDL2 Assets", "Segoe UI Symbol";
        }
        QPushButton#minBtn:hover, QPushButton#maxBtn:hover {
            background: #3f3f46;
            color: #ffffff;
        }
        QPushButton#minBtn:pressed, QPushButton#maxBtn:pressed {
            background: #555558;
        }

        /* 关闭按钮 —— VS 风格：hover 红色 */
        QPushButton#closeBtn {
            color: #cccccc;
            background: transparent;
            border: none;
            font-size: 13px;
        }
        QPushButton#closeBtn:hover {
            background: #c42b1c;
            color: #ffffff;
        }
        QPushButton#closeBtn:pressed {
            background: #b22a1b;
        }
        }
    )");
}