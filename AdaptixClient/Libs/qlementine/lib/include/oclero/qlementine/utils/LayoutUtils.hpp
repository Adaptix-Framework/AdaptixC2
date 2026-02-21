// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QMargins>
#include <QLayout>

#include <tuple>

class QWidget;

namespace oclero::qlementine {
/// Retrieves the widget's QStyle margins.
QMargins getLayoutMargins(const QWidget* widget);

/// Retrieves the widget's QStyle horizontal spacing.
int getLayoutHSpacing(const QWidget* widget);

/// Retrieves the widget's QStyle vertical spacing.
int getLayoutVSpacing(const QWidget* widget);

/// Retrieves the widget's QStyle horizontal spacing and margins.
std::tuple<int, QMargins> getHLayoutProps(const QWidget* widget);

/// Retrieves the widget's QStyle vertical spacing and margins.
std::tuple<int, QMargins> getVLayoutProps(const QWidget* widget);

/// Retrieves the widget's QStyle vertical/horizontal spacings and margins.
std::tuple<int, int, QMargins> getFormLayoutProps(const QWidget* widget);

/// Remove and deletes all the elements in the layout.
void clearLayout(QLayout* layout);
} // namespace oclero::qlementine
