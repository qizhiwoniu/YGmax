#pragma once
#include <QWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QFrame>

class ToolBar : public QWidget
{
    Q_OBJECT
public:
    explicit ToolBar(QWidget* parent = nullptr);

    // 外部可拿到按钮连接信号
    QPushButton* selectBtn() { return m_selectBtn; }
    QPushButton* defaultBtn() { return m_defaultBtn; }

signals:
    void selectModeActivated();
    void defaultModeActivated();
    void actionTriggered(int id);   // 其他图标按钮用 id 区分

private:
    QPushButton* m_selectBtn = nullptr;
    QPushButton* m_defaultBtn = nullptr;
    QButtonGroup* m_modeGroup = nullptr;

    QPushButton* addIconBtn(const QString& icon, const QString& tooltip, int id, bool tintOnActive);
    QFrame* addSeparator();
    void         applyStyle();
};