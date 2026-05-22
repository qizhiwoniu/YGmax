#include "stdafx.h"
#include "BottomPanel.h"
#include <QHeaderView>
#include <QScrollBar>
#include <QDateTime>
#include <QTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QCoreApplication>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QApplication>
#include <QFileIconProvider>
#include <QStyle>
#include <functional>

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
            if (!f.exists()) {f.open(QIODevice::WriteOnly);f.close();}
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

// 在 BottomPanel 完成 connect 后调用，补发初始化日志
void AssetBrowserPanel::initLog()
{
    emit logMessage(tr("[AssetBrowser] 初始化完成，根目录: %1").arg(m_currentDir), 0);

    // 统计当前目录内容
    QDir dir(m_currentDir);
    int dirs  = dir.entryList(QDir::Dirs  | QDir::NoDotAndDotDot).count();
    int files = dir.entryList(QDir::Files).count();
    emit logMessage(tr("[AssetBrowser] 当前目录包含 %1 个文件夹，%2 个文件")
                        .arg(dirs).arg(files), 0);
}

// ── 构建左侧目录树 ────────────────────────────────────────────
void AssetBrowserPanel::setupFolderTree()
{
    m_folderTree = new QTreeWidget(this);
    m_folderTree->setObjectName("folderTree");
    m_folderTree->setHeaderHidden(true);
    m_folderTree->setIndentation(16);
    m_folderTree->setRootIsDecorated(true);   // 显示展开/折叠箭头
    m_folderTree->setAnimated(true);
    m_folderTree->setIconSize(QSize(16, 16));
    m_folderTree->setContextMenuPolicy(Qt::CustomContextMenu);

    // 系统图标提供器
    QFileIconProvider iconProvider;

    // 根节点 = exe 目录
    QString exeDir  = QCoreApplication::applicationDirPath();
    QString dirName = QDir(exeDir).dirName();

    auto* root = new QTreeWidgetItem(m_folderTree, { dirName });
    root->setData(0, Qt::UserRole, exeDir);
    root->setIcon(0, iconProvider.icon(QFileIconProvider::Computer));
    root->setExpanded(true);

    // Media 节点
    QTreeWidgetItem* mediaNode = buildTreeNode(root, mediaRoot(), true);
    Q_UNUSED(mediaNode)

    m_folderTree->setCurrentItem(root);

    connect(m_folderTree, &QTreeWidget::currentItemChanged,
            this, &AssetBrowserPanel::onFolderSelected);
    connect(m_folderTree, &QTreeWidget::customContextMenuRequested,
            this, &AssetBrowserPanel::onFolderTreeContextMenu);
}

// 递归构建目录树
QTreeWidgetItem* AssetBrowserPanel::buildTreeNode(
        QTreeWidgetItem* parent,
        const QString&   dirPath,
        bool             expanded)
{
    QFileIconProvider iconProvider;
    QDir dir(dirPath);
    auto* node = new QTreeWidgetItem(parent, { dir.dirName() });
    node->setData(0, Qt::UserRole, dirPath);
    node->setExpanded(expanded);
    // 用系统文件夹图标
    node->setIcon(0, iconProvider.icon(QFileInfo(dirPath)));

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
    m_fileList->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_fileList, &QTableWidget::itemClicked,
            this, &AssetBrowserPanel::onFileClicked);
    connect(m_fileList, &QTableWidget::itemDoubleClicked,
            this, &AssetBrowserPanel::onFileDoubleClicked);
    connect(m_fileList, &QTableWidget::customContextMenuRequested,
            this, &AssetBrowserPanel::onFileListContextMenu);
}

// ── 填充右侧文件列表 ──────────────────────────────────────────
void AssetBrowserPanel::populateFileList(const QString& dirPath,
                                          const QString& filter)
{
    m_fileList->setRowCount(0);
    QDir dir(dirPath);
    QFileIconProvider iconProvider;

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
        // 使用系统图标
        nameItem->setIcon(iconProvider.icon(fi));
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

    QString rel = QDir(QCoreApplication::applicationDirPath())
                      .relativeFilePath(path);
    m_pathLabel->setText(rel.replace("/", " / "));

    populateFileList(path, m_searchBar->text());
    emit logMessage(tr("[AssetBrowser] 浏览目录: %1").arg(path), 0);
}

void AssetBrowserPanel::onSearchChanged(const QString& text)
{
    populateFileList(m_currentDir, text);
    if (!text.isEmpty())
        emit logMessage(tr("[AssetBrowser] 搜索: \"%1\" 在 %2").arg(text, m_currentDir), 0);
}

void AssetBrowserPanel::onFileClicked(QTableWidgetItem* item)
{
    if (!item || item->column() != 0) return;
    QString path = item->data(Qt::UserRole).toString();
    if (QFileInfo(path).isFile()) {
        emit assetSelected(path);
        emit logMessage(tr("[AssetBrowser] 选中文件: %1").arg(path), 0);
    }
}

// 双击进入目录
void AssetBrowserPanel::onFileDoubleClicked(QTableWidgetItem* item)
{
    if (!item || item->column() != 0) return;
    QString path = item->data(Qt::UserRole).toString();
    if (QFileInfo(path).isDir()) {
        m_currentDir = path;
        QString rel = QDir(QCoreApplication::applicationDirPath())
                          .relativeFilePath(path);
        m_pathLabel->setText(rel.replace("/", " / "));
        populateFileList(path, m_searchBar->text());
        emit logMessage(tr("[AssetBrowser] 进入目录: %1").arg(path), 0);
    }
}

// ── 右侧文件列表右键菜单 ──────────────────────────────────────
void AssetBrowserPanel::onFileListContextMenu(const QPoint& pos)
{
    // 优先取鼠标下那一行（任意列都行），其次用当前选中行
    QString selectedPath;
    QTableWidgetItem* hitItem = m_fileList->itemAt(pos);
    int row = -1;
    if (hitItem) {
        row = hitItem->row();
    } else {
        // 点在行间空白时，用当前选中行
        auto selected = m_fileList->selectedItems();
        if (!selected.isEmpty())
            row = selected.first()->row();
    }
    if (row >= 0) {
        auto* nameItem = m_fileList->item(row, 0);
        if (nameItem) {
            selectedPath = nameItem->data(Qt::UserRole).toString();
            // 同步选中高亮
            m_fileList->selectRow(row);
        }
    }

    QMenu menu(this);
    menu.setObjectName("contextMenu");

    // 有选中项时才显示文件操作
    if (!selectedPath.isEmpty()) {
        QAction* actRename = menu.addAction(tr("重命名"));
        QAction* actCopy   = menu.addAction(tr("复制"));
        QAction* actCut    = menu.addAction(tr("剪切"));
        menu.addSeparator();
        QAction* actDelete = menu.addAction(tr("删除"));
        menu.addSeparator();

        connect(actRename, &QAction::triggered, this, [this, selectedPath]() {
            QFileInfo fi(selectedPath);
            bool ok;
            QString newName = QInputDialog::getText(
                this, tr("重命名"),
                tr("新名称:"),
                QLineEdit::Normal, fi.fileName(), &ok);
            if (!ok || newName.isEmpty() || newName == fi.fileName()) return;
            QString newPath = fi.dir().filePath(newName);
            if (QFile::rename(selectedPath, newPath)) {
                populateFileList(m_currentDir, m_searchBar->text());
                refreshTreeNode(m_currentDir);
                emit logMessage(tr("[AssetBrowser] 重命名: %1  →  %2")
                    .arg(fi.fileName(), newName), 0);
            } else {
                emit logMessage(tr("[AssetBrowser] 重命名失败: %1").arg(fi.fileName()), 2);
                QMessageBox::warning(this, tr("重命名失败"),
                    tr("无法重命名: %1").arg(fi.fileName()));
            }
        });

        connect(actCopy, &QAction::triggered, this, [this, selectedPath]() {
            m_clipboardPath = selectedPath;
            m_clipboardIsCut = false;
            emit logMessage(tr("[AssetBrowser] 已复制: %1")
                .arg(QFileInfo(selectedPath).fileName()), 0);
        });

        connect(actCut, &QAction::triggered, this, [this, selectedPath]() {
            m_clipboardPath = selectedPath;
            m_clipboardIsCut = true;
            emit logMessage(tr("[AssetBrowser] 已剪切: %1")
                .arg(QFileInfo(selectedPath).fileName()), 1);
        });

        connect(actDelete, &QAction::triggered, this, [this, selectedPath]() {
            QFileInfo fi(selectedPath);
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("确认删除"));
            msgBox.setText(tr("确定要删除 \"%1\" 吗？").arg(fi.fileName()));
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::No);
            if (msgBox.exec() != QMessageBox::Yes) {
                emit logMessage(tr("[AssetBrowser] 取消删除: %1").arg(fi.fileName()), 0);
                return;
            }
            bool ok = fi.isDir()
                ? QDir(selectedPath).removeRecursively()
                : QFile::remove(selectedPath);
            if (ok) {
                populateFileList(m_currentDir, m_searchBar->text());
                refreshTreeNode(m_currentDir);
                emit logMessage(tr("[AssetBrowser] 已删除: %1").arg(selectedPath), 1);
            } else {
                emit logMessage(tr("[AssetBrowser] 删除失败: %1").arg(selectedPath), 2);
                QMessageBox::warning(this, tr("删除失败"),
                    tr("无法删除: %1").arg(fi.fileName()));
            }
        });
    }

    // 粘贴总是可用（如果剪贴板有内容）
    if (!m_clipboardPath.isEmpty()) {
        QAction* actPaste = menu.addAction(tr("粘贴"));
        connect(actPaste, &QAction::triggered, this, &AssetBrowserPanel::filePaste);
    }

    menu.addSeparator();
    QAction* actNewFolder = menu.addAction(tr("新建文件夹"));
    connect(actNewFolder, &QAction::triggered, this, &AssetBrowserPanel::fileNewFolder);

    // 应用暗色样式
    menu.setStyleSheet(R"(
        QMenu {
            background: #2d2d2d;
            border: 1px solid #3f3f46;
            color: #cccccc;
            font-size: 12px;
            padding: 2px 0;
        }
        QMenu::item {
            padding: 4px 24px 4px 16px;
        }
        QMenu::item:selected {
            background: #094771;
            color: #ffffff;
        }
        QMenu::separator {
            height: 1px;
            background: #3f3f46;
            margin: 2px 8px;
        }
    )");

    menu.exec(m_fileList->viewport()->mapToGlobal(pos));
}

// ── 左侧目录树右键菜单 ────────────────────────────────────────
void AssetBrowserPanel::onFolderTreeContextMenu(const QPoint& pos)
{
    // indexAt 比 itemAt 更可靠，能准确命中当前行
    QModelIndex idx = m_folderTree->indexAt(pos);
    QTreeWidgetItem* item = idx.isValid()
        ? m_folderTree->itemFromIndex(idx)
        : m_folderTree->currentItem();   // 点空白时降级到当前选中项
    if (!item) return;
    QString dirPath = item->data(0, Qt::UserRole).toString();
    if (dirPath.isEmpty()) return;

    QMenu menu(this);

    QAction* actRename    = menu.addAction(tr("重命名文件夹"));
    QAction* actNewFolder = menu.addAction(tr("新建子文件夹"));
    menu.addSeparator();
    QAction* actDelete = menu.addAction(tr("删除文件夹"));

    // 用持久化索引避免 menu.exec() 期间指针失效
    //QPersistentModelIndex persistIdx = m_folderTree->indexFromItem(item);

    connect(actRename, &QAction::triggered, this, [this, /*persistIdx*/item, dirPath]() {
        //QTreeWidgetItem* it = m_folderTree->itemFromIndex(persistIdx);
        //if (!it) return;
        QFileInfo fi(dirPath);
        bool ok;
        QString newName = QInputDialog::getText(
            this, tr("重命名文件夹"),
            tr("新名称:"), QLineEdit::Normal, fi.fileName(), &ok);
        if (!ok || newName.isEmpty() || newName == fi.fileName()) return;
        QString newPath = fi.dir().filePath(newName);
        if (QFile::rename(dirPath, newPath)) {
            item->setText(0, newName);
            item->setData(0, Qt::UserRole, newPath);
            if (m_currentDir == dirPath) {
                m_currentDir = newPath;
                populateFileList(newPath, m_searchBar->text());
            }
            emit logMessage(tr("[AssetBrowser] 文件夹重命名: %1  →  %2")
                .arg(fi.fileName(), newName), 0);
        } else {
            emit logMessage(tr("[AssetBrowser] 文件夹重命名失败: %1").arg(fi.fileName()), 2);
            QMessageBox::warning(this, tr("重命名失败"),
                tr("无法重命名文件夹: %1").arg(fi.fileName()));
        }
    });

    connect(actNewFolder, &QAction::triggered, this, [this, /*persistIdx*/item, dirPath]() {
        //QTreeWidgetItem* it = m_folderTree->itemFromIndex(persistIdx);
        //if (!it) return;
        bool ok;
        QString name = QInputDialog::getText(
            this, tr("新建文件夹"), tr("文件夹名称:"),
            QLineEdit::Normal, tr("新建文件夹"), &ok);
        if (!ok || name.isEmpty()) return;
        QString newPath = dirPath + "/" + name;
        if (QDir().mkdir(newPath)) {
            QFileIconProvider iconProvider;
            auto* child = new QTreeWidgetItem(item, { name });
            child->setData(0, Qt::UserRole, newPath);
            child->setIcon(0, iconProvider.icon(QFileInfo(newPath)));
            item->setExpanded(true);
            populateFileList(m_currentDir, m_searchBar->text());
            emit logMessage(tr("[AssetBrowser] 新建文件夹: %1").arg(newPath), 0);
        } else {
            emit logMessage(tr("[AssetBrowser] 新建文件夹失败: %1").arg(newPath), 2);
        }
    });

    connect(actDelete, &QAction::triggered, this, [this, /*persistIdx*/item, dirPath]() {
        //QTreeWidgetItem* it = m_folderTree->itemFromIndex(persistIdx);
        //if (!it) return;
        if (!item) return;
        QFileInfo fi(dirPath);

        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("确认删除"));
        msgBox.setText(tr("确定要删除文件夹 \"%1\" 及其所有内容吗？").arg(fi.fileName()));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        if (msgBox.exec() != QMessageBox::Yes) {
            emit logMessage(tr("[AssetBrowser] 取消删除文件夹: %1").arg(fi.fileName()), 0);
            return;
        }

        if (QDir(dirPath).removeRecursively()) {
            QTreeWidgetItem* parentItem = item->parent();
            QString parentPath = parentItem
                ? parentItem->data(0, Qt::UserRole).toString()
                : mediaRoot();

            if (parentItem)
                parentItem->removeChild(item);
            else
                delete item;

            emit logMessage(tr("[AssetBrowser] 已删除文件夹: %1").arg(dirPath), 1);

            if (m_currentDir == dirPath || m_currentDir.startsWith(dirPath + "/")) {
                m_currentDir = parentPath;
                QString rel = QDir(QCoreApplication::applicationDirPath())
                                  .relativeFilePath(m_currentDir);
                m_pathLabel->setText(rel.replace("/", " / "));
                populateFileList(m_currentDir, m_searchBar->text());
            }
        } else {
            emit logMessage(tr("[AssetBrowser] 删除文件夹失败: %1").arg(dirPath), 2);
            QMessageBox::warning(this, tr("删除失败"),
                tr("无法删除文件夹: %1").arg(fi.fileName()));
        }
    });

    menu.setStyleSheet(R"(
        QMenu {
            background: #2d2d2d;
            border: 1px solid #3f3f46;
            color: #cccccc;
            font-size: 12px;
            padding: 2px 0;
        }
        QMenu::item { padding: 4px 24px 4px 16px; }
        QMenu::item:selected { background: #094771; color: #ffffff; }
        QMenu::separator { height: 1px; background: #3f3f46; margin: 2px 8px; }
    )");

    menu.exec(m_folderTree->viewport()->mapToGlobal(pos));
}

// ── 文件操作辅助 ──────────────────────────────────────────────
void AssetBrowserPanel::fileCopy()
{
    auto* item = m_fileList->currentItem();
    if (!item) return;
    int row = item->row();
    auto* nameItem = m_fileList->item(row, 0);
    if (nameItem) {
        m_clipboardPath = nameItem->data(Qt::UserRole).toString();
        m_clipboardIsCut = false;
    }
}

void AssetBrowserPanel::fileCut()
{
    fileCopy();
    m_clipboardIsCut = true;
}

void AssetBrowserPanel::filePaste()
{
    if (m_clipboardPath.isEmpty()) return;
    QFileInfo fi(m_clipboardPath);
    if (!fi.exists()) { m_clipboardPath.clear(); return; }

    QString dest = m_currentDir + "/" + fi.fileName();

    // 避免同名覆盖
    if (QFileInfo::exists(dest)) {
        QString base = fi.completeBaseName();
        QString ext  = fi.suffix().isEmpty() ? "" : "." + fi.suffix();
        int n = 1;
        do { dest = m_currentDir + "/" + base + QString("_%1").arg(n++) + ext; }
        while (QFileInfo::exists(dest));
    }

    bool ok = false;
    if (fi.isDir()) {
        // 目录：递归复制（Qt 无内置，用 QDir）
        std::function<bool(const QString&, const QString&)> copyDir;
        copyDir = [&](const QString& src, const QString& dst) -> bool {
            QDir().mkpath(dst);
            for (auto& e : QDir(src).entryInfoList(
                     QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
                QString d = dst + "/" + e.fileName();
                if (e.isDir()) { if (!copyDir(e.absoluteFilePath(), d)) return false; }
                else           { if (!QFile::copy(e.absoluteFilePath(), d)) return false; }
            }
            return true;
        };
        ok = copyDir(m_clipboardPath, dest);
        if (ok && m_clipboardIsCut)
            QDir(m_clipboardPath).removeRecursively();
    } else {
        ok = QFile::copy(m_clipboardPath, dest);
        if (ok && m_clipboardIsCut)
            QFile::remove(m_clipboardPath);
    }

    if (ok) {
        QString op = m_clipboardIsCut ? tr("移动") : tr("复制");
        emit logMessage(tr("[AssetBrowser] %1: %2  →  %3")
            .arg(op, m_clipboardPath, dest), 0);
        if (m_clipboardIsCut) m_clipboardPath.clear();
        populateFileList(m_currentDir, m_searchBar->text());
        refreshTreeNode(m_currentDir);
    } else {
        emit logMessage(tr("[AssetBrowser] 粘贴失败: %1").arg(fi.fileName()), 2);
        QMessageBox::warning(this, tr("粘贴失败"),
            tr("无法粘贴: %1").arg(fi.fileName()));
    }
}

void AssetBrowserPanel::fileRename()
{
    auto* item = m_fileList->currentItem();
    if (!item) return;
    int row = item->row();
    auto* nameItem = m_fileList->item(row, 0);
    if (!nameItem) return;
    QString path = nameItem->data(Qt::UserRole).toString();
    QFileInfo fi(path);
    bool ok;
    QString newName = QInputDialog::getText(
        this, tr("重命名"), tr("新名称:"),
        QLineEdit::Normal, fi.fileName(), &ok);
    if (!ok || newName.isEmpty() || newName == fi.fileName()) return;
    if (QFile::rename(path, fi.dir().filePath(newName))) {
        populateFileList(m_currentDir, m_searchBar->text());
        refreshTreeNode(m_currentDir);
        emit logMessage(tr("[AssetBrowser] 重命名: %1  →  %2")
            .arg(fi.fileName(), newName), 0);
    } else {
        emit logMessage(tr("[AssetBrowser] 重命名失败: %1").arg(fi.fileName()), 2);
    }
}

void AssetBrowserPanel::fileDelete()
{
    auto* item = m_fileList->currentItem();
    if (!item) return;
    int row = item->row();
    auto* nameItem = m_fileList->item(row, 0);
    if (!nameItem) return;
    QString path = nameItem->data(Qt::UserRole).toString();
    QFileInfo fi(path);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("确认删除"));
    msgBox.setText(tr("确定要删除 \"%1\" 吗？").arg(fi.fileName()));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    if (msgBox.exec() != QMessageBox::Yes) {
        emit logMessage(tr("[AssetBrowser] 取消删除: %1").arg(fi.fileName()), 0);
        return;
    }

    bool ok = fi.isDir()
        ? QDir(path).removeRecursively()
        : QFile::remove(path);
    if (ok) {
        populateFileList(m_currentDir, m_searchBar->text());
        refreshTreeNode(m_currentDir);
        emit logMessage(tr("[AssetBrowser] 已删除: %1").arg(path), 1);
    } else {
        emit logMessage(tr("[AssetBrowser] 删除失败: %1").arg(path), 2);
        QMessageBox::warning(this, tr("删除失败"),
            tr("无法删除: %1").arg(fi.fileName()));
    }
}

void AssetBrowserPanel::fileNewFolder()
{
    bool ok;
    QString name = QInputDialog::getText(
        this, tr("新建文件夹"), tr("文件夹名称:"),
        QLineEdit::Normal, tr("新建文件夹"), &ok);
    if (!ok || name.isEmpty()) return;
    QString newPath = m_currentDir + "/" + name;
    if (QDir().mkdir(newPath)) {
        populateFileList(m_currentDir, m_searchBar->text());
        refreshTreeNode(m_currentDir);
        emit logMessage(tr("[AssetBrowser] 新建文件夹: %1").arg(newPath), 0);
    } else {
        emit logMessage(tr("[AssetBrowser] 新建文件夹失败: %1").arg(newPath), 2);
    }
}

// 刷新目录树中对应节点的子项
void AssetBrowserPanel::refreshTreeNode(const QString& dirPath)
{
    QFileIconProvider iconProvider;
    // 找到对应节点
    QList<QTreeWidgetItem*> items = m_folderTree->findItems(
        QDir(dirPath).dirName(), Qt::MatchExactly | Qt::MatchRecursive);
    for (auto* it : items) {
        if (it->data(0, Qt::UserRole).toString() == dirPath) {
            // 清除旧子节点，重建
            while (it->childCount()) delete it->takeChild(0);
            for (const QFileInfo& fi :
                 QDir(dirPath).entryInfoList(
                     QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
            {
                buildTreeNode(it, fi.absoluteFilePath(), false);
            }
            break;
        }
    }
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
        QTreeWidget#folderTree::branch {
            background: #252526;
        }
        QTreeWidget#folderTree::branch:has-children:!has-siblings:closed,
        QTreeWidget#folderTree::branch:closed:has-children:has-siblings {
            border-image: none;
            image: none;
            color: #888888;
        }
        /* 展开/折叠指示符：用 Qt 内置箭头 */
        QTreeWidget#folderTree::branch:has-children:!has-siblings:closed,
        QTreeWidget#folderTree::branch:closed:has-children:has-siblings {
            border-image: none;
            image: url(:/qt-project.org/styles/commonstyle/images/right-arrow.png);
        }
        QTreeWidget#folderTree::branch:open:has-children:!has-siblings,
        QTreeWidget#folderTree::branch:open:has-children:has-siblings {
            border-image: none;
            image: url(:/qt-project.org/styles/commonstyle/images/down-arrow.png);
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
    m_output->setAcceptRichText(true);
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

    // 加时间戳
    QString timestamp = QTime::currentTime().toString("HH:mm:ss");

    // 移到末尾再插入，避免覆盖光标位置
    QTextCursor cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_output->setTextCursor(cursor);

    // insertHtml 才能真正渲染颜色
    m_output->insertHtml(
        QString("<span style='color:#555555;font-size:11px;'>%1</span>"
                "&nbsp;"
                "<span style='color:%2;font-size:12px;'>%3</span><br>")
            .arg(timestamp, color, msg.toHtmlEscaped())
    );
    m_output->ensureCursorVisible();
    m_output->repaint();
    // 自动滚到底部
    //auto* sb = m_output->verticalScrollBar();
    //sb->setValue(sb->maximum());
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

    // Asset Browser → Log Console 操作日志
    connect(m_assetBrowser, &AssetBrowserPanel::logMessage,
            m_logConsole,   &LogConsolePanel::appendMessage);

    // connect 建立后补发初始化日志
    m_assetBrowser->initLog();

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
