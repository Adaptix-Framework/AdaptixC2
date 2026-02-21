// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>

#include <oclero/qlementine/utils/BadgeUtils.hpp>

namespace oclero::qlementine {
/// A QWidget that displays a badge indicating Status (Error, etc.).
class StatusBadgeWidget : public QWidget {
  Q_OBJECT

public:
  explicit StatusBadgeWidget(QWidget* parent = nullptr);
  StatusBadgeWidget(StatusBadge badge, QWidget* parent = nullptr);
  StatusBadgeWidget(StatusBadge badge, StatusBadgeSize badgeSize, QWidget* parent = nullptr);

  StatusBadge badge() const;
  Q_SLOT void setBadge(StatusBadge badge);
  Q_SIGNAL void badgeChanged();

  StatusBadgeSize badgeSize() const;
  Q_SLOT void setBadgeSize(StatusBadgeSize size);
  Q_SIGNAL void badgeSizeChanged();

  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* e) override;

private:
  StatusBadgeSize _badgeSize{ StatusBadgeSize::Medium };
  StatusBadge _badge{ StatusBadge::Info };
};
} // namespace oclero::qlementine
