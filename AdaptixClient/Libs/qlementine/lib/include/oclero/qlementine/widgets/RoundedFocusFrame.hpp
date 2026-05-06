// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <oclero/qlementine/utils/RadiusesF.hpp>

#include <QFocusFrame>

namespace oclero::qlementine {
/// A rounded QFocusFrame.
class RoundedFocusFrame : public QFocusFrame {
  Q_OBJECT

  Q_PROPERTY(RadiusesF radiuses READ radiuses WRITE setRadiuses NOTIFY radiusesChanged)

public:
  using QFocusFrame::QFocusFrame;

  const RadiusesF& radiuses() const;
  Q_SLOT void setRadiuses(const RadiusesF&);
  Q_SIGNAL void radiusesChanged();

private:
  RadiusesF _radiuses;
};
} // namespace oclero::qlementine
