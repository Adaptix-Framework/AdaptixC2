// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QRect>
#include <QPainter>

#include <oclero/qlementine/style/Theme.hpp>

namespace oclero::qlementine {
enum class StatusBadge {
  Success,
  Info,
  Warning,
  Error,
};

enum class StatusBadgeSize {
  Small,
  Medium,
};

/// Gets the bounding box and icon size.
std::pair<QSize, QSize> getStatusBadgeSizes(StatusBadgeSize statusBadgeSize, const Theme& theme);

/// Draws a status badge.
void drawStatusBadge(QPainter* p, const QRect& rect, StatusBadge statusBadge, StatusBadgeSize size, const Theme& theme);
} // namespace oclero::qlementine
