// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once


#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Delegates.hpp>

#include <QEvent>
#include <QObject>
#include <QComboBox>
#include <QTreeView>
#include <QAbstractItemView>
#include <QChildEvent>
#include <QScreen>

namespace oclero::qlementine {
// Event filter for the item view in the QComboBox's popup.
class ComboboxItemViewFilter : public QObject {
public:
  ComboboxItemViewFilter(QComboBox* comboBox, QAbstractItemView* view, QlementineStyle* style)
    : QObject(view)
    , _comboBox(comboBox)
    , _view(view)
    , _qlementineStyle(style)
    , _initialMaxHeight(view->maximumHeight()) {
    _view->installEventFilter(this);

    if (auto* viewport = _view->viewport()) {
      viewport->installEventFilter(this);
    }

    auto* comboBoxPopup = _view->parentWidget();
    comboBoxPopup->installEventFilter(this);
    _comboBox->installEventFilter(this);

    if (const auto* treeView = qobject_cast<QTreeView*>(_view)) {
      QObject::connect(treeView, &QTreeView::expanded, this, &ComboboxItemViewFilter::fixViewGeometry);
    }
  }

protected:
  bool eventFilter(QObject* watchedObject, QEvent* evt) override {
    switch (evt->type()) {
      case QEvent::Type::ChildAdded: {
        if (watchedObject == _comboBox) {
          const auto* childEvent = static_cast<QChildEvent*>(evt);
          const auto* child = childEvent->child();
          if (child == _comboBox->view()) {
            if (_qlementineStyle) {
              _comboBox->setItemDelegate(new ComboBoxDelegate(_comboBox, *_qlementineStyle));
            }
          }
        }
      } break;
      case QEvent::Type::Show:
        fixViewGeometry();
        break;
      case QEvent::Type::Resize:
        if (watchedObject == _comboBox) {
          fixViewGeometry();
        }
        break;
      // case QEvent::Type::MouseButtonPress:
      //   if (watchedObject == _view->viewport()) {
      //     // _clickPoint = _view->itemat
      //     _view->indexAt()
      //     return false;
      //   }
      //   break;
      // case QEvent::Type::MouseButtonRelease:
      //   if (watchedObject == _view->viewport()) {
      //     return true;
      //   }
      //   break;
      default:
        break;
    }
    return false;
  }

private:
  void fixViewGeometry() const {
    if (_comboBox && _qlementineStyle) {
      if (auto* view = _comboBox->view()) {
        {
          const auto isTreeView = view->inherits("QTreeView");
          const auto hMargin = _qlementineStyle->pixelMetric(QStyle::PM_MenuHMargin);
          const auto shadowWidth = _qlementineStyle->theme().spacing;
          const auto borderWidth = _qlementineStyle->theme().borderWidth;

          // Width.
          const auto absoluteMinWidth = _qlementineStyle->theme().controlHeightLarge * (isTreeView ? 2 : 1);
          const auto absoluteMaxWidth = _qlementineStyle->theme().controlHeightLarge * 24;
          const auto width = std::min(absoluteMaxWidth, std::max({
                                                          _comboBox->width(),
                                                          view->sizeHintForColumn(0),
                                                          absoluteMinWidth,
                                                        }))
                             + shadowWidth * 2 + hMargin * 2 + borderWidth * 2;

          // Height - always show full content, only limit by screen size.
          const auto contentHeight = viewMinimumSizeHint().height();
          const auto screen = view->screen();
          const auto viewGlobalY = view->mapToGlobal(QPoint(0, 0)).y();
          const auto absoluteMaxHeight = screen != nullptr ?
                                             screen->geometry().height() - 64 - viewGlobalY :
                                             _qlementineStyle->theme().controlHeightLarge * 20;
          // Add padding for borders and shadow
          const auto totalHeight = contentHeight + shadowWidth * 2 + borderWidth * 2;
          const auto height = std::min(absoluteMaxHeight, totalHeight);

          view->setFixedWidth(width);
          view->setFixedHeight(height);
          view->parentWidget()->adjustSize();
        }
      }
    }
  }

  QSize viewMinimumSizeHint() const {
    auto height = 0;
    const auto rowCount = _view->model() ? _view->model()->rowCount() : 0;
    
    if (const auto* treeView = qobject_cast<QTreeView*>(_view)) {
      // For a QTreeView, look at expanded rows.
      auto currentIndex = treeView->indexAt(QPoint(0, 0));
      while (currentIndex.isValid()) {
        const auto rowSizeHint = _view->sizeHintForIndex(currentIndex).height();
        height += std::max(0, rowSizeHint);
        currentIndex = treeView->indexBelow(currentIndex);
      }
    } else {
      // QListView::minimumSizeHint() doesn't give the correct minimumHeight,
      // so we have to compute it. Always calculate full content height.
      for (auto i = 0; i < rowCount; ++i) {
        const auto rowSizeHint = _view->sizeHintForRow(i);
        height += rowSizeHint;
      }
    }
    // It looks like it is OK for the width, though.
    const auto width = _view->sizeHintForColumn(0);
    return { width, height };
  }

  QComboBox* _comboBox{ nullptr };
  QAbstractItemView* _view{ nullptr };
  QlementineStyle* _qlementineStyle{ nullptr };
  int _initialMaxHeight{ 0 };
  QModelIndex _clickedIndex{};
};

class ComboboxFilter : public QObject {
public:
  explicit ComboboxFilter(QComboBox* comboBox, QlementineStyle* style)
    : QObject(comboBox)
    , _comboBox(comboBox)
    , _qlementineStyle(style) {
    if (const auto* view = comboBox->view()) {
      if (auto* popup = view->parentWidget()) {
        popup->installEventFilter(this);
      }
    }
  }

  bool eventFilter(QObject* watchedObject, QEvent* evt) override {
    switch (evt->type()) {
      // This is the only way we found to know when the QComboBox's view has changed.
      case QEvent::ChildAdded: {
        const auto* childEvent = static_cast<QChildEvent*>(evt);
        const auto* child = childEvent->child();
        if (child == _comboBox->view()) {
          if (_qlementineStyle) {
            _comboBox->setItemDelegate(new ComboBoxDelegate(_comboBox, *_qlementineStyle));
            if (auto* newView = _comboBox->view()) {
              new ComboboxItemViewFilter(_comboBox, newView, _qlementineStyle);
            }
          }
        }
      } break;
      default:
        break;
    }

    return QObject::eventFilter(watchedObject, evt);
  }

private:
  QComboBox* _comboBox{ nullptr };
  QlementineStyle* _qlementineStyle{ nullptr };
};
} // namespace oclero::qlementine
