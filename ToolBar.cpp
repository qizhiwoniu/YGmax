#include "stdafx.h"
#include "ToolBar.h"
#include <QPainter>

ToolBar::ToolBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(52);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 0, 12, 0);
    layout->setSpacing(4);

    // ── 模式按钮组（互斥 checkable）──────────────────────
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);


    // Default 按钮 — 用图标，选中时蓝色，未选中时原色
    m_defaultBtn = new QPushButton(this);
    m_defaultBtn->setCheckable(true);
    m_defaultBtn->setChecked(true);
    m_defaultBtn->setObjectName("modeBtn");
    m_defaultBtn->setToolTip(tr("选择模式"));
    m_defaultBtn->setFixedSize(44, 44);

    // 用 QIcon 的 Normal/Selected 两个状态
    QIcon defaultIcon;
    QPixmap basePix(":/YGmax/ico/Select.png");
    // 未选中：原图（灰色调）
    defaultIcon.addPixmap(basePix, QIcon::Normal, QIcon::Off);
    // 选中：染成蓝色
    QPixmap bluePix(basePix.size());
    bluePix.fill(Qt::transparent);
    QPainter painter(&bluePix);
    painter.drawPixmap(0, 0, basePix);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(bluePix.rect(), QColor("#0078d4"));
    painter.end();
    defaultIcon.addPixmap(bluePix, QIcon::Normal, QIcon::On);

    m_defaultBtn->setIcon(defaultIcon);
    m_defaultBtn->setIconSize(QSize(28, 28));

    // 
    layout->addWidget(m_defaultBtn);
    connect(m_defaultBtn, &QPushButton::clicked, this, &ToolBar::defaultModeActivated);


    // ── 分割线 ────────────────────────────────────────────
    layout->addWidget(addSeparator());

    // ── 图标按钮（按需增减）──────────────────────────────
    layout->addWidget(addIconBtn("📂", tr("打开"), 1));
    layout->addWidget(addIconBtn("💾", tr("保存"), 2));
    layout->addWidget(addSeparator());
    layout->addWidget(addIconBtn("↩", tr("撤销"), 3));
    layout->addWidget(addIconBtn("↪", tr("重做"), 4));

    layout->addStretch();
    setLayout(layout);

    applyStyle();
}

QPushButton* ToolBar::addIconBtn(const QString& icon, const QString& tooltip, int id)
{
    auto* btn = new QPushButton(icon, this);
    btn->setFixedSize(44, 44);
    btn->setFlat(true);
    btn->setObjectName("iconBtn");
    btn->setToolTip(tooltip);
    connect(btn, &QPushButton::clicked, this, [this, id]() {
        emit actionTriggered(id);
        });
    return btn;
}

QFrame* ToolBar::addSeparator()
{
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(1);
    sep->setFixedHeight(28);
    sep->setObjectName("toolSep");
    return sep;
}

void ToolBar::applyStyle()
{
    setStyleSheet(R"(
        ToolBar {
            background-color: #2d2d2d;
            border-bottom: 1px solid #3f3f46;
        }
        /* Default / Select 模式按钮 */
        QPushButton#modeBtn {
            background: transparent;
            border: 1px solid transparent;
            border-radius: 6px;
            height: 44px;
            font-size: 13px;
        }
        QPushButton#modeBtn:hover {
            background: #3f3f46;
        }
        QPushButton#modeBtn:checked {
            background: #1a3a52;
            border-color: #0078d4;
        }
        QPushButton#modeBtn:pressed {
            background: #0078d4;
        }
        /* 图标按钮 */
        QPushButton#iconBtn {
            color: #cccccc;
            background: transparent;
            border: none;
            border-radius: 6px;
            font-size: 20px;
        }
        QPushButton#iconBtn:hover {
            background: #3f3f46;
            color: #ffffff;
        }
        QPushButton#iconBtn:pressed {
            background: #555558;
        }
        /* 分割线 */
        QFrame#toolSep {
            color: #3f3f46;
        }
    )");
}