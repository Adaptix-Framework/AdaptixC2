// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QAbstractScrollArea>
#include <QFocusFrame>
#include <QTimer>
#include <QEvent>

namespace oclero::qlementine {
class WidgetWithFocusFrameEventFilter : public QObject {
  Q_OBJECT
public:
  explicit WidgetWithFocusFrameEventFilter(QWidget* widget)
    : QObject(widget)
    , _widget(widget) {
    _focusFrame = new QFocusFrame(_widget);
  }

  bool eventFilter(QObject* watchedObject, QEvent* evt) override {
    if (!_added && watchedObject == _widget) {
      const auto type = evt->type();
      // Create the focus frame as late as possible to give
      // more chances to any parent (e.g. scrollarea) to already exist.
      // QEvent::Show isn't sufficient. We need to delay even more, so
      // waiting for the first QEvent::Paint is our only solution.
      if (type == QEvent::Paint) {
        QTimer::singleShot(0, this, [this]() {
          if (!_added) {
            _added = true;
            _focusFrame->setWidget(_widget);
          }
        });
      }
    }

    return QObject::eventFilter(watchedObject, evt);
  }

private:
  QWidget* _widget{ nullptr };
  QFocusFrame* _focusFrame{ nullptr };
  bool _added{ false };
};
} // namespace oclero::qlementine
