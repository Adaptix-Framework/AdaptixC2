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

#include "kdbindings/signal.h"

using namespace KDDockWidgets;
using namespace KDDockWidgets::QtWidgets;

namespace KDDockWidgets::QtWidgets {
class Stack::Private
{
public:
    KDBindings::ScopedConnection tabBarAutoHideChanged;
    KDBindings::ScopedConnection screenChangedConnection;
    KDBindings::ScopedConnection buttonsToHideIfDisabledConnection;

    QWidget *buttonsWidget = nullptr;
    QHBoxLayout *buttonsLayout = nullptr;
    QAbstractButton *floatButton = nullptr;
    QAbstractButton *closeButton = nullptr;
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

    auto factory = static_cast<ViewFactory *>(Config::self().viewFactory());
    d->closeButton = factory->createTitleBarButton(this, TitleBarButtonType::Close);
    d->floatButton = factory->createTitleBarButton(this, TitleBarButtonType::Float);

    d->buttonsWidget = new QWidget(this);
    d->buttonsWidget->setObjectName(QStringLiteral("TabBar Buttons Overlay"));
    d->buttonsWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    d->buttonsWidget->raise();

    d->buttonsLayout = new QHBoxLayout(d->buttonsWidget);
    d->buttonsLayout->setContentsMargins(0, 0, 4, 0);
    d->buttonsLayout->setSpacing(2);

    d->buttonsLayout->addWidget(d->floatButton, 0, Qt::AlignVCenter);
    d->buttonsLayout->addWidget(d->closeButton, 0, Qt::AlignVCenter);

    connect(d->floatButton, &QAbstractButton::clicked, this, [this] {
        Core::TitleBar *tb = m_stack->group()->titleBar();
        tb->onFloatClicked();
    });

    connect(d->closeButton, &QAbstractButton::clicked, this, [this] {
        Core::TitleBar *tb = m_stack->group()->titleBar();
        tb->onCloseClicked();
    });

    d->screenChangedConnection = DockRegistry::self()->dptr()->windowChangedScreen.connect([this](Core::Window::Ptr w) {
        if (View::d->isInWindow(w))
            updateButtonsPosition();
    });

    d->buttonsToHideIfDisabledConnection = m_stack->d->buttonsToHideIfDisabledChanged.connect([this] {
        updateTabBarButtons();
    });

    if (auto tb = qobject_cast<QtWidgets::TabBar *>(tabBar()))
        connect(tb, &QtWidgets::TabBar::countChanged, this, &Stack::updateTabBarButtons);

    updateTabBarButtons();
    updateButtonsPosition();
}

void Stack::updateTabBarButtons()
{
    if (d->closeButton) {
        const bool enabled = !m_stack->group()->anyNonClosable();
        const bool visible = enabled || !m_stack->buttonHidesIfDisabled(TitleBarButtonType::Close);
        d->closeButton->setEnabled(enabled);
        d->closeButton->setVisible(visible);
    }
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

    int buttonsWidth = d->buttonsWidget->sizeHint().width();
    int buttonsHeight = tb->height();

    d->buttonsWidget->setFixedHeight(buttonsHeight);
    d->buttonsWidget->move(width() - buttonsWidth - 2, 0);
    d->buttonsWidget->raise();
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

    // Click on a tab => No menu
    if (tabBar->tabAt(pos) >= 0)
        return;

    // Right click is allowed only on the tabs area
    QRect tabAreaRect = tabBar->rect();
    tabAreaRect.setWidth(this->width());
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
