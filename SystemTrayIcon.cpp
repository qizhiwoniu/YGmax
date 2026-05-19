#include "stdafx.h"
#include "SystemTrayIcon.h"
#include "version.h"

#include <QApplication>

SystemTrayIcon::SystemTrayIcon(QWidget* mainWindow, QObject* parent)
    : QSystemTrayIcon(parent)
    , m_mainWindow(mainWindow)
{
    // 使用和主窗口相同的图标
    setIcon(QIcon(":/YGmax/YGmax.ico"));
    setToolTip(QString("%1  v%2").arg(VER_PRODUCT_NAME).arg(VERSION_STR));

    buildMenu();

    // 双击 / 单击托盘图标
    connect(this, &QSystemTrayIcon::activated,
        this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick ||
                reason == QSystemTrayIcon::Trigger) {
                emit showMainWindow();
            }
        });
}

void SystemTrayIcon::buildMenu()
{
    m_menu = new QMenu();
    m_menu->setStyleSheet(R"(
        QMenu {
            background-color: #1b1b1c;
            color: #cccccc;
            border: 1px solid #333333;
            border-radius: 6px;
            padding: 4px;
            font-size: 13px;
        }
        QMenu::item {
            padding: 6px 24px 6px 12px;
            border-radius: 4px;
        }
        QMenu::item:selected {
            background-color: #2d2d2d;
        }
        QMenu::separator {
            height: 1px;
            background: #333333;
            margin: 4px 8px;
        }
    )");

    m_showAct = m_menu->addAction(tr("显示主窗口"));
    m_updateAct = m_menu->addAction(tr("检查更新"));
    m_menu->addSeparator();
    m_quitAct = m_menu->addAction(tr("退出"));

    setContextMenu(m_menu);

    connect(m_showAct, &QAction::triggered, this, &SystemTrayIcon::showMainWindow);
    connect(m_updateAct, &QAction::triggered, this, &SystemTrayIcon::checkUpdate);
    connect(m_quitAct, &QAction::triggered, qApp, &QApplication::quit);
}