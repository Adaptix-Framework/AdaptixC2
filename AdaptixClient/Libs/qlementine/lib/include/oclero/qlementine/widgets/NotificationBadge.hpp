// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>
#include <QPointer>
#include <QMargins>

namespace oclero::qlementine {
/// A small badge to display a notification (with or without text) on another widget.
class NotificationBadge : public QWidget {
  Q_OBJECT

  Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
  Q_PROPERTY(QColor foregroundColor READ foregroundColor WRITE setForegroundColor NOTIFY foregroundColorChanged)
  Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
  Q_PROPERTY(QPoint relativePosition READ relativePosition WRITE setRelativePosition NOTIFY relativePositionChanged)

public:
  explicit NotificationBadge(QWidget* parent = nullptr);

  // Will track changes to widget and resize itself automatically.
  // If the monitored widget's parent changes, will follow the widget and place
  // itself around the widget automatically.
  // If the monitored widget is deleted, will set it to zero.
  void setWidget(QWidget* widget);
  QWidget* widget() const;

  const QString& text() const;
  Q_SLOT void setText(const QString&);
  Q_SIGNAL void textChanged();

  const QColor& foregroundColor() const;
  Q_SLOT void setForegroundColor(const QColor&);
  Q_SIGNAL void foregroundColorChanged();

  const QColor& backgroundColor() const;
  Q_SLOT void setBackgroundColor(const QColor&);
  Q_SIGNAL void backgroundColorChanged();

  const QPoint& relativePosition() const;
  Q_SLOT void setRelativePosition(const QPoint&);
  void setRelativePosition(int x, int y);
  Q_SIGNAL void relativePositionChanged();

  const QMargins& padding() const;
  Q_SLOT void setPadding(const QMargins&);
  Q_SIGNAL void paddingChanged();

  QSize minimumSizeHint() const override;
  QSize sizeHint() const override;

protected:
  void paintEvent(QPaintEvent* evt) override;
  bool eventFilter(QObject* obj, QEvent* evt) override;
  bool event(QEvent* evt) override;

private:
  void updatePosition();

private:
  QString _text;
  QColor _foregroundColor{ Qt::white };
  QColor _backgroundColor{ Qt::red };
  QMargins _padding{ 4, 2, 4, 2 };
  QPointer<QWidget> _widget{ nullptr };
  QPointer<QWidget> _widgetParent{ nullptr };
  QPoint _relativePos{ 4, -4 };
};
} // namespace oclero::qlementine
