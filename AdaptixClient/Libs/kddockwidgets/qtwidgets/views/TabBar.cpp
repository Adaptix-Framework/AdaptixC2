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
namespace { // anonymous namespace to silence -Wweak-vtables
class MyProxy : public QProxyStyle
{
    Q_OBJECT
public:
    MyProxy()
    {
        setParent(qApp);
    }

    int styleHint(QStyle::StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *returnData = nullptr) const override
    {
        if (hint == QStyle::SH_Widget_Animation_Duration) {
            return 0;
        }
        return baseStyle()->styleHint(hint, option, widget, returnData);
    }
};
}

static MyProxy *proxyStyle()
{
    static auto *proxy = new MyProxy;
    return proxy;
}

TabBarProxyStyle::TabBarProxyStyle(TabBar* tabBar) : QProxyStyle(), m_tabBar(tabBar)
{
    setBaseStyle(proxyStyle());
}

void TabBarProxyStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (element == CE_TabBarTabLabel && m_tabBar && m_tabBar->count() > 0) {
        const QStyleOptionTab *tab = qstyleoption_cast<const QStyleOptionTab*>(option);
        if (tab) {
            int tabIndex = m_tabBar->tabIndexFromRect(tab->rect);

            if (tabIndex >= 0 && tabIndex < m_tabBar->count() && m_tabBar->isTabHighlighted(tabIndex) && tabIndex != m_tabBar->currentIndex()) {
                QRect textRect = subElementRect(SE_TabBarTabText, tab, widget);

                if (!tab->icon.isNull()) {
                    QRect iconRect = subElementRect(SE_TabBarTabLeftButton, tab, widget);
                    if (!iconRect.isValid()) {
                        int iconSize = proxy()->pixelMetric(PM_TabBarIconSize, tab, widget);
                        iconRect = QRect(tab->rect.left() + 6,
                                        tab->rect.center().y() - iconSize/2,
                                        iconSize, iconSize);
                    }
                    tab->icon.paint(painter, iconRect);
                }

                painter->save();
                QColor highlightColor = m_tabBar->currentHighlightColor();
                painter->setPen(highlightColor);
                painter->setFont(m_tabBar->font());
                painter->drawText(textRect, Qt::AlignCenter, tab->text);
                painter->restore();
                return;
            }
        }
    }

    QProxyStyle::drawControl(element, option, painter, widget);
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
    const auto allButtons = findChildren<QAbstractButton *>();
    QList<QAbstractButton *> scrollButtons;
    for (QAbstractButton *btn : allButtons) {
        if (auto toolBtn = qobject_cast<QToolButton *>(btn)) {
            if (toolBtn->arrowType() != Qt::NoArrow) {
                scrollButtons.append(btn);
            }
        }
    }

    QColor backgroundColor;

    QTabWidget *tabWidget = this->tabWidget();
    if (tabWidget) {
        QPalette tabWidgetPal = tabWidget->palette();
        backgroundColor = tabWidgetPal.color(QPalette::Base);
    }

    if (!backgroundColor.isValid()) {
        QPalette tabBarPal = this->palette();
        backgroundColor = tabBarPal.color(QPalette::Window);
    }

    if (!backgroundColor.isValid()) {
        QWidget *parent = this->parentWidget();
        if (parent) {
            QWidget *titleBar = parent->findChild<QWidget *>(QStringLiteral("KDDWTitleBar"));
            if (!titleBar) {
                QWidget *topLevel = QWidget::window();
                if (topLevel) {
                    titleBar = topLevel->findChild<QWidget *>(QStringLiteral("KDDWTitleBar"), Qt::FindChildrenRecursively);
                }
            }
            if (titleBar) {
                QPalette titleBarPal = titleBar->palette();
                backgroundColor = titleBarPal.color(QPalette::Window);
            }
        }
    }

    if (!backgroundColor.isValid()) {
        QPalette appPal = QApplication::palette();
        backgroundColor = appPal.color(QPalette::Window);
    }

    for (QAbstractButton *btn : scrollButtons) {
        btn->setAutoFillBackground(true);
        QPalette btnPal = btn->palette();
        btnPal.setColor(QPalette::Button, backgroundColor);
        btnPal.setColor(QPalette::Window, backgroundColor);
        btn->setPalette(btnPal);
        btn->setAttribute(Qt::WA_TranslucentBackground, false);
        btn->setAttribute(Qt::WA_OpaquePaintEvent, true);
        QString colorStr = QString("rgb(%1, %2, %3)").arg(backgroundColor.red()).arg(backgroundColor.green()).arg(backgroundColor.blue());
        btn->setStyleSheet(QString("background: %1; background-color: %1; border: none; border-top: 1px solid #808080;").arg(colorStr));
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

                // Получаем точный rect текста
                QRect textRect = style()->subElementRect(QStyle::SE_TabBarTabText, &opt, this);

                // Рисуем текст с мигающим цветом поверх старого
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

#include "TabBar.moc"
