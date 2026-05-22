#pragma once
#include <QWidget>
#include <QSplitter>
#include <QTabBar>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

// ─────────────────────────────────────────────────────────────
//  AssetBrowserPanel   左：文件夹树   右：文件列表
// ─────────────────────────────────────────────────────────────
#include <QDir>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QCoreApplication>
#include <QDateTime>

class AssetBrowserPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AssetBrowserPanel(QWidget* parent = nullptr);

signals:
    void assetSelected(const QString& path);

private slots:
    void onFolderSelected(QTreeWidgetItem* cur, QTreeWidgetItem* prev);
    void onSearchChanged(const QString& text);
    void onFileClicked(QTableWidgetItem* item);

private:
    void applyStyle();
    void setupFolderTree();
    void setupFileList();

    // 返回 exe 同级的 Media 根目录，不存在则自动创建示例结构
    static QString mediaRoot();
    static void    ensureMediaStructure(const QString& mediaPath);

    // 把 dirPath 下内容填充到 m_fileList（含搜索过滤）
    void populateFileList(const QString& dirPath,
                          const QString& filter = {});

    // 递归构建目录树节点
    QTreeWidgetItem* buildTreeNode(QTreeWidgetItem* parent,
                                   const QString&   dirPath,
                                   bool             expanded = false);

    QSplitter*    m_splitter   = nullptr;
    QTreeWidget*  m_folderTree = nullptr;
    QTableWidget* m_fileList   = nullptr;
    QLineEdit*    m_searchBar  = nullptr;
    QLabel*       m_pathLabel  = nullptr;

    QString       m_currentDir;  // 当前显示的目录路径
};

// ─────────────────────────────────────────────────────────────
//  LogConsolePanel
// ─────────────────────────────────────────────────────────────
class LogConsolePanel : public QWidget
{
    Q_OBJECT
public:
    explicit LogConsolePanel(QWidget* parent = nullptr);

    // level: 0=info  1=warn  2=error
    void appendMessage(const QString& msg, int level = 0);
    void clear();

private:
    void applyStyle();

    QTextEdit*   m_output   = nullptr;
    QPushButton* m_clearBtn = nullptr;
};

// ─────────────────────────────────────────────────────────────
//  AssetPreviewPanel
// ─────────────────────────────────────────────────────────────
class AssetPreviewPanel : public QWidget
{
    Q_OBJECT
public:
    explicit AssetPreviewPanel(QWidget* parent = nullptr);

public slots:
    void previewAsset(const QString& path);

private:
    void applyStyle();

    QLabel* m_previewLabel = nullptr;
    QLabel* m_infoLabel    = nullptr;
    QLabel* m_noSelLabel   = nullptr;
};

// ─────────────────────────────────────────────────────────────
//  BottomPanel   顶层容器：左侧 TabBar + 右侧预览
// ─────────────────────────────────────────────────────────────
class BottomPanel : public QWidget
{
    Q_OBJECT
public:
    explicit BottomPanel(QWidget* parent = nullptr);

    LogConsolePanel*   logConsole()   const { return m_logConsole; }
    AssetBrowserPanel* assetBrowser() const { return m_assetBrowser; }

private:
    void applyStyle();

    QTabBar*         m_tabBar      = nullptr;
    QStackedWidget*  m_stack       = nullptr;
    QSplitter*       m_splitter    = nullptr;

    AssetBrowserPanel* m_assetBrowser = nullptr;
    LogConsolePanel*   m_logConsole   = nullptr;
    AssetPreviewPanel* m_assetPreview = nullptr;
};
