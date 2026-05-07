/*
  This file is part of KDDockWidgets.

  SPDX-FileCopyrightText: 2019 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Sérgio Martins <sergio.martins@kdab.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "TabBar.h"
#include "DockWidget.h"
#include "Stack.h"
#include "kddockwidgets/core/DockWidget.h"
#include "kddockwidgets/core/TabBar.h"
#include "kddockwidgets/core/Stack.h"
#include "core/Utils_p.h"
#include "core/TabBar_p.h"
#include "core/Logging_p.h"
#include "Config.h"
#include "qtwidgets/ViewFactory.h"
#include "kddockwidgets/core/DockRegistry.h"

#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QAbstractButton>
#include <QToolButton>
#include <QApplication>
#include <QTimer>
#include <QProxyStyle>
#include <QColor>
#include <QPalette>
#include <QTabWidget>
#include <QPainter>
#include <QPaintEvent>
#include <QStyleOptionTab>

using namespace KDDockWidgets;
using namespace KDDockWidgets::QtWidgets;

namespace KDDockWidgets {

class TabCloseButtonFilter : public QObject
{
public:
    explicit TabCloseButtonFilter(QObject* parent) : QObject(parent) {}

    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() != QEvent::Paint)
            return false;

        auto* btn = qobject_cast<QAbstractButton*>(obj);
        if (!btn)
            return false;

        auto* tabBar = qobject_cast<QTabBar*>(btn->parentWidget());
        if (!tabBar)
            return false;

        const QRect rect = btn->rect();
        const int tabIndex = tabBar->tabAt(btn->mapToParent(rect.center()));
        const bool tabSelected = (tabBar->currentIndex() == tabIndex);

        bool tabHovered = false;
        if (tabBar->underMouse()) {
            const auto mousePos = tabBar->mapFromGlobal(QCursor::pos());
            tabHovered = (tabBar->tabAt(mousePos) == tabIndex);
        }

        if (!tabSelected && !tabHovered)
            return true;

        const bool buttonHovered = btn->underMouse();
        const bool buttonPressed = btn->isDown();

        QPainter p(btn);
        p.setRenderHint(QPainter::Antialiasing, true);

        if (buttonHovered || buttonPressed) {
            const auto radius = rect.height() / 2.0;
            QColor bgColor = tabBar->palette().color(QPalette::WindowText);
            bgColor.setAlphaF(buttonPressed ? 0.2f : 0.1f);
            p.setPen(Qt::NoPen);
            p.setBrush(bgColor);
            p.drawRoundedRect(rect, radius, radius);
        }

        QColor fgColor = tabBar->palette().color(QPalette::WindowText);
        if (!tabSelected && !buttonHovered)
            fgColor.setAlphaF(0.5f);

        const int iconSz = qRound(qMin(rect.width(), rect.height()) * 0.38);
        const QRectF closeRect(
            rect.x() + (rect.width() - iconSz) / 2.0,
            rect.y() + (rect.height() - iconSz) / 2.0,
            iconSz, iconSz
        );

        p.setPen(QPen(fgColor, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawLine(closeRect.topLeft(), closeRect.bottomRight());
        p.drawLine(closeRect.topRight(), closeRect.bottomLeft());

        return true;
    }
};

class ScrollButtonFilter : public QObject
{
public:
    explicit ScrollButtonFilter(bool isLeft, QObject* parent) : QObject(parent), m_isLeft(isLeft) {}

    bool eventFilter(QObject* obj, QEvent* event) override
    {
        if (event->type() != QEvent::Paint)
            return false;

        auto* btn = qobject_cast<QToolButton*>(obj);
        if (!btn)
            return false;

        QPainter p(btn);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRect r = btn->rect();

        QColor bgColor = btn->palette().color(QPalette::Window);
        p.fillRect(r, bgColor);

        if (btn->underMouse()) {
            QColor hover = btn->palette().color(QPalette::WindowText);
            hover.setAlphaF(0.08f);
            p.fillRect(r, hover);
        }

        QColor fgColor = btn->palette().color(QPalette::WindowText);
        if (!btn->isEnabled())
            fgColor.setAlphaF(0.3f);

        const int arrowH = qRound(r.height() * 0.3);
        const int arrowW = qRound(arrowH * 0.5);
        const int cx = r.center().x();
        const int cy = r.center().y();

        p.setPen(QPen(fgColor, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);

        if (m_isLeft) {
            p.drawLine(QPoint(cx + arrowW/2, cy - arrowH/2), QPoint(cx - arrowW/2, cy));
            p.drawLine(QPoint(cx - arrowW/2, cy), QPoint(cx + arrowW/2, cy + arrowH/2));
        } else {
            p.drawLine(QPoint(cx - arrowW/2, cy - arrowH/2), QPoint(cx + arrowW/2, cy));
            p.drawLine(QPoint(cx + arrowW/2, cy), QPoint(cx - arrowW/2, cy + arrowH/2));
        }

        return true;
    }

private:
    bool m_isLeft;
};

QStyle* TabBarProxyStyle::appStyle() const
{
    return QApplication::style();
}

TabBarProxyStyle::TabBarProxyStyle(TabBar* tabBar) : QProxyStyle(), m_tabBar(tabBar){}

int TabBarProxyStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    if (hint == SH_Widget_Animation_Duration)
        return 0;
    return appStyle()->styleHint(hint, option, widget, returnData);
}

void TabBarProxyStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    appStyle()->drawPrimitive(element, option, painter, widget);
}

QRect TabBarProxyStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const
{
    if (element == SE_TabBarScrollLeftButton || element == SE_TabBarScrollRightButton) {
        const auto& rect = option->rect;
        const int compactW = qMax(20, rect.height() / 2 + 4);
        const int h = rect.height();
        if (element == SE_TabBarScrollLeftButton) {
            const int x = rect.x() + rect.width() - 2 * compactW;
            return { x, rect.y(), compactW, h };
        } else {
            const int x = rect.x() + rect.width() - compactW;
            return { x, rect.y(), compactW, h };
        }
    }
    if (element == SE_TabBarTabRightButton || element == SE_TabBarTabLeftButton) {
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab*>(option)) {
            bool fixRight = (element == SE_TabBarTabRightButton && tab->rightButtonSize.isEmpty());
            bool fixLeft = (element == SE_TabBarTabLeftButton && tab->leftButtonSize.isEmpty());
            if (fixRight || fixLeft) {
                int w = appStyle()->pixelMetric(PM_TabCloseIndicatorWidth, option, widget);
                int h = appStyle()->pixelMetric(PM_TabCloseIndicatorHeight, option, widget);
                QStyleOptionTab fixedOpt = *tab;
                if (fixRight)
                    fixedOpt.rightButtonSize = QSize(w, h);
                if (fixLeft)
                    fixedOpt.leftButtonSize = QSize(w, h);
                return appStyle()->subElementRect(element, &fixedOpt, widget);
            }
        }
    }
    return appStyle()->subElementRect(element, option, widget);
}

QSize TabBarProxyStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const
{
    if (type == CT_TabBarTab) {
        if (const auto *tab = qstyleoption_cast<const QStyleOptionTab*>(option)) {
            const auto *tabBar = qobject_cast<const QTabBar*>(widget);
            if (tabBar && tabBar->tabsClosable() && tab->rightButtonSize.isEmpty()) {
                QStyleOptionTab fixedOpt = *tab;
                int w = appStyle()->pixelMetric(PM_TabCloseIndicatorWidth, option, widget);
                int h = appStyle()->pixelMetric(PM_TabCloseIndicatorHeight, option, widget);
                fixedOpt.rightButtonSize = QSize(w, h);
                return appStyle()->sizeFromContents(type, &fixedOpt, size, widget);
            }
        }
    }
    return appStyle()->sizeFromContents(type, option, size, widget);
}

int TabBarProxyStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    if (metric == PM_TabBarScrollButtonWidth) {
        const int tabH = appStyle()->pixelMetric(PM_TabBarTabVSpace, option, widget);
        return qMax(20, tabH > 0 ? tabH : 24);
    }
    return appStyle()->pixelMetric(metric, option, widget);
}

QIcon TabBarProxyStyle::standardIcon(StandardPixmap standardIcon, const QStyleOption *option, const QWidget *widget) const
{
    return appStyle()->standardIcon(standardIcon, option, widget);
}

QPalette TabBarProxyStyle::standardPalette() const
{
    return appStyle()->standardPalette();
}

void TabBarProxyStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (element == CE_TabBarTabLabel && m_tabBar && m_tabBar->count() > 0) {
        const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab*>(option);
        if (tab) {
            int tabIndex = m_tabBar->tabIndexFromRect(tab->rect);

            if (tabIndex >= 0 && tabIndex < m_tabBar->count() && m_tabBar->isTabHighlighted(tabIndex) && tabIndex != m_tabBar->currentIndex()) {
                const int iconSz = appStyle()->pixelMetric(PM_TabBarIconSize, tab, widget);
                const int iconSpacing = 6;
                const int iconPadding = 8;

                QRect iconRect;
                if (!tab->icon.isNull()) {
                    iconRect = QRect(tab->rect.left() + iconPadding,
                                    tab->rect.center().y() - iconSz/2,
                                    iconSz, iconSz);
                    tab->icon.paint(painter, iconRect);
                }

                QRect textRect = tab->rect;
                if (!tab->icon.isNull()) {
                    textRect.setLeft(iconRect.right() + iconSpacing);
                }
                textRect.adjust(iconPadding, 0, -iconPadding, 0);

                painter->save();
                QColor highlightColor = m_tabBar->currentHighlightColor();
                painter->setPen(highlightColor);
                painter->setFont(m_tabBar->font());
                painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, tab->text);
                painter->restore();
                return;
            }
        }
    }

    appStyle()->drawControl(element, option, painter, widget);
}

class QtWidgets::TabBar::Private
{
public:
    explicit Private(Core::TabBar *controller)
        : m_controller(controller)
    {
    }

    void onTabMoved(int from, int to);

    Core::TabBar *const m_controller;
    KDBindings::ScopedConnection m_currentDockWidgetChangedConnection;
    int wheelDeltaAccumulator = 0;
    int scrollOffset = 0;
    int targetScrollOffset = 0;
    QTimer *scrollAnimationTimer = nullptr;
};

}

TabBar::TabBar(Core::TabBar *controller, QWidget *parent)
    : View(controller, Core::ViewType::TabBar, parent)
    , TabBarViewInterface(controller)
    , d(new Private(controller))
{
    setShape(Config::self().tabsAtBottom() ? QTabBar::RoundedSouth : QTabBar::RoundedNorth);

    setStyle(new TabBarProxyStyle(this));

    setUsesScrollButtons(true);
    setExpanding(false);
    setElideMode(Qt::ElideNone);

    d->scrollAnimationTimer = new QTimer(this);
    d->scrollAnimationTimer->setInterval(16);
    connect(d->scrollAnimationTimer, &QTimer::timeout, this, &TabBar::performSmoothScroll);
}

TabBar::~TabBar()
{
    delete d;
}

void TabBar::init()
{
    connect(this, &QTabBar::currentChanged, m_tabBar, &Core::TabBar::setCurrentIndex);
    connect(this, &QTabBar::tabMoved, this, [this](int from, int to) {
        d->onTabMoved(from, to);
    });

    d->m_currentDockWidgetChangedConnection = d->m_controller->dptr()->currentDockWidgetChanged.connect([this](KDDockWidgets::Core::DockWidget *dw) {
        Q_EMIT currentDockWidgetChanged(dw);
    });

    connect(this, &QTabBar::currentChanged, this, [this](int index) {
        Q_UNUSED(index)
        if (!m_highlightedTabs.isEmpty()) {
            update();
        }
    });

    QTimer::singleShot(0, this, [this]() {
        updateScrollButtonsColors();
    });
}

int TabBar::tabAt(QPoint localPos) const
{
    return QTabBar::tabAt(localPos);
}

void TabBar::mousePressEvent(QMouseEvent *e)
{
    d->m_controller->onMousePress(e->pos());
    QTabBar::mousePressEvent(e);
}

void TabBar::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton) {
        const int index = tabAt(e->pos());
        if (index >= 0) {
            if (auto stack = d->m_controller->stack()) {
                if (auto dw = stack->tabBar()->dockWidgetAt(index)) {
                    if (dw->options() & DockWidgetOption_NotClosable) {
                        qWarning() << "TabBar::mouseReleaseEvent: Refusing to close dock widget with "
                                      "Option_NotClosable option. name="
                                   << dw->uniqueName();
                    } else {
                        dw->view()->close();
                    }
                }
            }
            e->accept();
            return;
        }
    }

    QTabBar::mouseReleaseEvent(e);
}

void TabBar::mouseMoveEvent(QMouseEvent *e)
{
    if (count() > 1) {
        QTabBar::mouseMoveEvent(e);
    }
}

void TabBar::mouseDoubleClickEvent(QMouseEvent *e)
{
    e->setAccepted(d->m_controller->onMouseDoubleClick(e->pos()));
}

void TabBar::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right) {
        if (count() > 0 && usesScrollButtons()) {
            QAbstractButton *scrollLeftBtn = nullptr;
            QAbstractButton *scrollRightBtn = nullptr;

            const auto children = findChildren<QAbstractButton *>();
            for (QAbstractButton *btn : children) {
                if (btn->isVisible()) {
                    QRect btnRect = btn->geometry();
                    QRect tabBarRect = rect();

                    if (btnRect.left() < tabBarRect.width() / 2) {
                        scrollLeftBtn = btn;
                    } else {
                        scrollRightBtn = btn;
                    }
                }
            }

            if (e->key() == Qt::Key_Left && scrollLeftBtn && scrollLeftBtn->isEnabled()) {
                scrollLeftBtn->click();
                e->accept();
                return;
            } else if (e->key() == Qt::Key_Right && scrollRightBtn && scrollRightBtn->isEnabled()) {
                scrollRightBtn->click();
                e->accept();
                return;
            }

            const int scrollStep = 50;
            if (e->key() == Qt::Key_Left) {
                scroll(-scrollStep, 0);
            } else {
                scroll(scrollStep, 0);
            }
            e->accept();
            return;
        }
    }

    QTabBar::keyPressEvent(e);
}

void TabBar::wheelEvent(QWheelEvent *e)
{
    int delta = e->angleDelta().y();
    if (delta == 0) {
        delta = -e->angleDelta().x();
    }

    if (delta == 0) {
        QTabBar::wheelEvent(e);
        return;
    }

    const int scrollAmount = delta / 8;
    d->targetScrollOffset += scrollAmount;

    if (!d->scrollAnimationTimer->isActive()) {
        d->scrollAnimationTimer->start();
    }

    e->accept();
}

void TabBar::performSmoothScroll()
{
    const int diff = d->targetScrollOffset - d->scrollOffset;

    if (qAbs(diff) < 2) {
        d->scrollOffset = d->targetScrollOffset;
        d->scrollAnimationTimer->stop();
        return;
    }

    const int step = diff / 4;
    const int actualStep = (step == 0) ? (diff > 0 ? 1 : -1) : step;
    d->scrollOffset += actualStep;

    QList<QToolButton *> scrollButtons = findChildren<QToolButton *>();
    QToolButton *leftButton = nullptr;
    QToolButton *rightButton = nullptr;

    for (QToolButton *btn : scrollButtons) {
        if (btn->arrowType() == Qt::LeftArrow) {
            leftButton = btn;
        } else if (btn->arrowType() == Qt::RightArrow) {
            rightButton = btn;
        }
    }

    if (actualStep > 0 && leftButton && leftButton->isEnabled()) {
        leftButton->click();
    } else if (actualStep < 0 && rightButton && rightButton->isEnabled()) {
        rightButton->click();
    } else {
        d->targetScrollOffset = d->scrollOffset;
        d->scrollAnimationTimer->stop();
    }
}

bool TabBar::event(QEvent *ev)
{
    auto parent = parentWidget();
    if (!parent) {
        return QTabBar::event(ev);
    }

    const bool result = QTabBar::event(ev);

    if (ev->type() == QEvent::Show) {
        parent->setFocusProxy(this);
    } else if (ev->type() == QEvent::Hide) {
        parent->setFocusProxy(nullptr);
    } else if (ev->type() == QEvent::PaletteChange || ev->type() == QEvent::StyleChange) {
        QTimer::singleShot(0, this, [this]() {
            updateScrollButtonsColors();
        });
    }

    return result;
}

QString TabBar::text(int index) const
{
    return tabText(index);
}

QRect TabBar::rectForTab(int index) const
{
    return QTabBar::tabRect(index);
}

void TabBar::moveTabTo(int from, int to)
{
    moveTab(from, to);
}

void TabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);

    if (tabsClosable()) {
        auto closeSide = static_cast<ButtonPosition>(
            style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, this));
        QWidget *btn = tabButton(index, closeSide);
        if (btn) {
            if (btn->size().isEmpty())
                btn->resize(btn->sizeHint());
            bool hasFilter = false;
            for (auto* child : btn->children()) {
                if (dynamic_cast<TabCloseButtonFilter*>(child)) {
                    hasFilter = true;
                    break;
                }
            }
            if (!hasFilter)
                btn->installEventFilter(new TabCloseButtonFilter(btn));
        }
    }

    Q_EMIT dockWidgetInserted(index);
    Q_EMIT countChanged();

    QSet<int> newHighlighted;
    for (int i : m_highlightedTabs) {
        if (i >= index) {
            newHighlighted.insert(i + 1);
        } else {
            newHighlighted.insert(i);
        }
    }
    m_highlightedTabs = newHighlighted;
}

void TabBar::tabRemoved(int index)
{
    QTabBar::tabRemoved(index);
    Q_EMIT dockWidgetRemoved(index);
    Q_EMIT countChanged();

    QSet<int> newHighlighted;
    for (int i : m_highlightedTabs) {
        if (i < index) {
            newHighlighted.insert(i);
        } else if (i > index) {
            newHighlighted.insert(i - 1);
        }
    }
    m_highlightedTabs = newHighlighted;

    if (m_highlightedTabs.isEmpty()) {
        stopBlinkTimer();
    }
}

void TabBar::setCurrentIndex(int index)
{
    QTabBar::setCurrentIndex(index);
}

void TabBar::updateScrollButtonsColors()
{
    const auto allButtons = findChildren<QToolButton *>();
    for (QToolButton *btn : allButtons) {
        if (btn->arrowType() == Qt::NoArrow)
            continue;

        bool isLeft = (btn->arrowType() == Qt::LeftArrow);

        bool hasFilter = false;
        for (auto* child : btn->children()) {
            if (dynamic_cast<ScrollButtonFilter*>(child)) {
                hasFilter = true;
                break;
            }
        }
        if (!hasFilter)
            btn->installEventFilter(new ScrollButtonFilter(isLeft, btn));

        const int compactW = qMax(18, height() * 2 / 3);
        btn->setFixedWidth(compactW);
    }
}

QTabWidget *TabBar::tabWidget() const
{
    if (auto tw = dynamic_cast<Stack *>(d->m_controller->stack()->view()))
        return tw;

    qWarning() << Q_FUNC_INFO << "Unexpected null QTabWidget";
    return nullptr;
}

void TabBar::renameTab(int index, const QString &text)
{
    setTabText(index, text);
}

void TabBar::changeTabIcon(int index, const QIcon &icon)
{
    setTabIcon(index, icon);
}

void TabBar::removeDockWidget(Core::DockWidget *dw)
{
    auto tabWidget = static_cast<QTabWidget *>(View_qt::asQWidget(m_tabBar->stack()));
    tabWidget->removeTab(m_tabBar->indexOfDockWidget(dw));
}

void TabBar::insertDockWidget(int index, Core::DockWidget *dw, const QIcon &icon,
                              const QString &title)
{
    auto tabWidget = static_cast<QTabWidget *>(View_qt::asQWidget(m_tabBar->stack()));
    tabWidget->insertTab(index, View_qt::asQWidget(dw), icon, title);
}

void TabBar::setTabsAreMovable(bool are)
{
    QTabBar::setMovable(are);
}

Core::TabBar *TabBar::tabBar() const
{
    return d->m_controller;
}

void TabBar::Private::onTabMoved(int from, int to)
{
    if (from == to || m_controller->isMovingTab())
        return;

    m_controller->dptr()->moveTabTo(from, to);
}

void TabBar::paintEvent(QPaintEvent *event)
{
    QTabBar::paintEvent(event);

    if (!m_highlightedTabs.isEmpty()) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing);

        QColor textColor = currentHighlightColor();
        painter.setPen(textColor);
        painter.setFont(font());

        for (int index : m_highlightedTabs) {
            if (index >= 0 && index < count() && index != currentIndex()) {
                QStyleOptionTab opt;
                initStyleOption(&opt, index);

                QRect textRect = style()->subElementRect(QStyle::SE_TabBarTabText, &opt, this);

                if (!opt.icon.isNull()) {
                    QRect iconRect = style()->subElementRect(QStyle::SE_TabBarTabLeftButton, &opt, this);
                    if (iconRect.isValid()) {
                        int iconRight = iconRect.right() + style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, this) / 2;
                        if (textRect.left() < iconRight) {
                            textRect.setLeft(iconRight);
                        }
                    } else {
                        int iconWidth = opt.iconSize.width();
                        if (iconWidth <= 0)
                            iconWidth = style()->pixelMetric(QStyle::PM_TabBarIconSize, &opt, this);
                        int spacing = style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, this) / 2;
                        textRect.setLeft(textRect.left() + iconWidth + spacing);
                    }
                }

                painter.drawText(textRect, Qt::AlignCenter, opt.text);
            }
        }
    }
}

QColor TabBar::currentHighlightColor() const
{
    return m_blinkState ? QColor("#FF6600") : QColor("#FFAA44");
}

int TabBar::tabIndexFromRect(const QRect& rect) const
{
    for (int i = 0; i < count(); ++i) {
        if (tabRect(i) == rect) {
            return i;
        }
    }
    return -1;
}

void TabBar::setTabHighlighted(int index, bool highlighted)
{
    if (index < 0 || index >= count())
        return;

    bool wasHighlighted = m_highlightedTabs.contains(index);
    if (wasHighlighted == highlighted)
        return;

    if (highlighted) {
        if (index == currentIndex())
            return;

        m_highlightedTabs.insert(index);

        startBlinkTimer();
    } else {
        m_highlightedTabs.remove(index);

        if (m_highlightedTabs.isEmpty()) {
            stopBlinkTimer();
        }
    }

    update();
    Q_EMIT tabHighlightChanged(index, highlighted);
}

bool TabBar::isTabHighlighted(int index) const
{
    return m_highlightedTabs.contains(index);
}

void TabBar::clearAllHighlights()
{
    QSet<int> tabs = m_highlightedTabs;
    for (int index : tabs) {
        setTabHighlighted(index, false);
    }
}

void TabBar::startBlinkTimer()
{
    if (!m_blinkTimer) {
        m_blinkTimer = new QTimer(this);
        m_blinkTimer->setInterval(600);
        connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
            m_blinkState = !m_blinkState;
            repaint();
        });
    }

    if (!m_blinkTimer->isActive()) {
        m_blinkState = true;
        m_blinkTimer->start();
    }
}

void TabBar::stopBlinkTimer()
{
    if (m_blinkTimer && m_blinkTimer->isActive()) {
        m_blinkTimer->stop();
    }
    m_blinkState = false;
}

