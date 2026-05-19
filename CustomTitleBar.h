#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QMenuBar>
#include <QHBoxLayout>
#include <QMouseEvent>

class CustomTitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit CustomTitleBar(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setIcon(const QPixmap& pixmap);
    QMenuBar* menuBar() { return m_menuBar; }

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QLabel* m_iconLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QMenuBar* m_menuBar = nullptr;
    QPushButton* m_minBtn = nullptr;
    QPushButton* m_maxBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;

    bool   m_dragging = false;
    QPoint m_dragStartPos = QPoint(0, 0);

    void applyStyle();
};