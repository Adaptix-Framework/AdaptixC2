#include <Utils/CustomElements/VerticalTabWidget.h>
#include <QHBoxLayout>
#include <QPainter>
#include <QMouseEvent>

VerticalTabBar::VerticalTabBar(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFixedWidth(m_tabWidth);
}

int VerticalTabBar::addTab(const QString &text)
{
    m_tabs.append({text});
    if (m_currentIndex < 0)
        setCurrentIndex(0);
    update();
    return m_tabs.size() - 1;
}

void VerticalTabBar::removeTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_tabs.removeAt(index);
    if (m_currentIndex >= m_tabs.size())
        setCurrentIndex(m_tabs.size() - 1);
    update();
}

void VerticalTabBar::setCurrentIndex(int index)
{
    if (index == m_currentIndex || index < 0 || index >= m_tabs.size()) return;
    m_currentIndex = index;
    Q_EMIT currentChanged(index);
    update();
}

QString VerticalTabBar::tabText(int index) const
{
    if (index < 0 || index >= m_tabs.size()) return QString();
    return m_tabs[index].text;
}

int VerticalTabBar::tabAt(const QPoint &pos) const
{
    int y = pos.y();
    int offset = m_showAddButton ? m_tabHeight : 0;
    if (y < offset) return -1;
    int index = (y - offset) / m_tabHeight;
    if (index >= 0 && index < m_tabs.size())
        return index;
    return -1;
}

QRect VerticalTabBar::closeButtonRect(int index) const
{
    int offset = m_showAddButton ? m_tabHeight : 0;
    int y = offset + index * m_tabHeight;
    return QRect(m_tabWidth - 14, y + 4, 12, 12);
}

QRect VerticalTabBar::addButtonRect() const
{
    return QRect(0, 0, m_tabWidth, m_tabHeight);
}

void VerticalTabBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QPalette &pal = palette();
    QColor bgNormal = pal.color(QPalette::Window);
    QColor bgHover = pal.color(QPalette::Mid);
    QColor bgSelected = pal.color(QPalette::Highlight);
    QColor textNormal = pal.color(QPalette::WindowText);
    QColor textSelected = pal.color(QPalette::HighlightedText);
    QColor borderColor = pal.color(QPalette::Dark);

    int yOffset = 0;

    if (m_showAddButton) {
        QRect addRect = addButtonRect();
        if (m_addButtonHovered) {
            p.fillRect(addRect, bgHover);
        }
        p.setPen(textNormal);
        QFont f = font();
        f.setBold(true);
        f.setPointSize(f.pointSize() + 2);
        p.setFont(f);
        p.drawText(addRect, Qt::AlignCenter, "+");
        p.setPen(borderColor);
        p.drawLine(addRect.bottomLeft(), addRect.bottomRight());
        yOffset = m_tabHeight;
    }

    for (int i = 0; i < m_tabs.size(); ++i) {
        QRect tabRect(0, yOffset + i * m_tabHeight, m_tabWidth, m_tabHeight);
        bool selected = (i == m_currentIndex);
        bool hovered = (i == m_hoveredIndex);

        if (selected) {
            p.fillRect(tabRect, bgSelected);
        } else if (hovered) {
            p.fillRect(tabRect, bgHover);
        }

        if (selected) {
            p.fillRect(0, tabRect.top(), 3, tabRect.height(), pal.color(QPalette::Highlight));
        }

        p.setPen(selected ? textSelected : textNormal);
        QFont f = font();
        f.setBold(selected);
        p.setFont(f);
        QString num = QString::number(i + 1);
        p.drawText(tabRect, Qt::AlignCenter, num);

        if (m_closable && (selected || hovered)) {
            QRect closeRect = closeButtonRect(i);
            if (m_hoveredCloseButton == i) {
                p.setBrush(QColor(200, 60, 60));
                p.setPen(Qt::NoPen);
                p.drawEllipse(closeRect);
                p.setPen(Qt::white);
            } else {
                p.setPen(selected ? textSelected : textNormal);
            }
            p.drawText(closeRect, Qt::AlignCenter, "×");
        }

        if (i < m_tabs.size() - 1) {
            p.setPen(borderColor);
            p.drawLine(tabRect.bottomLeft(), tabRect.bottomRight());
        }
    }
}

void VerticalTabBar::mousePressEvent(QMouseEvent *event)
{
    if (m_showAddButton && addButtonRect().contains(event->pos())) {
        Q_EMIT addTabRequested();
        return;
    }
    int index = tabAt(event->pos());
    if (index >= 0) {
        if (m_closable && closeButtonRect(index).contains(event->pos())) {
            Q_EMIT tabCloseRequested(index);
        } else {
            setCurrentIndex(index);
        }
    }
}

void VerticalTabBar::mouseMoveEvent(QMouseEvent *event)
{
    int oldHover = m_hoveredIndex;
    int oldCloseHover = m_hoveredCloseButton;
    bool oldAddHover = m_addButtonHovered;

    m_addButtonHovered = m_showAddButton && addButtonRect().contains(event->pos());
    m_hoveredIndex = tabAt(event->pos());
    m_hoveredCloseButton = (m_closable && m_hoveredIndex >= 0 && closeButtonRect(m_hoveredIndex).contains(event->pos())) ? m_hoveredIndex : -1;

    if (oldHover != m_hoveredIndex || oldCloseHover != m_hoveredCloseButton || oldAddHover != m_addButtonHovered)
        update();
}

void VerticalTabBar::leaveEvent(QEvent *)
{
    m_hoveredIndex = -1;
    m_hoveredCloseButton = -1;
    m_addButtonHovered = false;
    update();
}

QSize VerticalTabBar::sizeHint() const
{
    int addButtonHeight = m_showAddButton ? m_tabHeight : 0;
    return QSize(m_tabWidth, addButtonHeight + m_tabs.size() * m_tabHeight);
}

VerticalTabWidget::VerticalTabWidget(QWidget *parent) : QWidget(parent)
{
    m_tabBar = new VerticalTabBar(this);
    m_stack = new QStackedWidget(this);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabBar);
    layout->addWidget(m_stack, 1);

    connect(m_tabBar, &VerticalTabBar::currentChanged, this, [this](int index) {
        m_stack->setCurrentIndex(index);
        Q_EMIT currentChanged(index);
    });
    connect(m_tabBar, &VerticalTabBar::tabCloseRequested, this, &VerticalTabWidget::tabCloseRequested);
}

int VerticalTabWidget::addTab(QWidget *widget, const QString &label)
{
    int index = m_tabBar->addTab(label);
    m_stack->insertWidget(index, widget);
    return index;
}

void VerticalTabWidget::removeTab(int index)
{
    QWidget *w = m_stack->widget(index);
    QSignalBlocker blocker(m_tabBar);
    m_tabBar->removeTab(index);
    m_stack->removeWidget(w);
}

void VerticalTabWidget::setCurrentIndex(int index)
{
    m_tabBar->setCurrentIndex(index);
}

QWidget *VerticalTabWidget::widget(int index) const
{
    return m_stack->widget(index);
}

void VerticalTabWidget::setCornerWidget(QWidget *widget)
{
    if (m_cornerWidget) {
        layout()->removeWidget(m_cornerWidget);
    }
    m_cornerWidget = widget;
    if (widget) {
        static_cast<QHBoxLayout*>(layout())->insertWidget(0, widget);
    }
}
