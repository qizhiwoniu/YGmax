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
#include <QKeyEvent>
#include <QFocusEvent>
#include <QTimer>
#include <QPointer>
#include <QDrag>
#include <QMimeData>
#include "Viewport3D.h"   // ObjectKind, ObjectProps

// 前向声明，供 SafeSpinBox::collectSiblings() 向上查找边界用
class PropertyEditorPanel;

// ═════════════════════════════════════════════════════════════
//  SafeSpinBox
//  • Enter/Return  → 立即 commit，焦点留在当前框
//  • Tab/Backtab   → 在9个框内循环跳转，commit 后跳焦点，
//                    阻止事件传播到窗口焦点系统
//  • 其他失焦      → singleShot(0) 延迟 commit，避免重入崩溃
//
//  修复说明：
//    Qt 处理 Tab 键时走的是 QWidget::event() → focusNextPrevChild()
//    这条路完全绕过 keyPressEvent，所以旧代码的 e->accept() 拦不住。
//    override focusNextPrevChild() 并返回 true 才能真正阻止 Tab
//    跑出 SpinBox 组。
// ═════════════════════════════════════════════════════════════
class SafeSpinBox : public QDoubleSpinBox
{
    Q_OBJECT
public:
    explicit SafeSpinBox(QWidget* parent = nullptr)
        : QDoubleSpinBox(parent)
    {
        setKeyboardTracking(false);
        setButtonSymbols(QAbstractSpinBox::NoButtons);
    }

signals:
    void commitValue(double v);

protected:
    // ── 核心修复：接管 Qt 焦点系统的 Tab 跳转 ──────────────────
    // Qt 在收到 Tab 键时，会在 QWidget::event() 里调用
    // focusNextPrevChild()，这条路完全绕过 keyPressEvent。
    // override 并返回 true 告诉 Qt "我自己处理了，别再往外跑"。
    //
    // 崩溃根因：emit commitValue() 可能触发链路：
    //   applyToViewport → inspectNode → rebuildForm → delete 所有 SpinBox
    // 此时仍在本函数栈帧内，siblings[next] 变成野指针。
    // 修复：
    //   1. emit 前用 QPointer 记住目标，emit 后检查是否还存活。
    //   2. 若目标仍存活，用 singleShot(0) 延迟 setFocus，
    //      彻底退出当前调用栈再跳焦点，避免任何重入。
    bool focusNextPrevChild(bool forward) override
    {
        QList<SafeSpinBox*> siblings = collectSiblings();
        int idx = siblings.indexOf(this);
        if (siblings.size() > 1 && idx >= 0) {
            int next = forward
                       ? (idx + 1) % siblings.size()
                       : (idx - 1 + siblings.size()) % siblings.size();

            // 用 QPointer 保护目标：若 emit 触发 rebuildForm 把控件
            // 全部销毁，nextGuard 会自动变 nullptr，不会访问野指针。
            QPointer<SafeSpinBox> nextGuard(siblings[next]);

            m_tabbing = true;
            interpretText();
            emit commitValue(value());
            // emit 之后 this 和 siblings[next] 都可能已被销毁，
            // 必须通过 QPointer 检查才能安全使用。
            m_tabbing = false;

            if (nextGuard) {
                // singleShot(0)：退出整个调用栈后再切焦点，
                // 防止在 focusNextPrevChild 栈帧内触发新一轮重建。
                QTimer::singleShot(0, nextGuard, [nextGuard]() {
                    if (nextGuard) {
                        nextGuard->setFocus(Qt::TabFocusReason);
                        nextGuard->selectAll();
                    }
                });
            }
            return true;   // 告知 Qt：已处理，不要再向外传递
        }
        return QDoubleSpinBox::focusNextPrevChild(forward);
    }

    void keyPressEvent(QKeyEvent* e) override
    {
        // Tab/Backtab 由 focusNextPrevChild() 接管，这里不再处理。
        // （Qt 会先触发 focusNextPrevChild，通常不会到达这里，
        //   但保留 accept 作为双重保险）
        if (e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab) {
            e->accept();
            return;
        }

        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            QDoubleSpinBox::keyPressEvent(e);
            interpretText();
            emit commitValue(value());
            return;
        }
        QDoubleSpinBox::keyPressEvent(e);
    }

    void focusOutEvent(QFocusEvent* e) override
    {
        QDoubleSpinBox::focusOutEvent(e);
        if (m_tabbing) return;   // Tab 内部跳转已经 commit，跳过
        interpretText();
        double v = value();
        QPointer<SafeSpinBox> guard(this);
        QTimer::singleShot(0, this, [this, guard, v]() {
            if (guard) emit commitValue(v);
        });
    }

private:
    bool m_tabbing = false;

    // ── sibling 查找：向上找 PropertyEditorPanel 作为边界 ──────
    // 比旧代码用 QScrollArea 判断更可靠，不会因层级变化找错 root。
    QList<SafeSpinBox*> collectSiblings() const
    {
        QWidget* root = parentWidget();
        while (root) {
            if (qobject_cast<PropertyEditorPanel*>(root)) break;
            if (!root->parentWidget()) break;
            root = root->parentWidget();
        }
        if (!root) return {};
        return root->findChildren<SafeSpinBox*>(
            QString(), Qt::FindChildrenRecursively);
    }
};

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
    struct Vec3Widgets { SafeSpinBox* x; SafeSpinBox* y; SafeSpinBox* z; };
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
    SafeSpinBox* m_spPX = nullptr;
    SafeSpinBox* m_spPY = nullptr;
    SafeSpinBox* m_spPZ = nullptr;
    // Rotation
    SafeSpinBox* m_spRX = nullptr;
    SafeSpinBox* m_spRY = nullptr;
    SafeSpinBox* m_spRZ = nullptr;
    // Scale
    SafeSpinBox* m_spSX = nullptr;
    SafeSpinBox* m_spSY = nullptr;
    SafeSpinBox* m_spSZ = nullptr;
    // Visibility / Layer
    QCheckBox* m_visCheck   = nullptr;
    QComboBox* m_layerCombo = nullptr;

    bool m_updating = false;   ///< 防止信号循环

    void applyToViewport();    ///< 读取所有控件值 → setObjectProps
};
