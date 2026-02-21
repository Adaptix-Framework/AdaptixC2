// SPDX-FileCopyrightText: Olivier Cléro <oclero@hotmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <QVariant>
#include <QColor>
#include <optional>

namespace oclero::qlementine {

/// Computes the contrast ratio between the two colors, according to w3.org spec.
double getContrastRatio(const QColor& c1, const QColor& c2);

/// Gets the color with the new alpha channel (from 0.0 to 1.0).
QColor colorWithAlphaF(QColor const& color, qreal alpha);

/// Gets the color the new alpha chanel (from 0 to 255).
QColor colorWithAlpha(QColor const& color, int alpha);

/// Gets the resulting color by applying the foreground color over the background color with 'SourceOver' composition mode.
QColor getColorSourceOver(const QColor& bg, const QColor& fg);

/// Attempts to parse the QVariant array to get a QColor from the array's values.
/// The order is R, G, B, A.
std::optional<QColor> tryGetColorFromVariantList(QVariantList const& variantList);

///  Attempts to parse the QString to get a QColor from the string's values.
/// The string must follow this rule: "R,G,B,A" (spaces are ignored).
std::optional<QColor> tryGetColorFromRGBAString(QString const& str);

/// Attempts to parse the QString to get a QColor from the string's values.
/// The string must follow this rule: "#RRGGBBAA" (AA is facultative, spaces are ignored).
std::optional<QColor> tryGetColorFromHexaString(QString const& str);

/// Attempts to parse the QVariant to get a QColor.
std::optional<QColor> tryGetColorFromVariant(QVariant const& variant);

/// Gives the color's hexadecimal RGB string.
QString toHexRGB(const QColor& color);

/// Gives the color's hexadecimal RGBA string.
QString toHexRGBA(const QColor& color);
} // namespace oclero::qlementine
