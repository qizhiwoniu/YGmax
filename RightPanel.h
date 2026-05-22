#pragma once

// ─────────────────────────────────────────────────────────────
//  RightPanel.h
//  右侧三个面板的声明：
//    ExplorerPanel      — 场景树（对标 Stingray Explorer）
//    CreatePanel        — 资产创建向导（对标 Stingray Create）
//    PropertyEditorPanel — 属性编辑器（对标 Stingray Property Editor）
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

// ═════════════════════════════════════════════════════════════
//  ExplorerPanel — 场景树
// ═════════════════════════════════════════════════════════════
class ExplorerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ExplorerPanel(QWidget* parent = nullptr);

signals:
    /// 用户在树中选中某节点时发出，传出节点名称（供 Property Editor 使用）
    void nodeSelected(const QString& nodeName);

private:
    void buildDefaultScene();

    QLineEdit*   m_searchBox  = nullptr;
    QTreeWidget* m_tree       = nullptr;
};

// ═════════════════════════════════════════════════════════════
//  CreatePanel — 资产创建向导
// ═════════════════════════════════════════════════════════════
class CreatePanel : public QWidget
{
    Q_OBJECT

public:
    explicit CreatePanel(QWidget* parent = nullptr);
};

// ═════════════════════════════════════════════════════════════
//  PropertyEditorPanel — 属性编辑器
// ═════════════════════════════════════════════════════════════
class PropertyEditorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyEditorPanel(QWidget* parent = nullptr);

public slots:
    /// 接收 ExplorerPanel::nodeSelected，显示对应属性
    void inspectNode(const QString& nodeName);

private:
    void buildEmptyState();
    void buildNodeProperties(const QString& nodeName);

    QScrollArea* m_scroll     = nullptr;
    QWidget*     m_container  = nullptr;
    QLabel*      m_titleLabel = nullptr;
};
