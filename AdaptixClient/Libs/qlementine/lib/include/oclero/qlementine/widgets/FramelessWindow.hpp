// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>

class QMenuBar;

namespace oclero::qlementine {
class FramelessWindow : public QWidget {
  Q_OBJECT

public:
  explicit FramelessWindow(QWidget* parent = nullptr);
  virtual ~FramelessWindow();

  QMenuBar* menuBar() const;

  QWidget* contentWidget() const;
  void setContentWidget(QWidget* content);

protected:
  void paintEvent(QPaintEvent* event) override;
  bool event(QEvent* event) override;

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};
} // namespace oclero::qlementine
