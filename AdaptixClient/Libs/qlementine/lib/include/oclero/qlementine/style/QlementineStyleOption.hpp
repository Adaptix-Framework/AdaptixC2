// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <oclero/qlementine/utils/RadiusesF.hpp>

#include <QStyleOption>

namespace oclero::qlementine {
/// Allows to customize the radius of the focus border.
class QStyleOptionFocusRoundedRect : public QStyleOptionFocusRect {
public:
  RadiusesF radiuses;
  int hMargin{ 0 };
  int vMargin{ 0 };
  QColor borderColor;

  QStyleOptionFocusRoundedRect() = default;

  static QStyleOptionFocusRoundedRect fromBase(QStyleOption const& opt, QRect const& rect, RadiusesF const& radiuses) {
    QStyleOptionFocusRoundedRect newOpt;
    newOpt.QStyleOption::operator=(opt);
    newOpt.radiuses = radiuses;
    newOpt.rect = rect;
    return newOpt;
  }

  QStyleOptionFocusRoundedRect(const QStyleOptionFocusRoundedRect& other)
    : QStyleOptionFocusRect(other) {
    *this = other;
  }

  QStyleOptionFocusRoundedRect& operator=(const QStyleOptionFocusRoundedRect&) = default;

  virtual ~QStyleOptionFocusRoundedRect() = default;
};

/// Allows to customize the radius of a button.
class QStyleOptionRoundedButton : public QStyleOptionButton {
public:
  enum StyleOptionType { Type = SO_CustomBase + 1 };

  RadiusesF radiuses;

  QStyleOptionRoundedButton() {
    type = Type;
    radiuses = 0.;
  }

  QStyleOptionRoundedButton(const QStyleOptionRoundedButton& other)
    : QStyleOptionButton(other) {
    type = Type;
    radiuses = other.radiuses;
  }

  QStyleOptionRoundedButton& operator=(const QStyleOptionRoundedButton&) = default;

  virtual ~QStyleOptionRoundedButton() = default;
};

/// Adds the ability to transition from one visual position to another.
class QStyleOptionSliderF : public QStyleOptionSlider {
public:
  static constexpr auto INITIALIZED = 2;
  qreal sliderPositionF{ 0. };
  int status{ 0 }; // Needed to track that it was created by us.

  QStyleOptionSliderF() = default;

  QStyleOptionSliderF(const QStyleOptionSliderF& other)
    : QStyleOptionSlider(other) {
    *this = other;
  }

  QStyleOptionSliderF& operator=(const QStyleOptionSliderF&) = default;

  virtual ~QStyleOptionSliderF() = default;
};

/// Adds the ability to have a second line of text in the button.
class QStyleOptionCommandLinkButton : public QStyleOptionButton {
public:
  QString description;

  using QStyleOptionButton::QStyleOptionButton;
};
} // namespace oclero::qlementine
