// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QPointer>

#include <vector>

#include <oclero/qlementine/style/Theme.hpp>
#include <oclero/qlementine/style/QlementineStyle.hpp>

namespace oclero::qlementine {
class ThemeManager : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString currentTheme READ currentTheme WRITE setCurrentTheme NOTIFY currentThemeChanged)
  Q_PROPERTY(int themeCount READ themeCount NOTIFY themeCountChanged)

public:
  explicit ThemeManager(QObject* parent = nullptr);
  explicit ThemeManager(QlementineStyle* style, QObject* parent = nullptr);
  ~ThemeManager() override = default;

  QlementineStyle* style() const;
  void setStyle(QlementineStyle* style);

  const std::vector<Theme>& themes() const;
  void addTheme(const Theme& theme);

  void loadDirectory(const QString& path);

  QString currentTheme() const;
  void setCurrentTheme(const QString& key);
  Q_SIGNAL void currentThemeChanged();

  int themeCount() const;
  Q_SIGNAL void themeCountChanged();

  Q_SLOT void setNextTheme();
  Q_SLOT void setPreviousTheme();

  int themeIndex(const QString& key) const;

  int currentThemeIndex() const;
  void setCurrentThemeIndex(int index);

private:
  void synchronizeThemeOnStyle();

private:
  std::vector<Theme> _themes;
  QPointer<QlementineStyle> _style{ nullptr };
  int _currentIndex{ -1 };
};
} // namespace oclero::qlementine
