// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>
#include <QScreen>
#include <QGuiApplication>

namespace oclero::qlementine {
QWidget* makeVerticalLine(QWidget* parentWidget, int maxHeight = -1);
QWidget* makeHorizontalLine(QWidget* parentWidget, int maxWidth = -1);

void centerWidget(QWidget* widget, QWidget* host = nullptr);

qreal getDpi(const QWidget* widget);

QWindow* getWindow(const QWidget* widget);

void clearFocus(QWidget* widget, bool recursive);

template<class T>
T* findFirstParentOfType(QWidget* child) {
  auto* parent = child;

  while (parent != nullptr) {
    parent = parent->parentWidget();
    if (auto* typedPArent = qobject_cast<T*>(parent)) {
      return typedPArent;
    }
  }

  return nullptr;
}
} // namespace oclero::qlementine
