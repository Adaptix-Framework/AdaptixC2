// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <oclero/qlementine/style/Theme.hpp>

#include <QStyle>
#include <QStyleOption>

namespace oclero::qlementine {
MouseState getMouseState(QStyle::State const& state);
MouseState getMouseState(bool const pressed, bool const hovered, bool const enabled);
MouseState getToolButtonMouseState(QStyle::State const& state);
MouseState getMenuItemMouseState(QStyle::State const& state);
MouseState getComboBoxItemMouseState(QStyle::State const& state);
MouseState getTabItemMouseState(QStyle::State const& state, const bool tabIsHovered);
ColorRole getColorRole(QStyle::State const& state, bool const isDefault);
ColorRole getColorRole(bool checked, bool const isDefault);
ColorRole getColorRole(CheckState const checked);
MouseState getSliderHandleState(QStyle::State const& state, QStyle::SubControls const activeSubControls);
MouseState getScrollBarHandleState(QStyle::State const& state, QStyle::SubControls const activeSubControls);
FocusState getFocusState(QStyle::State const& state);
FocusState getFocusState(bool focused);
CheckState getCheckState(QStyle::State const& state);
CheckState getCheckState(Qt::CheckState const& state);
CheckState getCheckState(bool checked);
ActiveState getActiveState(QStyle::State const& state);
SelectionState getSelectionState(QStyle::State const& state);
AlternateState getAlternateState(QStyleOptionViewItem::ViewItemFeatures const& state);
QStyle::State getState(const bool enabled, const bool hover, const bool pressed);
QIcon::Mode getIconMode(MouseState const mouse);
QIcon::State getIconState(CheckState const checked);
QPalette::ColorGroup getPaletteColorGroup(QStyle::State const& state);
QPalette::ColorGroup getPaletteColorGroup(MouseState const mouse);

QString mouseStateToString(MouseState const state);
QString focusStateToString(FocusState const state);
QString activeStateToString(ActiveState const state);
QString selectionStateToString(SelectionState const state);
QString checkStateToString(CheckState const state);
QString printState(QStyle::State const& state);
} // namespace oclero::qlementine
