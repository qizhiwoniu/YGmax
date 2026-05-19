#pragma once

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>

class SystemTrayIcon : public QSystemTrayIcon
{
    Q_OBJECT

public:
    explicit SystemTrayIcon(QWidget* mainWindow, QObject* parent = nullptr);

signals:
    void showMainWindow();
    void checkUpdate();

private:
    void buildMenu();

    QWidget* m_mainWindow;
    QMenu* m_menu;
    QAction* m_showAct;
    QAction* m_updateAct;
    QAction* m_quitAct;
};