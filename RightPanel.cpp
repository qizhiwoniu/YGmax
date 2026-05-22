#include "stdafx.h"
#include "RightPanel.h"

// ─────────────────────────────────────────────────────────────
//  共用暗色样式（面板内部控件）
// ─────────────────────────────────────────────────────────────
static const char* kPanelQss = R"(
    QWidget {
        background: #1e1e1e;
        color: #cccccc;
        font-size: 12px;
    }
    QLineEdit {
        background: #2d2d2d;
        border: 1px solid #3f3f46;
        border-radius: 3px;
        padding: 3px 6px;
        color: #cccccc;
    }
    QLineEdit:focus {
        border-color: #0078d4;
    }
    QTreeWidget {
        background: #1e1e1e;
        border: none;
        color: #cccccc;
    }
    QTreeWidget::item {
        height: 22px;
        padding-left: 4px;
    }
    QTreeWidget::item:selected {
        background: #094771;
        color: #ffffff;
    }
    QTreeWidget::item:hover:!selected {
        background: #2a2d2e;
    }
    QTreeWidget::branch {
        background: transparent;
    }
    QScrollBar:vertical {
        background: #1e1e1e;
        width: 8px;
    }
    QScrollBar::handle:vertical {
        background: #3f3f46;
        border-radius: 4px;
        min-height: 20px;
    }
    QScrollBar::handle:vertical:hover {
        background: #0078d4;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0px;
    }
    QGroupBox {
        border: 1px solid #3f3f46;
        border-radius: 4px;
        margin-top: 6px;
        font-size: 11px;
        color: #888888;
    }
    QGroupBox::title {
        subcontrol-origin: margin;
        subcontrol-position: top left;
        padding: 0 4px;
        left: 8px;
    }
    QDoubleSpinBox, QComboBox {
        background: #2d2d2d;
        border: 1px solid #3f3f46;
        border-radius: 3px;
        padding: 2px 4px;
        color: #cccccc;
    }
    QDoubleSpinBox:focus, QComboBox:focus {
        border-color: #0078d4;
    }
    QCheckBox {
        spacing: 6px;
    }
    QCheckBox::indicator {
        width: 14px;
        height: 14px;
        background: #2d2d2d;
        border: 1px solid #3f3f46;
        border-radius: 2px;
    }
    QCheckBox::indicator:checked {
        background: #0078d4;
        border-color: #0078d4;
    }
    QPushButton, QToolButton {
        background: #2d2d2d;
        border: 1px solid #3f3f46;
        border-radius: 3px;
        padding: 4px 10px;
        color: #cccccc;
    }
    QPushButton:hover, QToolButton:hover {
        background: #3f3f46;
    }
    QPushButton:pressed, QToolButton:pressed {
        background: #0078d4;
        border-color: #0078d4;
    }
    QLabel#sectionLabel {
        color: #888888;
        font-size: 11px;
    }
)";

// ══════════════════════════════════════════════════════════════
//  ExplorerPanel
// ══════════════════════════════════════════════════════════════
ExplorerPanel::ExplorerPanel(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(kPanelQss);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 顶部搜索栏 ─────────────────────────────────────────
    auto* searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(4, 4, 4, 4);
    searchRow->setSpacing(4);

    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Search…"));
    searchRow->addWidget(m_searchBox);

    QToolButton* filterBtn = new QToolButton(this);
    filterBtn->setText("⊞");
    filterBtn->setFixedWidth(24);
    searchRow->addWidget(filterBtn);

    root->addLayout(searchRow);

    // ── 分隔线 ─────────────────────────────────────────────
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background:#3f3f46; max-height:1px;");
    root->addWidget(sep);

    // ── 场景树 ─────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(16);
    m_tree->setAnimated(true);
    root->addWidget(m_tree, 1);

    buildDefaultScene();

    // 搜索过滤
    connect(m_searchBox, &QLineEdit::textChanged, this, [this](const QString& text) {
        std::function<void(QTreeWidgetItem*)> filter = [&](QTreeWidgetItem* item) {
            bool match = text.isEmpty() ||
                         item->text(0).contains(text, Qt::CaseInsensitive);
            for (int i = 0; i < item->childCount(); ++i) {
                filter(item->child(i));
                if (item->child(i)->isHidden() == false) match = true;
            }
            item->setHidden(!match);
        };
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
            filter(m_tree->topLevelItem(i));
    });

    // 选中信号
    connect(m_tree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem* item, int) {
                emit nodeSelected(item->text(0));
            });
}

void ExplorerPanel::buildDefaultScene()
{
    // Units
    auto* units = new QTreeWidgetItem(m_tree, { tr("Units") });
    units->setIcon(0, QIcon());
    for (const QString& name : { "Basic_Floor (Unit)", "ChamfBox (Unit)",
                                  "reflection_probe (Unit)", "Skydome (Unit)", "Wall (Unit)" })
        new QTreeWidgetItem(units, { name });
    units->setExpanded(true);

    // Level Objects
    auto* levelObjs = new QTreeWidgetItem(m_tree, { tr("Level Objects") });
    new QTreeWidgetItem(levelObjs, { "midday_shading_environment (LevelEntity)" });
    levelObjs->setExpanded(true);

    // Lights
    auto* lights = new QTreeWidgetItem(m_tree, { tr("Lights") });
    new QTreeWidgetItem(lights, { "light_source (Unit)" });
    lights->setExpanded(true);

    m_tree->expandAll();
}

// ══════════════════════════════════════════════════════════════
//  CreatePanel
// ══════════════════════════════════════════════════════════════
CreatePanel::CreatePanel(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(kPanelQss);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // ── 分组：Primitives ──────────────────────────────────
    auto* primGroup = new QGroupBox(tr("Primitives"), this);
    auto* primLayout = new QVBoxLayout(primGroup);
    primLayout->setSpacing(3);
    for (const QString& name : { tr("Box"), tr("Sphere"), tr("Cylinder"),
                                   tr("Plane"), tr("Cone") }) {
        auto* btn = new QPushButton(name, primGroup);
        primLayout->addWidget(btn);
    }
    root->addWidget(primGroup);

    // ── 分组：Lights ──────────────────────────────────────
    auto* lightGroup = new QGroupBox(tr("Lights"), this);
    auto* lightLayout = new QVBoxLayout(lightGroup);
    lightLayout->setSpacing(3);
    for (const QString& name : { tr("Point Light"), tr("Spot Light"),
                                   tr("Directional Light") }) {
        auto* btn = new QPushButton(name, lightGroup);
        lightLayout->addWidget(btn);
    }
    root->addWidget(lightGroup);

    // ── 分组：Camera ──────────────────────────────────────
    auto* camGroup = new QGroupBox(tr("Camera"), this);
    auto* camLayout = new QVBoxLayout(camGroup);
    camLayout->setSpacing(3);
    auto* camBtn = new QPushButton(tr("Camera Unit"), camGroup);
    camLayout->addWidget(camBtn);
    root->addWidget(camGroup);

    root->addStretch(1);
}

// ══════════════════════════════════════════════════════════════
//  PropertyEditorPanel
// ══════════════════════════════════════════════════════════════
PropertyEditorPanel::PropertyEditorPanel(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(kPanelQss);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 标题行 ────────────────────────────────────────────
    m_titleLabel = new QLabel(tr("(no object selected)"), this);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setStyleSheet("padding: 4px 8px; color:#888888; font-size:12px;"
                                 "border-bottom:1px solid #3f3f46; background:#2d2d2d;");
    root->addWidget(m_titleLabel);

    // ── 可滚动内容区 ─────────────────────────────────────
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    root->addWidget(m_scroll, 1);

    buildEmptyState();
}

void PropertyEditorPanel::buildEmptyState()
{
    auto* placeholder = new QLabel(tr("Nothing to show"), m_scroll);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color:#555555; font-size:12px;");
    m_scroll->setWidget(placeholder);
    m_container = placeholder;
}

void PropertyEditorPanel::inspectNode(const QString& nodeName)
{
    m_titleLabel->setText(nodeName);

    // 销毁旧内容
    if (m_container) {
        m_container->deleteLater();
        m_container = nullptr;
    }

    auto* container = new QWidget;
    container->setStyleSheet(kPanelQss);
    auto* form = new QFormLayout(container);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // ── Transform 组 ─────────────────────────────────────
    auto makeVec3Row = [&](const QString& label) {
        auto* row = new QHBoxLayout;
        row->setSpacing(3);
        for (const QString& axis : { "X", "Y", "Z" }) {
            auto* spin = new QDoubleSpinBox;
            spin->setRange(-99999, 99999);
            spin->setSingleStep(0.1);
            spin->setDecimals(3);
            spin->setPrefix(axis + " ");
            spin->setFixedWidth(78);
            row->addWidget(spin);
        }
        form->addRow(label, row);
    };

    auto* transLabel = new QLabel(tr("Transform"), container);
    transLabel->setObjectName("sectionLabel");
    form->addRow(transLabel);
    makeVec3Row(tr("Position"));
    makeVec3Row(tr("Rotation"));
    makeVec3Row(tr("Scale"));

    // ── Visibility ────────────────────────────────────────
    auto* visLabel = new QLabel(tr("Visibility"), container);
    visLabel->setObjectName("sectionLabel");
    form->addRow(visLabel);
    auto* visCheck = new QCheckBox(tr("Visible"), container);
    visCheck->setChecked(true);
    form->addRow("", visCheck);

    // ── Layer ─────────────────────────────────────────────
    auto* layerCombo = new QComboBox(container);
    layerCombo->addItems({ "Default", "Layer1", "Layer2", "Background" });
    form->addRow(tr("Layer"), layerCombo);

    m_container = container;
    m_scroll->setWidget(container);
}
