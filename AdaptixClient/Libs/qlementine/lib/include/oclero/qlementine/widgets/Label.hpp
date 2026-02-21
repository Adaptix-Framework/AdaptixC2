// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <oclero/qlementine/Common.hpp>

#include <QLabel>

namespace oclero::qlementine {
/// A QLabel that handles automatic styling for different text roles (titles, normal text, ect.)
/// like h1,h2,p in HTML.
class Label : public QLabel {
  Q_OBJECT

  Q_PROPERTY(oclero::qlementine::TextRole role READ role WRITE setRole NOTIFY roleChanged)

public:
  explicit Label(QWidget* parent = nullptr);
  explicit Label(const QString& text, QWidget* parent = nullptr);
  explicit Label(const QString& text, TextRole role = TextRole::Default, QWidget* parent = nullptr);
  ~Label() override;

  TextRole role() const;
  Q_SLOT void setRole(TextRole role);
  Q_SIGNAL void roleChanged();

protected:
  bool event(QEvent* e) override;
  bool eventFilter(QObject* o, QEvent* e) override;

private:
  void updatePaletteFromTheme();

  TextRole _role{ TextRole::Default };
  bool _changingPaletteFlag{ false };
};
} // namespace oclero::qlementine
