// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>
#include <QStyleOption>

namespace oclero::qlementine {
/// Hover events (enter/leave events) are disabled by default on widgets.
/// However, some widgets need them.
bool shouldHaveHoverEvents(const QWidget* w);

/// Mouse tracking events (mouse moved events) are disabled by default on widgets.
/// However, some widgets need them.
bool shouldHaveMouseTracking(const QWidget* w);

/// Should the widget text be displayed in bold.
bool shouldHaveBoldFont(const QWidget* w);

/// Focus border outside the widget.
bool shouldHaveExternalFocusFrame(const QWidget* w);

/// Should the widget be focusable only with Tab.
bool shouldHaveTabFocus(const QWidget* w);

/// Should we prevent the widget to resize vertically.
bool shouldNotBeVerticallyCompressed(const QWidget* w);

/// Horizontal paddings (left, right) are different according to the content of the widget.
std::tuple<int, int> getHPaddings(const bool hasIcon, const bool hasText, const bool hasIndicator, const int padding);

/// Should the widget not receive wheel events when not focused.
bool shouldNotHaveWheelEvents(const QWidget* w);

/// Gets the tab index from the QStyleOption (if v4 or superior), or from the tab position in the QTabBar.
int getTabIndex(const QStyleOptionTab* optTab, const QWidget* parentWidget);

/// Gets the tab count.
int getTabCount(const QWidget* parentWidget);
} // namespace oclero::qlementine
