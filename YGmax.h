#pragma once

#include <QtWidgets/QWidget>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QPainterPath>
#include "ui_YGmax.h"
#include "ToolBar.h"
#include "CustomTitleBar.h"
#include "SystemTrayIcon.h" 
#include "DocumentTabBar.h" 

class YGmax : public QWidget
{
    Q_OBJECT

public:
    YGmax(QWidget *parent = nullptr);
    ~YGmax();
protected:
    void paintEvent(QPaintEvent* event) override;  // ← 新增
    void resizeEvent(QResizeEvent* event) override; // ← 新增
    void closeEvent(QCloseEvent* event) override;   // ← 新增
private:
    //Ui::YGmaxClass    ui;
    QAction*            m_newDocAct      = nullptr;
    CustomTitleBar*     m_titleBar       = nullptr;
    DocumentTabBar*     m_tabBar         = nullptr;   // ← 新增
    ToolBar*            m_toolBar        = nullptr;   // ← 新增
    QWidget*            m_contentWidget  = nullptr;   // ← 新增
    SystemTrayIcon*     m_tray           = nullptr;

    void setupMenuBar();
    void setupTray();
};

