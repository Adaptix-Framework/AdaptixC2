// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <oclero/qlementine/Common.hpp>

#include <QPlainTextEdit>

namespace oclero::qlementine {
/// An improved QPlainTextEdit.
class PlainTextEdit : public QPlainTextEdit {
  Q_OBJECT

  Q_PROPERTY(Status status READ status WRITE setStatus NOTIFY statusChanged)

public:
  explicit PlainTextEdit(QWidget* parent = nullptr);

  QSize minimumSizeHint() const override;
  QSize sizeHint() const override;

  void setUseMonoSpaceFont(bool useMonoSpaceFont);
  bool useMonoSpaceFont() const;

  Status status() const;
  Q_SLOT void setStatus(Status status);
  Q_SIGNAL void statusChanged();

private:
  void updateFont();

  bool _useMonospaceFont{ false };
  Status _status{ Status::Default };
};
} // namespace oclero::qlementine
