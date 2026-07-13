#ifndef ADAPTIXCLIENT_VERTICALTABWIDGET_H
#define ADAPTIXCLIENT_VERTICALTABWIDGET_H

#include <main.h>
#include <QStackedWidget>

class VerticalTabBar : public QWidget
{
Q_OBJECT

    struct TabInfo { QString text; };
    QVector<TabInfo> m_tabs;
    int m_currentIndex = -1;
    int m_hoveredIndex = -1;
    int m_hoveredCloseButton = -1;
    bool m_closable = false;
    int m_tabHeight = 28;
    int m_tabWidth = 32;
    bool m_showAddButton = false;
    bool m_addButtonHovered = false;

    int tabAt(const QPoint &pos) const;
    QRect closeButtonRect(int index) const;
    QRect addButtonRect() const;

public:
    explicit VerticalTabBar(QWidget *parent = nullptr);

    int addTab(const QString &text);
    void removeTab(int index);
    int count() const { return m_tabs.size(); }
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);
    void setTabsClosable(bool closable) { m_closable = closable; update(); }
    void setShowAddButton(bool show) { m_showAddButton = show; update(); }
    QString tabText(int index) const;

Q_SIGNALS:
    void currentChanged(int index);
    void tabCloseRequested(int index);
    void addTabRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    QSize sizeHint() const override;
};




class VerticalTabWidget : public QWidget
{
Q_OBJECT
    VerticalTabBar *m_tabBar;
    QStackedWidget *m_stack;
    QWidget *m_cornerWidget = nullptr;

public:
    explicit VerticalTabWidget(QWidget *parent = nullptr);

    int addTab(QWidget *widget, const QString &label);
    void removeTab(int index);
    int count() const { return m_tabBar->count(); }
    int currentIndex() const { return m_tabBar->currentIndex(); }
    void setCurrentIndex(int index);
    QWidget *widget(int index) const;
    void setTabsClosable(bool closable) { m_tabBar->setTabsClosable(closable); }
    VerticalTabBar *tabBar() const { return m_tabBar; }
    void setCornerWidget(QWidget *widget);

Q_SIGNALS:
    void currentChanged(int index);
    void tabCloseRequested(int index);
};

#endif
