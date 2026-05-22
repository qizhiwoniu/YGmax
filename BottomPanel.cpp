#include "stdafx.h"
#include "BottomPanel.h"
#include <QHeaderView>
#include <QScrollBar>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QCoreApplication>

// ═══════════════════════════════════════════════════════════════
//  AssetBrowserPanel  —  真实文件系统版
// ═══════════════════════════════════════════════════════════════

// ── 示例文件列表（每个子目录写入一批空文件作为占位资产）──────
static const QMap<QString, QStringList> kSampleFiles = {
    { "audio",     { "ambient_wind.wav", "footstep_grass.wav",
                     "explosion_01.wav", "music_main_theme.ogg",
                     "ui_click.wav"    } },
    { "levels",    { "level_01.lvl",    "level_02.lvl",
                     "tutorial.lvl",    "sandbox.lvl"          } },
    { "models",    { "player_hero.fbx", "enemy_goblin.fbx",
                     "tree_pine.fbx",   "rock_large.fbx",
                     "chest.fbx",       "barrel.fbx"           } },
    { "materials", { "stone_wall.mat",  "wood_floor.mat",
                     "metal_rust.mat",  "grass_ground.mat",
                     "water_surface.mat"                        } },
};

// ── 根目录：exe 同级的 Media 文件夹 ─────────────────────────
QString AssetBrowserPanel::mediaRoot()
{
    return QCoreApplication::applicationDirPath() + "/Media";
}

void AssetBrowserPanel::ensureMediaStructure(const QString& mediaPath)
{
    QDir root(mediaPath);
    if (!root.exists()) root.mkpath(".");

    for (auto it = kSampleFiles.cbegin(); it != kSampleFiles.cend(); ++it) {
        QDir sub(mediaPath + "/" + it.key());
        if (!sub.exists()) sub.mkpath(".");
        for (const QString& fname : it.value()) {
            QFile f(sub.filePath(fname));
            if (!f.exists()) { f.open(QIODevice::WriteOnly); }
        }
    }
}

// ── 构造 ─────────────────────────────────────────────────────
AssetBrowserPanel::AssetBrowserPanel(QWidget* parent)
    : QWidget(parent)
{
    // 确保 Media 目录及示例文件存在
    ensureMediaStructure(mediaRoot());
    m_currentDir = mediaRoot();

    // ── 顶部工具栏 ────────────────────────────────────────────
    m_pathLabel = new QLabel("Media", this);
    m_pathLabel->setObjectName("pathLabel");

    m_searchBar = new QLineEdit(this);
    m_searchBar->setObjectName("assetSearch");
    m_searchBar->setPlaceholderText(tr("搜索"));
    m_searchBar->setFixedWidth(180);

    auto* btnBack = new QPushButton("←", this);
    auto* btnFwd  = new QPushButton("→", this);
    btnBack->setObjectName("navBtn");
    btnFwd->setObjectName("navBtn");
    btnBack->setFixedSize(20, 20);
    btnFwd->setFixedSize(20, 20);

    auto* topBar = new QHBoxLayout;
    topBar->setContentsMargins(6, 2, 6, 2);
    topBar->setSpacing(6);
    topBar->addWidget(btnBack);
    topBar->addWidget(btnFwd);
    topBar->addWidget(m_pathLabel, 1);
    topBar->addWidget(m_searchBar);

    // 导航按钮暂作上一级
    connect(btnBack, &QPushButton::clicked, this, [this]() {
        QDir d(m_currentDir);
        if (d.cdUp() && d.absolutePath().length() >=
                        mediaRoot().length()) {
            m_currentDir = d.absolutePath();
            // 更新路径标签（显示相对 Media 的部分）
            QString rel = QDir(mediaRoot()).relativeFilePath(m_currentDir);
            m_pathLabel->setText(rel.isEmpty() ? "Media" : "Media / " + rel);
            populateFileList(m_currentDir, m_searchBar->text());
        }
    });

    // ── 内容区 ────────────────────────────────────────────────
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(1);

    setupFolderTree();
    setupFileList();

    m_splitter->addWidget(m_folderTree);
    m_splitter->addWidget(m_fileList);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({ 160, 600 });

    auto* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);
    vlay->addLayout(topBar);
    vlay->addWidget(m_splitter, 1);
    setLayout(vlay);

    // ── 信号连接 ──────────────────────────────────────────────
    connect(m_searchBar, &QLineEdit::textChanged,
            this, &AssetBrowserPanel::onSearchChanged);

    applyStyle();

    // 默认展示 Media 根目录（只有子目录，无文件）
    populateFileList(m_currentDir);
}

// ── 构建左侧目录树 ────────────────────────────────────────────
void AssetBrowserPanel::setupFolderTree()
{
    m_folderTree = new QTreeWidget(this);
    m_folderTree->setObjectName("folderTree");
    m_folderTree->setHeaderHidden(true);
    m_folderTree->setIndentation(14);
    m_folderTree->setRootIsDecorated(true);
    m_folderTree->setAnimated(true);

    // 根节点 = Debug（exe 目录名）
    QString exeDir  = QCoreApplication::applicationDirPath();
    QString dirName = QDir(exeDir).dirName();   // "Debug" 或 "x64" 等

    auto* root = new QTreeWidgetItem(m_folderTree, { dirName });
    root->setData(0, Qt::UserRole, exeDir);
    root->setExpanded(true);

    // Media 节点
    QTreeWidgetItem* mediaNode = buildTreeNode(root, mediaRoot(), true);
    Q_UNUSED(mediaNode)

    m_folderTree->setCurrentItem(root);

    connect(m_folderTree, &QTreeWidget::currentItemChanged,
            this, &AssetBrowserPanel::onFolderSelected);
}

// 递归构建目录树
QTreeWidgetItem* AssetBrowserPanel::buildTreeNode(
        QTreeWidgetItem* parent,
        const QString&   dirPath,
        bool             expanded)
{
    QDir dir(dirPath);
    auto* node = new QTreeWidgetItem(parent, { dir.dirName() });
    node->setData(0, Qt::UserRole, dirPath);
    node->setExpanded(expanded);

    // 只递归子目录
    for (const QFileInfo& fi :
         dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    {
        buildTreeNode(node, fi.absoluteFilePath(), false);
    }
    return node;
}

// ── 构建右侧文件表格 ──────────────────────────────────────────
void AssetBrowserPanel::setupFileList()
{
    m_fileList = new QTableWidget(this);
    m_fileList->setObjectName("fileList");
    m_fileList->setColumnCount(4);
    m_fileList->setHorizontalHeaderLabels(
        { tr("名称"), tr("类型"), tr("大小"), tr("修改日期") });
    m_fileList->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    m_fileList->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    m_fileList->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    m_fileList->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    m_fileList->verticalHeader()->setVisible(false);
    m_fileList->verticalHeader()->setDefaultSectionSize(22);
    m_fileList->setShowGrid(false);
    m_fileList->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileList->setAlternatingRowColors(false);

    connect(m_fileList, &QTableWidget::itemClicked,
            this, &AssetBrowserPanel::onFileClicked);
}

// ── 填充右侧文件列表 ──────────────────────────────────────────
void AssetBrowserPanel::populateFileList(const QString& dirPath,
                                          const QString& filter)
{
    m_fileList->setRowCount(0);
    QDir dir(dirPath);

    // 先列子目录
    QFileInfoList entries =
        dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    // 再列文件
    entries += dir.entryInfoList(QDir::Files, QDir::Name);

    for (const QFileInfo& fi : entries) {
        if (!filter.isEmpty() &&
            !fi.fileName().contains(filter, Qt::CaseInsensitive))
            continue;

        int row = m_fileList->rowCount();
        m_fileList->insertRow(row);

        QString typeName = fi.isDir()
            ? tr("[文件夹]")
            : fi.suffix().isEmpty() ? tr("文件") : fi.suffix();

        QString sizeStr;
        if (fi.isFile()) {
            qint64 sz = fi.size();
            sizeStr = sz < 1024 ? QString("%1 B").arg(sz)
                    : sz < 1024*1024 ? QString("%1 KB").arg(sz/1024)
                    : QString("%1 MB").arg(sz/(1024*1024));
        }

        QString dateStr = fi.lastModified()
                              .toString("yyyy/MM/dd HH:mm");

        auto* nameItem = new QTableWidgetItem(fi.fileName());
        nameItem->setData(Qt::UserRole, fi.absoluteFilePath());
        // 目录用浅蓝色区分
        if (fi.isDir())
            nameItem->setForeground(QColor("#6cb6ff"));

        m_fileList->setItem(row, 0, nameItem);
        m_fileList->setItem(row, 1, new QTableWidgetItem(typeName));
        m_fileList->setItem(row, 2, new QTableWidgetItem(sizeStr));
        m_fileList->setItem(row, 3, new QTableWidgetItem(dateStr));
    }
}

// ── Slots ─────────────────────────────────────────────────────
void AssetBrowserPanel::onFolderSelected(QTreeWidgetItem* cur,
                                          QTreeWidgetItem* /*prev*/)
{
    if (!cur) return;
    QString path = cur->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) return;

    m_currentDir = path;

    // 路径标签显示相对 exe 目录的路径
    QString rel = QDir(QCoreApplication::applicationDirPath())
                      .relativeFilePath(path);
    m_pathLabel->setText(rel.replace("/", " / "));

    populateFileList(path, m_searchBar->text());
}

void AssetBrowserPanel::onSearchChanged(const QString& text)
{
    populateFileList(m_currentDir, text);
}

void AssetBrowserPanel::onFileClicked(QTableWidgetItem* item)
{
    if (!item || item->column() != 0) return;
    QString path = item->data(Qt::UserRole).toString();
    if (QFileInfo(path).isFile())
        emit assetSelected(path);
}

void AssetBrowserPanel::applyStyle()
{
    setStyleSheet(R"(
        AssetBrowserPanel {
            background: #252526;
        }
        QLabel#pathLabel {
            color: #cccccc;
            font-size: 11px;
            background: transparent;
        }
        QLineEdit#assetSearch {
            background: #3c3c3c;
            border: 1px solid #555555;
            border-radius: 3px;
            color: #cccccc;
            font-size: 11px;
            padding: 1px 6px;
            height: 20px;
        }
        QLineEdit#assetSearch:focus {
            border-color: #0078d4;
        }
        QPushButton#navBtn {
            background: transparent;
            border: none;
            color: #888888;
            font-size: 13px;
            border-radius: 3px;
        }
        QPushButton#navBtn:hover {
            background: #3f3f46;
            color: #cccccc;
        }
        QTreeWidget#folderTree {
            background: #252526;
            border: none;
            border-right: 1px solid #1b1b1c;
            color: #cccccc;
            font-size: 12px;
            outline: none;
        }
        QTreeWidget#folderTree::item {
            height: 22px;
            padding-left: 2px;
        }
        QTreeWidget#folderTree::item:hover {
            background: #2a2d2e;
        }
        QTreeWidget#folderTree::item:selected {
            background: #094771;
            color: #ffffff;
        }
        QTableWidget#fileList {
            background: #1e1e1e;
            gridline-color: transparent;
            border: none;
            color: #cccccc;
            font-size: 12px;
            outline: none;
        }
        QTableWidget#fileList::item {
            padding: 0 6px;
            border: none;
        }
        QTableWidget#fileList::item:hover {
            background: #2a2d2e;
        }
        QTableWidget#fileList::item:selected {
            background: #094771;
            color: #ffffff;
        }
        QHeaderView::section {
            background: #2d2d2d;
            color: #888888;
            font-size: 11px;
            border: none;
            border-right: 1px solid #1b1b1c;
            border-bottom: 1px solid #1b1b1c;
            padding: 2px 6px;
            height: 20px;
        }
        QScrollBar:vertical, QScrollBar:horizontal {
            background: #252526;
            width: 8px; height: 8px;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #555555;
            border-radius: 4px;
            min-height: 20px; min-width: 20px;
        }
        QScrollBar::handle:hover { background: #777777; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
        QSplitter::handle { background: #1b1b1c; }
    )");
}


// ═══════════════════════════════════════════════════════════════
//  LogConsolePanel
// ═══════════════════════════════════════════════════════════════
LogConsolePanel::LogConsolePanel(QWidget* parent)
    : QWidget(parent)
{
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setObjectName("logOutput");
    m_output->document()->setMaximumBlockCount(2000);  // 最多2000行，防内存膨胀

    m_clearBtn = new QPushButton(tr("清空"), this);
    m_clearBtn->setObjectName("logClearBtn");
    m_clearBtn->setFixedHeight(22);
    connect(m_clearBtn, &QPushButton::clicked, this, &LogConsolePanel::clear);

    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(6, 2, 6, 2);
    toolbar->setSpacing(4);
    toolbar->addStretch();
    toolbar->addWidget(m_clearBtn);

    auto* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);
    vlay->addLayout(toolbar);
    vlay->addWidget(m_output, 1);
    setLayout(vlay);

    applyStyle();

    // 写几条示例日志
    appendMessage(tr("[INFO]  Level 'content/levels/basic.level' loaded."), 0);
    appendMessage(tr("[INFO]  Engine initialized.  OpenGL 4.6"), 0);
}

void LogConsolePanel::appendMessage(const QString& msg, int level)
{
    static const char* colors[] = { "#cccccc", "#e8b930", "#f44747" };
    const char* color = (level >= 0 && level <= 2) ? colors[level] : colors[0];
    m_output->append(
        QString("<span style='color:%1;font-size:12px;'>%2</span>")
            .arg(color, msg.toHtmlEscaped())
    );
    // 自动滚到底部
    auto* sb = m_output->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void LogConsolePanel::clear()
{
    m_output->clear();
}

void LogConsolePanel::applyStyle()
{
    setStyleSheet(R"(
        LogConsolePanel {
            background: #1e1e1e;
        }
        QTextEdit#logOutput {
            background: #1e1e1e;
            border: none;
            color: #cccccc;
            font-family: Consolas, monospace;
            font-size: 12px;
        }
        QPushButton#logClearBtn {
            background: #3c3c3c;
            border: 1px solid #555;
            border-radius: 3px;
            color: #aaaaaa;
            font-size: 11px;
            padding: 0 10px;
        }
        QPushButton#logClearBtn:hover {
            background: #4f4f4f;
            color: #ffffff;
        }
        QScrollBar:vertical {
            background: #1e1e1e;
            width: 8px;
        }
        QScrollBar::handle:vertical {
            background: #555555;
            border-radius: 4px;
            min-height: 20px;
        }
        QScrollBar::handle:vertical:hover { background: #777777; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
    )");
}


// ═══════════════════════════════════════════════════════════════
//  AssetPreviewPanel
// ═══════════════════════════════════════════════════════════════
AssetPreviewPanel::AssetPreviewPanel(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(220);   // 和截图右侧预览区宽度一致

    m_noSelLabel = new QLabel(tr("No previewable asset selected"), this);
    m_noSelLabel->setObjectName("noSelLabel");
    m_noSelLabel->setAlignment(Qt::AlignCenter);
    m_noSelLabel->setWordWrap(true);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setObjectName("previewLabel");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setScaledContents(false);
    m_previewLabel->setVisible(false);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setObjectName("infoLabel");
    m_infoLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setVisible(false);

    auto* vlay = new QVBoxLayout(this);
    vlay->setContentsMargins(4, 4, 4, 4);
    vlay->setSpacing(4);
    vlay->addStretch();
    vlay->addWidget(m_noSelLabel, 0, Qt::AlignCenter);
    vlay->addWidget(m_previewLabel, 1, Qt::AlignCenter);
    vlay->addWidget(m_infoLabel);
    vlay->addStretch();
    setLayout(vlay);

    applyStyle();
}

void AssetPreviewPanel::previewAsset(const QString& path)
{
    if (path.isEmpty()) {
        m_noSelLabel->setVisible(true);
        m_previewLabel->setVisible(false);
        m_infoLabel->setVisible(false);
        return;
    }

    // 尝试作为图片加载
    QPixmap px(path);
    if (!px.isNull()) {
        QPixmap scaled = px.scaled(200, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_previewLabel->setPixmap(scaled);
        m_previewLabel->setVisible(true);
        m_infoLabel->setText(QString("%1\n%2 x %3")
            .arg(QFileInfo(path).fileName())
            .arg(px.width()).arg(px.height()));
        m_infoLabel->setVisible(true);
        m_noSelLabel->setVisible(false);
    } else {
        // 非图片资产，显示文件名
        m_noSelLabel->setText(QFileInfo(path).fileName());
        m_noSelLabel->setVisible(true);
        m_previewLabel->setVisible(false);
        m_infoLabel->setVisible(false);
    }
}

void AssetPreviewPanel::applyStyle()
{
    setStyleSheet(R"(
        AssetPreviewPanel {
            background: #252526;
            border-left: 1px solid #1b1b1c;
        }
        QLabel#noSelLabel {
            color: #666666;
            font-size: 11px;
        }
        QLabel#infoLabel {
            color: #888888;
            font-size: 11px;
        }
    )");
}


// ═══════════════════════════════════════════════════════════════
//  BottomPanel
// ═══════════════════════════════════════════════════════════════
BottomPanel::BottomPanel(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(220);    // 与截图底部面板高度一致，可在 YGmax 里调整

    // ── 子面板 ─────────────────────────────────────────────────
    m_assetBrowser = new AssetBrowserPanel(this);
    m_logConsole   = new LogConsolePanel(this);
    m_assetPreview = new AssetPreviewPanel(this);

    // ── 左侧：TabBar + StackedWidget ───────────────────────────
    m_tabBar = new QTabBar(this);
    m_tabBar->setObjectName("bottomTabBar");
    m_tabBar->setExpanding(false);
    m_tabBar->addTab(tr("Asset Browser"));
    m_tabBar->addTab(tr("Log Console"));

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_assetBrowser);
    m_stack->addWidget(m_logConsole);

    connect(m_tabBar, &QTabBar::currentChanged,
            m_stack, &QStackedWidget::setCurrentIndex);

    auto* leftWidget = new QWidget(this);
    auto* leftVlay   = new QVBoxLayout(leftWidget);
    leftVlay->setContentsMargins(0, 0, 0, 0);
    leftVlay->setSpacing(0);
    leftVlay->addWidget(m_tabBar);
    leftVlay->addWidget(m_stack, 1);
    leftWidget->setLayout(leftVlay);

    // ── 右侧预览面板标题栏 ──────────────────────────────────────
    auto* previewHeader = new QLabel(tr("Asset Preview"), this);
    previewHeader->setObjectName("previewHeader");
    previewHeader->setFixedHeight(24);
    previewHeader->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    previewHeader->setContentsMargins(8, 0, 0, 0);

    auto* rightWidget = new QWidget(this);
    auto* rightVlay   = new QVBoxLayout(rightWidget);
    rightVlay->setContentsMargins(0, 0, 0, 0);
    rightVlay->setSpacing(0);
    rightVlay->addWidget(previewHeader);
    rightVlay->addWidget(m_assetPreview, 1);
    rightWidget->setLayout(rightVlay);
    rightWidget->setFixedWidth(220);

    // ── 左右用 Splitter 拼合 ──────────────────────────────────
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(1);
    m_splitter->addWidget(leftWidget);
    m_splitter->addWidget(rightWidget);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);

    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);
    mainLay->addWidget(m_splitter, 1);
    setLayout(mainLay);

    // Asset Browser → Asset Preview 信号联通
    connect(m_assetBrowser, &AssetBrowserPanel::assetSelected,
            m_assetPreview, &AssetPreviewPanel::previewAsset);

    applyStyle();
}

void BottomPanel::applyStyle()
{
    setStyleSheet(R"(
        BottomPanel {
            background: #252526;
            border-top: 1px solid #1b1b1c;
        }
        QTabBar#bottomTabBar {
            background: #2d2d2d;
        }
        QTabBar#bottomTabBar::tab {
            background: #2d2d2d;
            color: #888888;
            border: none;
            border-right: 1px solid #1b1b1c;
            padding: 0 14px;
            height: 26px;
            font-size: 12px;
        }
        QTabBar#bottomTabBar::tab:selected {
            background: #1e1e1e;
            color: #cccccc;
            border-top: 2px solid #0078d4;
        }
        QTabBar#bottomTabBar::tab:hover:!selected {
            background: #3f3f46;
            color: #cccccc;
        }
        QLabel#previewHeader {
            background: #2d2d2d;
            color: #cccccc;
            font-size: 12px;
            border-bottom: 1px solid #1b1b1c;
        }
        QSplitter::handle {
            background: #1b1b1c;
        }
    )");
}
