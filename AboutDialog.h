#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};