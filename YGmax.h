#pragma once

#include <QtWidgets/QWidget>
#include <QMainWindow>
#include <QDockWidget>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QMouseEvent>
#include <QCursor>
#include "ui_YGmax.h"
#include "ToolBar.h"
#include "CustomTitleBar.h"
#include "SystemTrayIcon.h"
#include "DocumentTabBar.h"
#include "BottomPanel.h"
#include "RightPanel.h"
#include "Viewport3D.h"

class YGmax : public QWidget
{
    Q_OBJECT

public:
    YGmax(QWidget* parent = nullptr);
    ~YGmax();

protected:
    void paintEvent(QPaintEvent* event)     override;
    void resizeEvent(QResizeEvent* event)   override;
    void closeEvent(QCloseEvent* event)     override;
    void mousePressEvent(QMouseEvent* event)   override;
    void mouseMoveEvent(QMouseEvent* event)    override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupMenuBar();
    void setupTray();
    void setupDockHost();
    void applyDockStyle();
    void saveLayout();
    void restoreLayout();

    // ── 无边框窗口缩放 ───────────────────────────────────────
    static const int kEdge = 6;   // 感应边缘宽度（px）

    enum ResizeDir {
        None = 0,
        Left = 1, Right = 2, Top = 4, Bottom = 8,
        TopLeft     = Top    | Left,
        TopRight    = Top    | Right,
        BottomLeft  = Bottom | Left,
        BottomRight = Bottom | Right
    };

    ResizeDir hitTest(const QPoint& pos) const;
    void      updateCursor(ResizeDir dir);

    bool       m_resizing    = false;
    ResizeDir  m_resizeDir   = None;
    QPoint     m_resizeStart;      // 全局坐标，拖拽起点
    QRect      m_resizeOrigGeom;   // 拖拽起点时的窗口几何

    // ── 顶部固定区域 ─────────────────────────────────────────
    QAction*        m_newDocAct     = nullptr;
    CustomTitleBar* m_titleBar      = nullptr;
    ToolBar*        m_toolBar       = nullptr;
    DocumentTabBar* m_tabBar        = nullptr;
    SystemTrayIcon* m_tray          = nullptr;

    // ── 内嵌 QMainWindow，负责所有可停靠面板 ─────────────────
    QMainWindow*    m_dockHost      = nullptr;
    Viewport3D*     m_viewport      = nullptr;  // 3-D Viewport (centralWidget)

    // ── 底部面板 Dock ────────────────────────────────────────
    QDockWidget*       m_dockAsset   = nullptr;
    QDockWidget*       m_dockLog     = nullptr;
    QDockWidget*       m_dockPreview = nullptr;

    AssetBrowserPanel* m_assetPanel   = nullptr;
    LogConsolePanel*   m_logPanel     = nullptr;
    AssetPreviewPanel* m_previewPanel = nullptr;

    // ── 右侧面板 Dock ────────────────────────────────────────
    QDockWidget*        m_dockExplorer    = nullptr;  // Explorer + Create (tabbed)
    QDockWidget*        m_dockCreate      = nullptr;
    QDockWidget*        m_dockProperty    = nullptr;  // Property Editor

    ExplorerPanel*      m_explorerPanel   = nullptr;
    CreatePanel*        m_createPanel     = nullptr;
    PropertyEditorPanel* m_propertyPanel  = nullptr;
};
