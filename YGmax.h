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
#include <QStackedWidget>
#include <QPlainTextEdit>
#include "ui/ToolBar.h"
#include "ui/CustomTitleBar.h"
#include "SystemTrayIcon.h"
#include "ui/DocumentTabBar.h"
#include "ui/BottomPanel.h"
#include "ui/RightPanel.h"
#include "Viewport3D.h"
#include "SceneSerializer.h"   // ← 新增：SceneSnapshot / SerializeError

class SceneRunnerWidget;

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
    // ── 布局 / 初始化 ─────────────────────────────────────────
    void setupMenuBar();
    void setupTray();
    void setupDockHost();
    void applyDockStyle();
    void saveLayout();
    void restoreLayout();

    // ── 文件操作 ──────────────────────────────────────────────
    /// Ctrl+S 统一入口：根据当前 Tab 类型自动分发
    void quickSave();

    /// 保存场景 (.ygx)；saveAs=false 时复用已有路径（快速保存）
    void saveScene(bool saveAs = false);

    /// 打开场景文件 (.ygx)
    void openScene();

    /// 保存 Lua 脚本 (.lvl)；saveAs=false 时复用已有路径
    void saveLuaScript(bool saveAs = false);

    /// 打开 Lua 脚本 (.lvl)
    void openLuaScript();

    /// 统一"打开"对话框，按扩展名分发给 openScene / openLuaScript
    void openFile();

    // ── 无边框窗口缩放 ────────────────────────────────────────
    static const int kEdge = 6;

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

    bool       m_resizing      = false;
    ResizeDir  m_resizeDir     = None;
    QPoint     m_resizeStart;
    QRect      m_resizeOrigGeom;

    SceneRunnerWidget* m_inlineRunner = nullptr;

    // ── 文件路径状态 ──────────────────────────────────────────
    QString m_currentScenePath;   ///< 当前场景的磁盘路径（空=未保存）
    QString m_currentLuaPath;     ///< 当前 Lua 脚本的磁盘路径（空=未保存）

    // ── 顶部固定区域 ──────────────────────────────────────────
    QStackedWidget* m_stack      = nullptr;
    QPlainTextEdit* m_textEditor = nullptr;

    QAction*        m_newDocAct  = nullptr;
    QAction*        m_newLua     = nullptr;
    CustomTitleBar* m_titleBar   = nullptr;
    ToolBar*        m_toolBar    = nullptr;
    DocumentTabBar* m_tabBar     = nullptr;
    SystemTrayIcon* m_tray       = nullptr;

    // ── 内嵌 QMainWindow ──────────────────────────────────────
    QMainWindow* m_dockHost  = nullptr;
    Viewport3D*  m_viewport  = nullptr;

    // ── 底部 Dock ─────────────────────────────────────────────
    QDockWidget*       m_dockAsset   = nullptr;
    QDockWidget*       m_dockLog     = nullptr;
    QDockWidget*       m_dockPreview = nullptr;

    AssetBrowserPanel* m_assetPanel   = nullptr;
    LogConsolePanel*   m_logPanel     = nullptr;
    AssetPreviewPanel* m_previewPanel = nullptr;

    // ── 右侧 Dock ─────────────────────────────────────────────
    QDockWidget*         m_dockExplorer  = nullptr;
    QDockWidget*         m_dockCreate    = nullptr;
    QDockWidget*         m_dockProperty  = nullptr;

    ExplorerPanel*       m_explorerPanel  = nullptr;
    CreatePanel*         m_createPanel    = nullptr;
    PropertyEditorPanel* m_propertyPanel  = nullptr;
};
