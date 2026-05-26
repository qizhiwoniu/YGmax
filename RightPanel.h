#pragma once

// ─────────────────────────────────────────────────────────────
//  RightPanel.h
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
#include <memory>
#include "Viewport3D.h"

// ═════════════════════════════════════════════════════════════
//  SafeSpinBox
//
//  Tab 跳转设计（最终方案）：
//
//  1. focusNextPrevChild() 拦截 Tab，立即 setFocus 到下一个框。
//     不在这里 emit commitValue，避免 emit→rebuildForm→delete 的
//     重入崩溃。值的提交完全交给 focusOutEvent 处理。
//
//  2. focusOutEvent 在焦点真正离开时 commit，用 singleShot(0)
//     延迟发射以避免 Qt 内部重入。m_tabbing 标志在 Tab 跳转时
//     置为 true，防止 focusOutEvent 重复提交。
//
//  3. Tab 列表用"注入式"：PropertyEditorPanel 在 rebuildForm
//     后调用 setTabGroup() 把9个有序指针注入进来。
//     不用 parentWidget() 爬树，因为 QScrollArea::setWidget()
//     会把 container 的 parent 强制改成内部 viewport()，爬树
//     永远找不到 PropertyEditorPanel。
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

    using TabGroup = QList<SafeSpinBox*>;

    /// rebuildForm 后调用，注入有序的9个兄弟指针
    void setTabGroup(std::shared_ptr<TabGroup> group)
    {
        m_tabGroup = std::move(group);
    }

signals:
    void commitValue(double v);

protected:
    // Qt 收到 Tab 时走 event()→focusNextPrevChild()，绕过 keyPressEvent。
    // 返回 true = 我接管了，Qt 不再向外传递焦点。
    bool focusNextPrevChild(bool forward) override
    {
        if (!m_tabGroup || m_tabGroup->size() < 2)
            return QDoubleSpinBox::focusNextPrevChild(forward);

        int idx = m_tabGroup->indexOf(this);
        if (idx < 0)
            return QDoubleSpinBox::focusNextPrevChild(forward);

        int next = forward
                   ? (idx + 1) % m_tabGroup->size()
                   : (idx - 1 + m_tabGroup->size()) % m_tabGroup->size();

        SafeSpinBox* target = (*m_tabGroup)[next];

        // m_tabbing=true：告诉 focusOutEvent 不要再 commit
        // （焦点马上要离开，但我们不想在这里触发 applyToViewport，
        //   因为 target->setFocus() 会触发本控件的 focusOutEvent，
        //   那时再 commit 即可）
        m_tabbing = true;
        target->setFocus(Qt::TabFocusReason);  // 立即跳，不延迟
        target->selectAll();
        // 注意：target->setFocus() 会同步触发本控件的 focusOutEvent，
        // 但 m_tabbing==true 所以不会重复 commit。
        // focusOutEvent 返回后我们才到这里。
        m_tabbing = false;

        return true;  // 告知 Qt：已处理，不向外传递
    }

    void keyPressEvent(QKeyEvent* e) override
    {
        // Tab 由 focusNextPrevChild 接管，此处只做双重保险
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
        if (m_tabbing) return;  // Tab 跳转中，跳过（由目标框的 focusIn 后续处理）
        interpretText();
        double v = value();
        QPointer<SafeSpinBox> guard(this);
        QTimer::singleShot(0, this, [this, guard, v]() {
            if (guard) emit commitValue(v);
        });
    }

private:
    bool m_tabbing = false;
    std::shared_ptr<TabGroup> m_tabGroup;
};

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
//  DraggableButton
// ═════════════════════════════════════════════════════════════
class DraggableButton : public QPushButton
{
    Q_OBJECT
public:
    explicit DraggableButton(const QString& label,
                              const QString& mimePayload,
                              QWidget* parent = nullptr);
protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent (QMouseEvent* e) override;
private:
    QString m_payload;
    QPoint  m_dragStart;
    bool    m_dragging = false;
};

// ═════════════════════════════════════════════════════════════
//  ExplorerPanel
// ═════════════════════════════════════════════════════════════
class ExplorerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ExplorerPanel(QWidget* parent = nullptr);
public slots:
    void addNode   (const QString& name, ObjectKind kind);
    void removeNode(const QString& name);
    void clearNodes();
signals:
    void nodeSelected    (const QString& name);
    void deleteRequested (const QString& name);
private:
    QLineEdit*   m_searchBox    = nullptr;
    QTreeWidget* m_tree         = nullptr;
    QTreeWidgetItem* m_groupObjects = nullptr;
    QTreeWidgetItem* m_groupLights  = nullptr;
    QTreeWidgetItem* m_groupLua     = nullptr;
    QTreeWidgetItem* findItem(const QString& name) const;
};

// ═════════════════════════════════════════════════════════════
//  CreatePanel
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
//  PropertyEditorPanel
// ═════════════════════════════════════════════════════════════
class PropertyEditorPanel : public QWidget
{
    Q_OBJECT
public:
    explicit PropertyEditorPanel(QWidget* parent = nullptr);
    void bindViewport(Viewport3D* vp);
public slots:
    void inspectNode(const QString& nodeName);
signals:
    void propsChanged(const QString& name);
private:
    void buildEmptyState();
    void rebuildForm(const QString& name, const ObjectProps& props);

    struct Vec3Widgets { SafeSpinBox* x; SafeSpinBox* y; SafeSpinBox* z; };
    Vec3Widgets makeVec3Row(QFormLayout* form, const QString& label,
                             QVector3D value,
                             double rangeMin, double rangeMax, double step);

    Viewport3D*  m_viewport    = nullptr;
    QString      m_currentNode;
    QScrollArea* m_scroll      = nullptr;
    QWidget*     m_container   = nullptr;
    QLabel*      m_titleLabel  = nullptr;

    SafeSpinBox* m_spPX = nullptr;  SafeSpinBox* m_spPY = nullptr;  SafeSpinBox* m_spPZ = nullptr;
    SafeSpinBox* m_spRX = nullptr;  SafeSpinBox* m_spRY = nullptr;  SafeSpinBox* m_spRZ = nullptr;
    SafeSpinBox* m_spSX = nullptr;  SafeSpinBox* m_spSY = nullptr;  SafeSpinBox* m_spSZ = nullptr;

    QCheckBox* m_visCheck   = nullptr;
    QComboBox* m_layerCombo = nullptr;

    bool m_updating = false;

    // 9个 SpinBox 的有序 Tab 列表，rebuildForm 后注入每个 SpinBox
    std::shared_ptr<SafeSpinBox::TabGroup> m_tabGroup;

    void applyToViewport();
};
