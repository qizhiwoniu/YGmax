#include "stdafx.h"
#include "RightPanel.h"

// ─────────────────────────────────────────────────────────────
//  共用暗色样式
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
    QLineEdit:focus { border-color: #0078d4; }
    QTreeWidget {
        background: #1e1e1e;
        border: none;
        color: #cccccc;
    }
    QTreeWidget::item { height: 22px; padding-left: 4px; }
    QTreeWidget::item:selected { background: #094771; color: #ffffff; }
    QTreeWidget::item:hover:!selected { background: #2a2d2e; }
    QTreeWidget::branch { background: transparent; }
    QScrollBar:vertical { background: #1e1e1e; width: 8px; }
    QScrollBar::handle:vertical {
        background: #3f3f46; border-radius: 4px; min-height: 20px;
    }
    QScrollBar::handle:vertical:hover { background: #0078d4; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    QGroupBox {
        border: 1px solid #3f3f46; border-radius: 4px;
        margin-top: 6px; font-size: 11px; color: #888888;
    }
    QGroupBox::title {
        subcontrol-origin: margin; subcontrol-position: top left;
        padding: 0 4px; left: 8px;
    }
    QDoubleSpinBox, QComboBox {
        background: #2d2d2d; border: 1px solid #3f3f46;
        border-radius: 3px; padding: 2px 4px; color: #cccccc;
    }
    QDoubleSpinBox:focus, QComboBox:focus { border-color: #0078d4; }
    QCheckBox { spacing: 6px; }
    QCheckBox::indicator {
        width: 14px; height: 14px; background: #2d2d2d;
        border: 1px solid #3f3f46; border-radius: 2px;
    }
    QCheckBox::indicator:checked { background: #0078d4; border-color: #0078d4; }
    QPushButton, QToolButton {
        background: #2d2d2d; border: 1px solid #3f3f46;
        border-radius: 3px; padding: 4px 10px; color: #cccccc;
        text-align: left;
    }
    QPushButton:hover, QToolButton:hover { background: #3f3f46; }
    QPushButton:pressed, QToolButton:pressed {
        background: #0078d4; border-color: #0078d4;
    }
    QLabel#sectionLabel { color: #888888; font-size: 11px; }
    QLabel#axisX { color: #e05555; font-weight: bold; }
    QLabel#axisY { color: #55cc55; font-weight: bold; }
    QLabel#axisZ { color: #5599ee; font-weight: bold; }
    QMenu {
        background: #2a2a2e; color: #d0d0d0;
        border: 1px solid #444; font-size: 12px;
    }
    QMenu::item { padding: 5px 20px 5px 12px; }
    QMenu::item:selected { background: #0e639c; }
)";

// ══════════════════════════════════════════════════════════════
//  DraggableButton
// ══════════════════════════════════════════════════════════════
DraggableButton::DraggableButton(const QString& label,
                                  const QString& mimePayload,
                                  QWidget* parent)
    : QPushButton(label, parent), m_payload(mimePayload)
{
    setCursor(Qt::OpenHandCursor);
}

void DraggableButton::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragStart = e->pos();
        m_dragging  = false;
    }
    QPushButton::mousePressEvent(e);
}

void DraggableButton::mouseMoveEvent(QMouseEvent* e)
{
    if (!(e->buttons() & Qt::LeftButton)) return;
    if (m_dragging) return;
    if ((e->pos() - m_dragStart).manhattanLength() < 8) return;

    m_dragging = true;
    setCursor(Qt::ClosedHandCursor);

    auto* drag = new QDrag(this);
    auto* mime = new QMimeData;
    mime->setData("application/x-primitive", m_payload.toUtf8());
    drag->setMimeData(mime);

    QPixmap pix(width(), height());
    pix.fill(Qt::transparent);
    QPainter painter(&pix);
    painter.setOpacity(0.75);
    render(&painter);
    painter.end();
    drag->setPixmap(pix);
    drag->setHotSpot(e->pos());

    drag->exec(Qt::CopyAction);
    setCursor(Qt::OpenHandCursor);
    m_dragging = false;
}

// ══════════════════════════════════════════════════════════════
//  ExplorerPanel — 三分组场景树
// ══════════════════════════════════════════════════════════════
ExplorerPanel::ExplorerPanel(QWidget* parent)
    : QWidget(parent)
{
    setStyleSheet(kPanelQss);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 搜索栏 ─────────────────────────────────────────────
    auto* searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(4, 4, 4, 4);
    searchRow->setSpacing(4);
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(tr("Search…"));
    searchRow->addWidget(m_searchBox);
    auto* filterBtn = new QToolButton(this);
    filterBtn->setText("⊞");
    filterBtn->setFixedWidth(24);
    searchRow->addWidget(filterBtn);
    root->addLayout(searchRow);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background:#3f3f46; max-height:1px;");
    root->addWidget(sep);

    // ── 场景树 ─────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(16);
    m_tree->setAnimated(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    root->addWidget(m_tree, 1);

    // ── 三个顶级分组（永不删除）─────────────────────────────
    auto makeGroup = [&](const QString& label, const QString& icon) {
        auto* item = new QTreeWidgetItem(m_tree, { icon + "  " + label });
        QFont f = item->font(0);
        f.setBold(true);
        item->setFont(0, f);
        item->setForeground(0, QColor("#888888"));
        item->setExpanded(true);
        return item;
    };

    m_groupObjects = makeGroup(tr("Scene Objects"), "⬜");
    m_groupLights  = makeGroup(tr("Scene Lights"),  "💡");
    m_groupLua     = makeGroup(tr("Scene Lua"),     "📜");

    // ── 搜索过滤 ────────────────────────────────────────────
    connect(m_searchBox, &QLineEdit::textChanged, this, [this](const QString& text) {
        std::function<void(QTreeWidgetItem*)> filter = [&](QTreeWidgetItem* item) {
            bool match = text.isEmpty() ||
                         item->text(0).contains(text, Qt::CaseInsensitive);
            for (int i = 0; i < item->childCount(); ++i) {
                filter(item->child(i));
                if (!item->child(i)->isHidden()) match = true;
            }
            item->setHidden(!match);
        };
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
            filter(m_tree->topLevelItem(i));
    });

    // ── 选中节点发出信号 ─────────────────────────────────────
    connect(m_tree, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem* item, int) {
                if (item == m_groupObjects ||
                    item == m_groupLights  ||
                    item == m_groupLua)     return;
                emit nodeSelected(item->text(0).trimmed());
            });

    // ── 右键菜单 ─────────────────────────────────────────────
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) {
                QTreeWidgetItem* item = m_tree->itemAt(pos);
                // 不允许对分组头操作
                if (!item ||
                    item == m_groupObjects ||
                    item == m_groupLights  ||
                    item == m_groupLua)     return;

                QMenu menu(this);
                menu.setStyleSheet(kPanelQss);
                QAction* actSel    = menu.addAction(tr("选中"));
                QAction* actDelete = menu.addAction(tr("删除"));

                QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
                const QString name = item->text(0).trimmed();
                if (chosen == actSel) {
                    emit nodeSelected(name);
                } else if (chosen == actDelete) {
                    // 从对应分组中移除
                    QTreeWidgetItem* parent = item->parent();
                    if (parent) parent->removeChild(item);
                    delete item;
                    emit deleteRequested(name);
                }
            });
}

void ExplorerPanel::addNode(const QString& name, ObjectKind kind)
{
    // 确定目标分组
    QTreeWidgetItem* group = nullptr;
    QString icon;
    switch (kind) {
    case ObjectKind::Light:  group = m_groupLights;  icon = "💡 "; break;
    case ObjectKind::Lua:    group = m_groupLua;     icon = "📜 "; break;
    default:                 group = m_groupObjects;  icon = "⬡ ";  break;
    }

    auto* item = new QTreeWidgetItem(group, { icon + name });
    group->setExpanded(true);
    m_tree->scrollToItem(item);
}

void ExplorerPanel::removeNode(const QString& name)
{
    QTreeWidgetItem* item = findItem(name);
    if (item) {
        QTreeWidgetItem* parent = item->parent();
        if (parent) parent->removeChild(item);
        delete item;
    }
}

void ExplorerPanel::clearNodes()
{
    // 只清除分组的子节点，保留三个顶级分组头
    for (auto* group : { m_groupObjects, m_groupLights, m_groupLua }) {
        while (group->childCount() > 0)
            delete group->takeChild(0);
    }
}

QTreeWidgetItem* ExplorerPanel::findItem(const QString& name) const
{
    // 在所有分组中递归搜索（子节点带图标前缀，用 trimmed 后的后半段匹配）
    std::function<QTreeWidgetItem*(QTreeWidgetItem*, const QString&)> find =
        [&](QTreeWidgetItem* node, const QString& n) -> QTreeWidgetItem* {
        for (int i = 0; i < node->childCount(); ++i) {
            QTreeWidgetItem* child = node->child(i);
            // 节点文本格式："⬡ Name"，取最后空格后的部分
            QString text = child->text(0);
            int idx = text.indexOf(' ');
            QString bare = (idx >= 0) ? text.mid(idx + 1).trimmed() : text.trimmed();
            if (bare == n) return child;
            auto* found = find(child, n);
            if (found) return found;
        }
        return nullptr;
    };

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto* found = find(m_tree->topLevelItem(i), name);
        if (found) return found;
    }
    return nullptr;
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

    auto* hint = new QLabel(tr("点击创建 · 拖入视图可定位放置"), this);
    hint->setStyleSheet("color:#555; font-size:10px; padding:0 0 4px 0;");
    hint->setWordWrap(true);
    root->addWidget(hint);

    // ── Primitives ────────────────────────────────────────
    auto* primGroup  = new QGroupBox(tr("Primitives"), this);
    auto* primLayout = new QVBoxLayout(primGroup);
    primLayout->setSpacing(3);
    for (const QString& name : { tr("Box"), tr("Sphere"), tr("Cylinder"),
                                   tr("Plane"), tr("Cone") }) {
        auto* btn = new DraggableButton("⬡  " + name, name, primGroup);
        connect(btn, &QPushButton::clicked, this, [this, name]() {
            emit createPrimitive(name);
        });
        primLayout->addWidget(btn);
    }
    root->addWidget(primGroup);

    // ── Lights ───────────────────────────────────────────
    auto* lightGroup  = new QGroupBox(tr("Lights"), this);
    auto* lightLayout = new QVBoxLayout(lightGroup);
    lightLayout->setSpacing(3);
    for (const QString& name : { tr("Point Light"), tr("Spot Light"),
                                   tr("Directional Light") }) {
        auto* btn = new DraggableButton("💡  " + name, name, lightGroup);
        connect(btn, &QPushButton::clicked, this, [this, name]() {
            emit createPrimitive(name);
        });
        lightLayout->addWidget(btn);
    }
    root->addWidget(lightGroup);

    // ── Camera ───────────────────────────────────────────
    auto* camGroup  = new QGroupBox(tr("Camera"), this);
    auto* camLayout = new QVBoxLayout(camGroup);
    camLayout->setSpacing(3);
    auto* camBtn = new DraggableButton("🎥  " + tr("Camera Unit"), "Camera", camGroup);
    connect(camBtn, &QPushButton::clicked, this, [this]() {
        emit createPrimitive("Camera");
    });
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

    m_titleLabel = new QLabel(tr("(no object selected)"), this);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setStyleSheet(
        "padding: 4px 8px; color:#888888; font-size:12px;"
        "border-bottom:1px solid #3f3f46; background:#2d2d2d;");
    root->addWidget(m_titleLabel);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    root->addWidget(m_scroll, 1);

    buildEmptyState();
}

void PropertyEditorPanel::bindViewport(Viewport3D* vp)
{
    m_viewport = vp;

    // 当 Viewport 内部修改了属性（如未来 gizmo 操作），刷新面板显示
    connect(vp, &Viewport3D::objectPropsChanged, this, [this](const QString& name) {
        if (name == m_currentNode) inspectNode(name);
    });
}

void PropertyEditorPanel::buildEmptyState()
{
    // 清空控件指针
    m_spPX = m_spPY = m_spPZ = nullptr;
    m_spRX = m_spRY = m_spRZ = nullptr;
    m_spSX = m_spSY = m_spSZ = nullptr;
    m_visCheck   = nullptr;
    m_layerCombo = nullptr;

    m_scroll->setWidget(nullptr);
    if (m_container) { delete m_container; m_container = nullptr; }

    auto* placeholder = new QLabel(tr("Nothing to show"), m_scroll);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("color:#555555; font-size:12px;");
    m_scroll->setWidget(placeholder);
    m_container = placeholder;
}

// ─── 创建带彩色轴标签的三轴 SafeSpinBox 行 ──────────────────
PropertyEditorPanel::Vec3Widgets
PropertyEditorPanel::makeVec3Row(QFormLayout* form,
                                  const QString& label,
                                  QVector3D value,
                                  double rangeMin, double rangeMax, double step)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(2);

    const QString axes[3]    = { "X", "Y", "Z" };
    const QString objNames[3] = { "axisX", "axisY", "axisZ" };
    SafeSpinBox* spins[3]    = {};

    for (int a = 0; a < 3; ++a) {
        auto* lbl = new QLabel(axes[a]);
        lbl->setObjectName(objNames[a]);
        lbl->setFixedWidth(12);
        lbl->setAlignment(Qt::AlignCenter);
        row->addWidget(lbl);

        auto* spin = new SafeSpinBox;
        spin->setRange(rangeMin, rangeMax);
        spin->setSingleStep(step);
        spin->setDecimals(3);
        spin->setFixedWidth(72);
        spin->setValue(a == 0 ? value.x() : a == 1 ? value.y() : value.z());
        row->addWidget(spin);
        spins[a] = spin;
    }

    form->addRow(label, row);
    return { spins[0], spins[1], spins[2] };
}

// ─── 填充/重建属性表单 ────────────────────────────────────────
void PropertyEditorPanel::rebuildForm(const QString& name, const ObjectProps& props)
{
    // 先断开旧容器与 ScrollArea 的连接，再安全销毁
    m_scroll->setWidget(nullptr);   // 解除所有权引用
    if (m_container) {
        delete m_container;         // 立即删除（不用 deleteLater）
        m_container = nullptr;
    }

    // 清空控件指针
    m_spPX = m_spPY = m_spPZ = nullptr;
    m_spRX = m_spRY = m_spRZ = nullptr;
    m_spSX = m_spSY = m_spSZ = nullptr;
    m_visCheck   = nullptr;
    m_layerCombo = nullptr;

    auto* container = new QWidget;
    container->setStyleSheet(kPanelQss);
    auto* form = new QFormLayout(container);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(5);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // ── Transform ─────────────────────────────────────────
    auto* transLabel = new QLabel(tr("Transform"), container);
    transLabel->setObjectName("sectionLabel");
    form->addRow(transLabel);

    // 坐标说明（原点 = 网格XZ交点 = 世界(0,0,0)）
    auto* originHint = new QLabel(
        tr("Origin: grid XZ center = (0, 0, 0)"), container);
    originHint->setStyleSheet("color:#444; font-size:10px; padding: 0 0 2px 0;");
    form->addRow("", originHint);

    auto posW = makeVec3Row(form, tr("Position"), props.position, -99999, 99999, 0.1);
    m_spPX = posW.x; m_spPY = posW.y; m_spPZ = posW.z;

    auto rotW = makeVec3Row(form, tr("Rotation °"), props.rotation, -360, 360, 1.0);
    m_spRX = rotW.x; m_spRY = rotW.y; m_spRZ = rotW.z;

    auto scaW = makeVec3Row(form, tr("Scale"), props.scale, 0.0001, 99999, 0.01);
    m_spSX = scaW.x; m_spSY = scaW.y; m_spSZ = scaW.z;

    // ── Visibility ────────────────────────────────────────
    auto* sep = new QFrame(container);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background:#3f3f46; max-height:1px; margin: 4px 0;");
    form->addRow(sep);

    auto* visLabel = new QLabel(tr("Visibility"), container);
    visLabel->setObjectName("sectionLabel");
    form->addRow(visLabel);

    m_visCheck = new QCheckBox(tr("Visible"), container);
    m_visCheck->setChecked(props.visible);
    form->addRow("", m_visCheck);

    // ── Layer ─────────────────────────────────────────────
    m_layerCombo = new QComboBox(container);
    m_layerCombo->addItems({ tr("Default"), tr("Layer 1"),
                              tr("Layer 2"), tr("Background") });
    m_layerCombo->setCurrentIndex(qBound(0, props.layer, 3));
    form->addRow(tr("Layer"), m_layerCombo);

    m_container = container;
    m_scroll->setWidget(container);

    // ── 连接所有控件 → applyToViewport ───────────────────
    // SafeSpinBox::commitValue 只在 Enter/Tab/失焦时发射，
    // 不存在中间状态触发，彻底解决键盘输入崩溃
    auto connectSpin = [&](SafeSpinBox* sp) {
        connect(sp, &SafeSpinBox::commitValue,
                this, [this](double) { if (!m_updating) applyToViewport(); });
    };
    for (auto* sp : { m_spPX, m_spPY, m_spPZ,
                      m_spRX, m_spRY, m_spRZ,
                      m_spSX, m_spSY, m_spSZ })
        connectSpin(sp);

    connect(m_visCheck, &QCheckBox::toggled, this, [this](bool) {
        if (!m_updating) applyToViewport();
    });
    connect(m_layerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { if (!m_updating) applyToViewport(); });
}

void PropertyEditorPanel::inspectNode(const QString& nodeName)
{
    // 防止 rebuildForm 触发 applyToViewport 时 m_viewport 还在更新
    m_updating = true;
    m_currentNode = nodeName;
    m_titleLabel->setText(nodeName);

    ObjectProps props;
    bool found = false;
    if (m_viewport) {
        found = m_viewport->getObjectProps(nodeName, props);
    }
    if (!found) {
        props.position = {0,0,0};
        props.rotation = {0,0,0};
        props.scale    = {1,1,1};
        props.visible  = true;
        props.layer    = 0;
    }

    rebuildForm(nodeName, props);
    m_updating = false;
}

void PropertyEditorPanel::applyToViewport()
{
    // 全套空指针 + 状态检查，任何一项不满足都安全退出
    if (m_updating)                    return;
    if (!m_viewport)                   return;
    if (m_currentNode.isEmpty())       return;
    if (!m_spPX || !m_spPY || !m_spPZ) return;
    if (!m_spRX || !m_spRY || !m_spRZ) return;
    if (!m_spSX || !m_spSY || !m_spSZ) return;

    ObjectProps props;
    props.position = { (float)m_spPX->value(),
                       (float)m_spPY->value(),
                       (float)m_spPZ->value() };
    props.rotation = { (float)m_spRX->value(),
                       (float)m_spRY->value(),
                       (float)m_spRZ->value() };
    props.scale    = { (float)m_spSX->value(),
                       (float)m_spSY->value(),
                       (float)m_spSZ->value() };
    props.visible  = m_visCheck   ? m_visCheck->isChecked()      : true;
    props.layer    = m_layerCombo ? m_layerCombo->currentIndex() : 0;

    // scale 不能为零，做最小值保护
    props.scale.setX(qMax(0.0001f, props.scale.x()));
    props.scale.setY(qMax(0.0001f, props.scale.y()));
    props.scale.setZ(qMax(0.0001f, props.scale.z()));

    m_viewport->setObjectProps(m_currentNode, props);
    emit propsChanged(m_currentNode);
}
