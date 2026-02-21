// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>
#include <QPointer>
#include <QVariantAnimation>

namespace oclero::qlementine {
/// A QWidget that allows to expand vertically or horizontally,
/// revealing or hiding its content with an animation.
class Expander : public QWidget {
  Q_OBJECT

  Q_PROPERTY(bool expanded READ expanded WRITE setExpanded NOTIFY expandedChanged)
  Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)

public:
  explicit Expander(QWidget* parent = nullptr);

  bool expanded() const;
  Q_SLOT void setExpanded(bool expanded);
  Q_SIGNAL void expandedChanged();
  void toggleExpanded();

  Q_SIGNAL void aboutToExpand();
  Q_SIGNAL void aboutToShrink();
  Q_SIGNAL void didExpand();
  Q_SIGNAL void didShrink();

  Qt::Orientation orientation() const;
  Q_SLOT void setOrientation(Qt::Orientation orientation);
  Q_SIGNAL void orientationChanged();

  QWidget* content() const;
  void setContent(QWidget* content);
  Q_SIGNAL void contentChanged();

  QSize sizeHint() const override;

protected:
  bool event(QEvent* e) override;
  void resizeEvent(QResizeEvent* e) override;
  bool eventFilter(QObject* o, QEvent* e) override;

private:
  void updateContentGeometry();

private:
  bool _expanded{ false };
  Qt::Orientation _orientation{ Qt::Orientation::Vertical };
  QVariantAnimation _animation;
  QPointer<QWidget> _content{ nullptr };
};
} // namespace oclero::qlementine
