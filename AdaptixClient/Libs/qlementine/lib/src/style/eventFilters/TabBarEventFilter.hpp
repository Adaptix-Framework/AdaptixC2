// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QEvent>
#include <QObject>
#include <QTabBar>
#include <QToolButton>
#include <QMouseEvent>
#include <QPointer>

#include <oclero/qlementine/style/QlementineStyle.hpp>

namespace oclero::qlementine {
class TabBarButtonEventFilter : public QObject {
public:
  explicit TabBarButtonEventFilter(QTabBar* tabBar)
    : QObject(tabBar)
    , _tabBar(tabBar) {}

protected:
  bool eventFilter(QObject*, QEvent* evt) override {
    const auto type = evt->type();
    if (type == QEvent::Leave || type == QEvent::Enter) {
      if (_tabBar) {
        _tabBar->update();
      }
    }

    return false;
  }

private:
  QTabBar* _tabBar{ nullptr };
};

class TabBarEventFilter : public QObject {
public:
  TabBarEventFilter(QTabBar* tabBar)
    : QObject(tabBar)
    , _tabBar(tabBar) {
    // Tweak left/right buttons.
    const auto toolButtons = tabBar->findChildren<QToolButton*>();
    if (toolButtons.size() == 2) {
      auto* buttonEvtFilter = new TabBarButtonEventFilter(_tabBar);

      _leftButton = toolButtons.at(0);
      _leftButton->setFocusPolicy(Qt::NoFocus);
      _leftButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
      _leftButton->setFixedSize(_leftButton->sizeHint());
      QlementineStyle::setAutoIconColor(_leftButton, AutoIconColor::None);
      _leftButton->installEventFilter(buttonEvtFilter);

      _rightButton = toolButtons.at(1);
      _rightButton->setFocusPolicy(Qt::NoFocus);
      _rightButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
      _rightButton->setFixedSize(_rightButton->sizeHint());
      QlementineStyle::setAutoIconColor(_rightButton, AutoIconColor::None);
      _rightButton->installEventFilter(buttonEvtFilter);
    }
  }

  bool eventFilter(QObject*, QEvent* evt) override {
    const auto type = evt->type();

    if (type == QEvent::MouseButtonRelease) {
      const auto* mouseEvent = static_cast<QMouseEvent*>(evt);
      if (mouseEvent->button() == Qt::MouseButton::MiddleButton) {
        // Close tab.
        const auto tabIndex = _tabBar->tabAt(mouseEvent->pos());
        if (tabIndex != -1 && _tabBar->isTabVisible(tabIndex)) {
          evt->accept();
          Q_EMIT _tabBar->tabCloseRequested(tabIndex);
          return true;
        }
      } else if (mouseEvent->button() == Qt::MouseButton::RightButton) {
        // Tab context menu.
        const auto tabIndex = _tabBar->tabAt(mouseEvent->pos());
        if (tabIndex != -1 && _tabBar->isTabVisible(tabIndex)) {
          evt->accept();
          Q_EMIT _tabBar->customContextMenuRequested(mouseEvent->pos());
          return true;
        }
      }
    } else if (type == QEvent::Wheel) {
      const auto* wheelEvent = static_cast<QWheelEvent*>(evt);

      // Block non-horizontal scroll.
      const bool wheelVertical = qAbs(wheelEvent->angleDelta().y()) > qAbs(wheelEvent->angleDelta().x());
      if (wheelVertical) {
        evt->ignore();
        return true;
      }

      auto delta = wheelEvent->pixelDelta().x();

      // If delta is null, it might be because we are on MacOS, using a trackpad.
      // So let's use angleDelta instead.
      if (delta == 0) {
        delta = wheelEvent->angleDelta().x();
      }

      // Invert the value if necessary.
      if (wheelEvent->inverted()) {
        delta = -delta;
      }

      if (delta > 0 && _rightButton) {
        // delta > 0 : scroll to the right.
        _rightButton->click();
        evt->accept();
        return true;
      } else if (delta < 0 && _leftButton) {
        // delta < 0 : scroll to the left.
        _leftButton->click();
        evt->accept();
        return true;
      } else {
        evt->ignore();
        return true;
      }
    } else if (type == QEvent::HoverMove) {
      const auto* mouseEvent = static_cast<QMouseEvent*>(evt);
      const auto beginX = _leftButton->x();
      if (mouseEvent->pos().x() > beginX) {
        _tabBar->update();
      }
    }

    return false;
  }

private:
  QTabBar* _tabBar{ nullptr };
  QPointer<QToolButton> _leftButton{ nullptr };
  QPointer<QToolButton> _rightButton{ nullptr };
};
} // namespace oclero::qlementine
