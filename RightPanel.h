#pragma once

// ─────────────────────────────────────────────────────────────
//  RightPanel.h
//  右侧三个面板：
//    ExplorerPanel       — 场景树，分三组：
//                          ├ Scene Objects（普通几何体）
//                          ├ Scene Lights （灯光）
//                          └ Scene Lua    （Lua 脚本实体）
//    CreatePanel         — 资产创建（可拖拽到 Viewport3D）
//    PropertyEditorPanel — 属性编辑器（双向绑定 Viewport3D）
// ─────────────────────────────────────────────────────────────

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QScrollArea>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include "Viewport3D.h"   // ObjectKind, ObjectProps

// ═════════════════════════════════════════════════════════════
//  DraggableButton — 按下后可拖拽，携带 MIME 类型字符串
// ═════════════════════════════════════════════════════════════
class DraggableButton : public QPushButton
{
    Q_OBJECT
public:
    explicit DraggableButton(const QString& label,
                              const QString& mimePayload,
                              QWidget* parent = nullptr);

protected:
    void mousePressEvent (QMouseEvent* e) override;
    void mouseMoveEvent  (QMouseEvent* e) override;

private:
    QString m_payload;
    QPoint  m_dragStart;
    bool    m_dragging = false;
};

// ═════════════════════════════════════════════════════════════
//  ExplorerPanel — 场景树（三分组）
// ═════════════════════════════════════════════════════════════
class ExplorerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ExplorerPanel(QWidget* parent = nullptr);

public slots:
    /// Viewport objectAdded(name, kind) → 按 kind 分组插入
    void addNode   (const QString& name, ObjectKind kind);
    void removeNode(const QString& name);
    void clearNodes();

signals:
    void nodeSelected    (const QString& name);  ///< 选中节点 → PropertyEditor
    void deleteRequested (const QString& name);  ///< 右键删除 → Viewport

private:
    QLineEdit*   m_searchBox   = nullptr;
    QTreeWidget* m_tree        = nullptr;

    /// 三个顶级分组节点（永远存在，不可删除）
    QTreeWidgetItem* m_groupObjects = nullptr;  // Scene Objects
    QTreeWidgetItem* m_groupLights  = nullptr;  // Scene Lights
    QTreeWidgetItem* m_groupLua     = nullptr;  // Scene Lua

    /// 按名称在全树中找节点（跨分组搜索）
    QTreeWidgetItem* findItem(const QString& name) const;
};

// ═════════════════════════════════════════════════════════════
//  CreatePanel — 资产创建向导
// ═════════════════════════════════════════════════════════════
class CreatePanel : public QWidget
{
    Q_OBJECT

public:
    explicit CreatePanel(QWidget* parent = nullptr);

signals:
    void createPrimitive(const QString& type);
};

// ═════════════════════════════════════════════════════════════
//  PropertyEditorPanel — 属性编辑器（双向绑定 Viewport3D）
// ═════════════════════════════════════════════════════════════
class PropertyEditorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyEditorPanel(QWidget* parent = nullptr);

    /// 绑定 Viewport，使编辑框可以回写场景数据
    void bindViewport(Viewport3D* vp);

public slots:
    /// Explorer 选中节点 / Viewport objectSelected → 填充属性
    void inspectNode(const QString& nodeName);

signals:
    /// 用户在面板上修改了属性，已通过 setObjectProps 写回 Viewport
    void propsChanged(const QString& name);

private:
    void buildEmptyState();
    void rebuildForm(const QString& name, const ObjectProps& props);

    // ── 工具：创建带彩色前缀标签的三轴行 ─────────────────────
    struct Vec3Widgets { QDoubleSpinBox* x; QDoubleSpinBox* y; QDoubleSpinBox* z; };
    Vec3Widgets makeVec3Row(QFormLayout* form, const QString& label,
                             QVector3D value,
                             double rangeMin, double rangeMax, double step);

    // ── 状态 ──────────────────────────────────────────────────
    Viewport3D* m_viewport    = nullptr;
    QString     m_currentNode;

    QScrollArea* m_scroll     = nullptr;
    QWidget*     m_container  = nullptr;
    QLabel*      m_titleLabel = nullptr;

    // ── 当前选中物体的各编辑控件（用于程序化更新值）──────────
    // Position
    QDoubleSpinBox* m_spPX = nullptr;
    QDoubleSpinBox* m_spPY = nullptr;
    QDoubleSpinBox* m_spPZ = nullptr;
    // Rotation
    QDoubleSpinBox* m_spRX = nullptr;
    QDoubleSpinBox* m_spRY = nullptr;
    QDoubleSpinBox* m_spRZ = nullptr;
    // Scale
    QDoubleSpinBox* m_spSX = nullptr;
    QDoubleSpinBox* m_spSY = nullptr;
    QDoubleSpinBox* m_spSZ = nullptr;
    // Visibility / Layer
    QCheckBox* m_visCheck   = nullptr;
    QComboBox* m_layerCombo = nullptr;

    bool m_updating = false;   ///< 防止信号循环

    void applyToViewport();    ///< 读取所有控件值 → setObjectProps
};
