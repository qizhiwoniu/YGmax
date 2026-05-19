#pragma once
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QList>

struct DocTab {
    QString   title;
    bool      modified = false;   // 是否有未保存修改（显示 •）
    QPushButton* btn = nullptr;
};

class DocumentTabBar : public QWidget
{
    Q_OBJECT
public:
    explicit DocumentTabBar(QWidget* parent = nullptr);

    int  addTab(const QString& title);       // 返回 index
    void removeTab(int index);
    void setCurrentTab(int index);
    void setTabModified(int index, bool modified);
    int  currentIndex() const { return m_current; }
    int  count()        const { return m_tabs.count(); }

signals:
    void tabChanged(int index);
    void tabCloseRequested(int index);
    void newTabRequested();

private:
    int indexOfBtn(QPushButton* btn) const {
        for (int i = 0; i < m_tabs.count(); ++i)
            if (m_tabs[i].btn == btn) return i;
        return -1;
    }
    void rebuildTabs();
    void applyStyle();

    QHBoxLayout* m_tabLayout = nullptr;
    QWidget* m_tabWidget = nullptr;
    QScrollArea* m_scroll = nullptr;
    QPushButton* m_newBtn = nullptr;

    QList<DocTab> m_tabs;
    int           m_current = -1;
};