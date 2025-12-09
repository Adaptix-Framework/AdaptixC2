/*
  This file is part of KDDockWidgets.

  SPDX-FileCopyrightText: 2019 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Sérgio Martins <sergio.martins@kdab.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "Stack.h"
#include "qtwidgets/views/DockWidget.h"
#include "qtwidgets/views/TabBar.h"
#include "core/Controller.h"
#include "core/Stack.h"
#include "core/TitleBar.h"
#include "core/Group.h"
#include "core/Group_p.h"
#include "core/Window_p.h"
#include "core/DockRegistry_p.h"
#include "core/Stack_p.h"
#include "Config.h"
#include "core/View_p.h"

#include "qtwidgets/ViewFactory.h"

#include <QMouseEvent>
#include <QResizeEvent>
#include <QTabBar>
#include <QHBoxLayout>
#include <QAbstractButton>
#include <QMenu>
#include <QSizePolicy>
#include <QTimer>

#include "kdbindings/signal.h"

using namespace KDDockWidgets;
using namespace KDDockWidgets::QtWidgets;

namespace KDDockWidgets::QtWidgets {
class Stack::Private
{
public:
    KDBindings::ScopedConnection tabBarAutoHideChanged;
    KDBindings::ScopedConnection buttonsToHideIfDisabledConnection;
    KDBindings::ScopedConnection titleBarVisibilityConnection;

    QWidget *buttonsWidget = nullptr;
    QHBoxLayout *buttonsLayout = nullptr;
    QAbstractButton *floatButton = nullptr;
    QAbstractButton *closeButton = nullptr;

    bool updatingButtons = false;
};
}


Stack::Stack(Core::Stack *controller, QWidget *parent)
    : View<QTabWidget>(controller, Core::ViewType::Stack, parent)
    , StackViewInterface(controller)
    , d(new Private())
{
    setTabPosition(Config::self().tabsAtBottom() ? TabPosition::South : TabPosition::North);
}

Stack::~Stack()
{
    delete d;
}

void Stack::init()
{
    setTabBar(tabBar());
    setTabsClosable(Config::self().flags() & Config::Flag_TabsHaveCloseButton);

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QTabWidget::customContextMenuRequested, this, &Stack::showContextMenu);

    // In case tabs closable is set by the factory, a tabClosedRequested() is emitted when the user
    // presses [x]
    connect(this, &QTabWidget::tabCloseRequested, this, [this](int index) {
        if (auto dw = m_stack->tabBar()->dockWidgetAt(index)) {
            if (dw->options() & DockWidgetOption_NotClosable) {
                qWarning() << "QTabWidget::tabCloseRequested: Refusing to close dock widget with "
                              "Option_NotClosable option. name="
                           << dw->uniqueName();
            } else {
                dw->view()->close();
            }
        } else {
            qWarning() << "QTabWidget::tabCloseRequested Couldn't find dock widget for index"
                       << index << "; count=" << count();
        }
    });

    QTabWidget::setTabBarAutoHide(m_stack->tabBarAutoHide());

    d->tabBarAutoHideChanged = m_stack->d->tabBarAutoHideChanged.connect(
        [this](bool is) { QTabWidget::setTabBarAutoHide(is); });

    if (!QTabWidget::tabBar()->isVisible())
        setFocusProxy(nullptr);

    setupTabBarButtons();

    setDocumentMode(m_stack->options() & StackOption_DocumentMode);
}

void Stack::mouseDoubleClickEvent(QMouseEvent *ev)
{
    if (m_stack->onMouseDoubleClick(ev->pos())) {
        ev->accept();
    } else {
        ev->ignore();
    }
}

void Stack::mousePressEvent(QMouseEvent *ev)
{
    QTabWidget::mousePressEvent(ev);

    if ((Config::self().flags() & Config::Flag_TitleBarIsFocusable)
        && !m_stack->group()->isFocused()) {
        // User clicked on the tab widget itself
        m_stack->group()->FocusScope::focus(Qt::MouseFocusReason);
    }
}

void Stack::setupTabBarButtons()
{
    if (!(Config::self().flags() & Config::Flag_ShowButtonsOnTabBarIfTitleBarHidden))
        return;

    if (d->buttonsWidget != nullptr)
        return;

    auto factory = static_cast<ViewFactory *>(Config::self().viewFactory());
    d->closeButton = factory->createTitleBarButton(this, TitleBarButtonType::Close);
    d->floatButton = factory->createTitleBarButton(this, TitleBarButtonType::Float);

    d->buttonsWidget = new QWidget(this);
    d->buttonsWidget->setObjectName(QStringLiteral("TabBar Buttons"));

    d->buttonsLayout = new QHBoxLayout(d->buttonsWidget);
    d->buttonsLayout->setContentsMargins(0, 0, 4, 0);
    d->buttonsLayout->setSpacing(2);

    d->floatButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    d->closeButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    d->buttonsLayout->addWidget(d->floatButton);
    d->buttonsLayout->addWidget(d->closeButton);

    setCornerWidget(d->buttonsWidget, Qt::TopRightCorner);

    connect(d->floatButton, &QAbstractButton::clicked, this, [this] {
        Core::TitleBar *tb = m_stack->group()->titleBar();
        tb->onFloatClicked();
    });

    connect(d->closeButton, &QAbstractButton::clicked, this, [this] {
        Core::TitleBar *tb = m_stack->group()->titleBar();
        tb->onCloseClicked();
    });

    d->buttonsToHideIfDisabledConnection = m_stack->d->buttonsToHideIfDisabledChanged.connect([this] {
        updateTabBarButtons();
    });

    auto tb = qobject_cast<QtWidgets::TabBar *>(tabBar());
    if (tb) {
        connect(tb, &QtWidgets::TabBar::countChanged, this, &Stack::updateTabBarButtons);
    }

    if (auto group = m_stack->group()) {
        d->titleBarVisibilityConnection = group->dptr()->actualTitleBarChanged.connect([this] {
            QTimer::singleShot(0, this, &Stack::updateTabBarButtons);
        });
    }

    updateTabBarButtons();

    QTimer::singleShot(0, this, [this] {
        updateTabBarButtons();
        updateButtonsPosition();
    });
}

void Stack::updateTabBarButtons()
{
    if (!d->buttonsWidget)
        return;

    if (d->updatingButtons)
        return;
    d->updatingButtons = true;

    auto group = m_stack->group();
    if (!group) {
        d->updatingButtons = false;
        return;
    }

    const bool hideTitleBarFlag = Config::self().flags() & Config::Flag_HideTitleBarWhenTabsVisible;
    const bool titleBarShouldBeHidden = hideTitleBarFlag && group->hasTabsVisible();

    d->buttonsWidget->setVisible(titleBarShouldBeHidden);

    if (d->closeButton) {
        const bool enabled = !group->anyNonClosable();
        const bool visible = enabled || !m_stack->buttonHidesIfDisabled(TitleBarButtonType::Close);
        d->closeButton->setEnabled(enabled);
        d->closeButton->setVisible(visible);
    }

    d->updatingButtons = false;
}

void Stack::updateMargins()
{
    // Deprecated - used updateButtonsPosition()
}

void Stack::updateButtonsPosition()
{
    if (!d->buttonsWidget)
        return;

    QTabBar *tb = tabBar();
    if (!tb)
        return;

    int tabBarHeight = tb->height();
    if (tabBarHeight > 0) {
        d->buttonsWidget->setFixedHeight(tabBarHeight);
    }
}

void Stack::resizeEvent(QResizeEvent *event)
{
    QTabWidget::resizeEvent(event);
    updateButtonsPosition();
}

void Stack::showContextMenu(QPoint pos)
{
    if (!(Config::self().flags() & Config::Flag_AllowSwitchingTabsViaMenu))
        return;

    QTabBar *tabBar = QTabWidget::tabBar();
    // We don't want context menu if there is only one tab
    if (tabBar->count() <= 1)
        return;

    // Convert pos to tabBar coordinates for tabAt() check
    QPoint tabBarPos = tabBar->mapFrom(this, pos);

    // Click on a tab => No menu
    if (tabBar->tabAt(tabBarPos) >= 0)
        return;

    // Right click is allowed only on the tabs area
    // Create a rectangle covering the tab bar area, expanded to full width of the widget
    // This rectangle is in widget coordinates (same as pos)
    QRect tabAreaRect = QRect(tabBar->mapTo(this, QPoint(0, 0)), tabBar->size());
    if (tabPosition() == QTabWidget::North || tabPosition() == QTabWidget::South) {
        tabAreaRect.setLeft(0);
        tabAreaRect.setRight(width());
    }
    if (!tabAreaRect.contains(pos))
        return;

    QMenu menu(this);
    for (int i = 0; i < tabBar->count(); ++i) {
        QAction *action = menu.addAction(tabText(i), this, [this, i] { setCurrentIndex(i); });
        if (i == currentIndex())
            action->setDisabled(true);
    }
    menu.exec(mapToGlobal(pos));
}

QTabBar *Stack::tabBar() const
{
    return static_cast<QTabBar *>(View_qt::asQWidget((m_stack->tabBar())));
}

void Stack::setDocumentMode(bool is)
{
    QTabWidget::setDocumentMode(is);
}

Core::Stack *Stack::stack() const
{
    return m_stack;
}

bool Stack::isPositionDraggable(QPoint p) const
{
    switch (tabPosition()) {
        case QTabWidget::North:
            return p.y() >= 0 && p.y() <= tabBar()->height();
        case QTabWidget::South:
            return p.y() >= tabBar()->y();
        default:
            qWarning() << Q_FUNC_INFO << "Not implemented yet. Only North and South is supported";
            return false;
    }
}

QAbstractButton *Stack::button(TitleBarButtonType type) const
{
    switch (type) {

    case TitleBarButtonType::Close:
        return d->closeButton;
    case TitleBarButtonType::Float:
        return d->floatButton;
    case TitleBarButtonType::Minimize:
    case TitleBarButtonType::Maximize:
    case TitleBarButtonType::Normal:
    case TitleBarButtonType::AutoHide:
    case TitleBarButtonType::UnautoHide:
    case TitleBarButtonType::AllTitleBarButtonTypes:
        return nullptr;
    }

    return nullptr;
}
