#pragma once

#include <QtWidgets/QWidget>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QPainterPath>
#include "ui_YGmax.h"
#include "CustomTitleBar.h"

class YGmax : public QWidget
{
    Q_OBJECT

public:
    YGmax(QWidget *parent = nullptr);
    ~YGmax();
protected:
    void paintEvent(QPaintEvent* event) override;  // ← 新增
    void resizeEvent(QResizeEvent* event) override; // ← 新增
private:
    Ui::YGmaxClass ui;
    CustomTitleBar* m_titleBar;

    void setupMenuBar();
};

