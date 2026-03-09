// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#include <oclero/qlementine/widgets/PopoverButton.hpp>

#include <oclero/qlementine/widgets/Popover.hpp>

#include <QPainter>
#include <QStyleOptionButton>

namespace oclero::qlementine {
PopoverButton::PopoverButton(QWidget* parent)
  : PopoverButton({}, {}, parent) {}

PopoverButton::PopoverButton(const QString& text, QWidget* parent)
  : PopoverButton(text, {}, parent) {}

PopoverButton::PopoverButton(const QString& text, const QIcon& icon, QWidget* parent)
  : QPushButton(icon, text, parent)
  , _popover(new Popover(this)) {
  _popover->setAnchorWidget(this);
  QObject::connect(this, &PopoverButton::clicked, this, [this]() {
    _popover->openPopover();
  });
  QObject::connect(_popover, &Popover::openedChanged, this, [this]() {
    update();
    Q_EMIT popoverOpenedChanged(_popover->isOpened());
  });
  QObject::connect(_popover, &Popover::contentWidgetChanged, this, [this]() {
    Q_EMIT popoverContentWidgetChanged();
  });
}

Popover* PopoverButton::popover() const {
  return _popover;
}

QWidget* PopoverButton::popoverContentWidget() const {
  return _popover->contentWidget();
}

void PopoverButton::setPopoverContentWidget(QWidget* widget) {
  _popover->setContentWidget(widget);
}

bool PopoverButton::showArrowIndicator() const {
  return _showArrowIndicator;
}

void PopoverButton::setShowArrowIndicator(bool show) {
  if (show != _showArrowIndicator) {
    _showArrowIndicator = show;
    update();
  }
}

void PopoverButton::setPopoverOpened(bool opened) {
  _popover->setOpened(opened);
}

void PopoverButton::paintEvent(QPaintEvent* /*e*/) {
  const auto* style = this->style();
  QPainter p(this);

  auto font = QFont(this->font());
  font.setBold(false);
  p.setFont(font);

  // Background.
  QStyleOptionButton bgOpt;
  initStyleOption(&bgOpt);
  bgOpt.features.setFlag(QStyleOptionButton::ButtonFeature::HasMenu, _showArrowIndicator);
  const auto isPressed = bgOpt.state.testFlag(QStyle::State_Sunken);
  bgOpt.state.setFlag(QStyle::State_Sunken, isPressed || _popover->isOpened());
  style->drawControl(QStyle::CE_PushButtonBevel, &bgOpt, &p, this);

  // Foreground.
  if (_showArrowIndicator) {
    QStyleOptionComboBox fgOpt;
    fgOpt.currentText = text();
    fgOpt.currentIcon = icon();
    fgOpt.editable = false;
    fgOpt.frame = false;
    fgOpt.iconSize = iconSize();
    fgOpt.subControls = QStyle::SC_ComboBoxFrame | QStyle::SC_ComboBoxArrow;
    fgOpt.activeSubControls = QStyle::SC_ComboBoxFrame;
    fgOpt.rect = bgOpt.rect;
    fgOpt.state = bgOpt.state;
    style->drawControl(QStyle::CE_ComboBoxLabel, &fgOpt, &p, this);
  } else {
    style->drawControl(QStyle::CE_PushButtonLabel, &bgOpt, &p, this);
  }
}
} // namespace oclero::qlementine
