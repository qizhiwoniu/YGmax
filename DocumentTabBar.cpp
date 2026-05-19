#include "stdafx.h"
#include "DocumentTabBar.h"

DocumentTabBar::DocumentTabBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(34);

    // 滚动区域（标签多了可以横向滚动）
    m_scroll = new QScrollArea(this);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);

    m_tabWidget = new QWidget(m_scroll);
    m_tabLayout = new QHBoxLayout(m_tabWidget);
    m_tabLayout->setContentsMargins(0, 0, 0, 0);
    m_tabLayout->setSpacing(2);
    m_tabLayout->addStretch();
    m_tabWidget->setLayout(m_tabLayout);
    m_scroll->setWidget(m_tabWidget);

    // 新建标签按钮
    m_newBtn = new QPushButton("+", this);
    m_newBtn->setFixedSize(30, 28);
    m_newBtn->setFlat(true);
    m_newBtn->setObjectName("newTabBtn");
    m_newBtn->setToolTip(tr("新建文档"));
    connect(m_newBtn, &QPushButton::clicked, this, &DocumentTabBar::newTabRequested);

    QHBoxLayout* outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 4, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(m_scroll);
    outerLayout->addWidget(m_newBtn);
    setLayout(outerLayout);

    applyStyle();
}

int DocumentTabBar::addTab(const QString& title)
{
    DocTab tab;
    tab.title = title;

    // 在 stretch 前插入
    auto* btn = new QPushButton(this);
    btn->setCheckable(true);
    btn->setObjectName("tabBtn");
    btn->setFixedHeight(30);
    btn->setMinimumWidth(120);
    btn->setMaximumWidth(200);

    tab.btn = btn;
    m_tabs.append(tab);

    // 标签内容：文件名 + 关闭按钮（用布局模拟）
    auto* closeBtn = new QPushButton("✕", btn);
    closeBtn->setFixedSize(16, 16);
    closeBtn->setObjectName("tabCloseBtn");
    closeBtn->setCursor(Qt::ArrowCursor);

    auto* btnLayout = new QHBoxLayout(btn);
    btnLayout->setContentsMargins(10, 0, 4, 0);
    btnLayout->setSpacing(4);

    auto* titleLabel = new QLabel(title, btn);
    titleLabel->setObjectName("tabTitle");
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    btnLayout->addWidget(titleLabel);
    btnLayout->addWidget(closeBtn);
    btn->setLayout(btnLayout);

    // 插到 stretch 前
    m_tabLayout->insertWidget(m_tabLayout->count() - 1, btn);

    // ← 不捕获 index，每次点击时动态查找
    connect(btn, &QPushButton::clicked, this, [this, btn]() {
        int idx = indexOfBtn(btn);
        if (idx >= 0) setCurrentTab(idx);
        });
    connect(closeBtn, &QPushButton::clicked, this, [this, btn]() {
        int idx = indexOfBtn(btn);
        if (idx >= 0) emit tabCloseRequested(idx);
        });

    int index = m_tabs.count() - 1;
    setCurrentTab(index);
    return index;
}

void DocumentTabBar::removeTab(int index)
{
    if (index < 0 || index >= m_tabs.count()) return;

    auto& tab = m_tabs[index];
    m_tabLayout->removeWidget(tab.btn);
    tab.btn->deleteLater();
    m_tabs.removeAt(index);

    // 重新确定当前标签
    if (m_tabs.isEmpty()) {
        m_current = -1;
    }
    else {
        int next = qMin(index, m_tabs.count() - 1);
        setCurrentTab(next);
    }
}

void DocumentTabBar::setCurrentTab(int index)
{
    if (index < 0 || index >= m_tabs.count()) return;
    m_current = index;
    for (int i = 0; i < m_tabs.count(); ++i)
        m_tabs[i].btn->setChecked(i == index);
    emit tabChanged(index);
}

void DocumentTabBar::setTabModified(int index, bool modified)
{
    if (index < 0 || index >= m_tabs.count()) return;
    m_tabs[index].modified = modified;

    // 找到标签里的 titleLabel 更新文字
    auto* label = m_tabs[index].btn->findChild<QLabel*>("tabTitle");
    if (label) {
        QString text = m_tabs[index].title;
        if (modified) text.prepend("• ");
        label->setText(text);
    }
}

void DocumentTabBar::applyStyle()
{
    setStyleSheet(R"(
        DocumentTabBar {
            background: #252526;
            border-bottom: 1px solid #3f3f46;
        }
        QScrollArea, QWidget {
            background: transparent;
        }
        /* 标签按钮 */
        QPushButton#tabBtn {
            background: #2d2d2d;
            color: #999999;
            border: none;
            border-right: 1px solid #1b1b1c;
            border-radius: 0px;
            text-align: left;
            font-size: 12px;
            padding: 0px;
        }
        QPushButton#tabBtn:hover {
            background: #3f3f46;
            color: #cccccc;
        }
        QPushButton#tabBtn:checked {
            background: #1b1b1c;
            color: #ffffff;
            border-top: 2px solid #0078d4;
        }
        /* 标签内的标题 */
        QLabel#tabTitle {
            background: transparent;
            color: inherit;
            font-size: 12px;
        }
        /* 标签关闭按钮 */
        QPushButton#tabCloseBtn {
            background: transparent;
            color: #666666;
            border: none;
            border-radius: 3px;
            font-size: 10px;
            padding: 0px;
        }
        QPushButton#tabCloseBtn:hover {
            background: #c42b1c;
            color: #ffffff;
        }
        /* 新建标签按钮 */
        QPushButton#newTabBtn {
            background: transparent;
            color: #999999;
            border: none;
            font-size: 18px;
            border-radius: 3px;
        }
        QPushButton#newTabBtn:hover {
            background: #3f3f46;
            color: #ffffff;
        }
    )");
}