/*
  This file is part of KDDockWidgets.

  SPDX-FileCopyrightText: 2019 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Waqar Ahmed <waqar.ahmed@kdab.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include "Logging_p.h"
#include "QtCompat_p.h"

#include <nlohmann/json.hpp>
#if defined(KDDW_FRONTEND_QT) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QVariant>
#include <QVariantMap>
#endif

namespace KDDockWidgets {

inline void to_json(nlohmann::json &j, Size size)
{
    j["width"] = size.width();
    j["height"] = size.height();
}

inline void from_json(const nlohmann::json &j, Size &size)
{
    Size s;
    size.setWidth(j.value("width", s.width()));
    size.setHeight(j.value("height", s.height()));
}

inline void to_json(nlohmann::json &j, Rect rect)
{
    j["x"] = rect.x();
    j["y"] = rect.y();
    j["width"] = rect.width();
    j["height"] = rect.height();
}

inline void from_json(const nlohmann::json &j, Rect &rect)
{
    Rect r;
    rect.setX(j.value("x", r.x()));
    rect.setY(j.value("y", r.y()));
    rect.setWidth(j.value("width", r.width()));
    rect.setHeight(j.value("height", r.height()));
}

}

QT_BEGIN_NAMESPACE

inline void from_json(const nlohmann::json &j, QString &string)
{
    string = QString::fromStdString(j.get<std::string>());
}

inline void to_json(nlohmann::json &j, const QString &s)
{
    j = s.toStdString();
}

inline void from_json(const nlohmann::json &j, KDDockWidgets::Vector<QString> &stringList)
{
    if (!j.is_null() && !j.is_array()) {
        KDDW_ERROR("This is not an array, fix the code");
        stringList.clear();
        return;
    }
    stringList.reserve(( int )j.size());
    for (const auto &v : j) {
        stringList.push_back(v.get<QString>());
    }
}

inline void to_json(nlohmann::json &j, const KDDockWidgets::Vector<QString> &stringList)
{
    for (const auto &s : stringList) {
        j.push_back(s);
    }
}
#ifdef KDDW_QTGUI_TYPES

inline void to_json(nlohmann::json &j, QSize size)
{
    KDDockWidgets::to_json(j, size);
}

inline void from_json(const nlohmann::json &j, QSize &size)
{
    KDDockWidgets::from_json(j, size);
}

inline void to_json(nlohmann::json &j, QRect rect)
{
    KDDockWidgets::to_json(j, rect);
}

inline void from_json(const nlohmann::json &j, QRect &rect)
{
    KDDockWidgets::from_json(j, rect);
}

#endif

#if defined(KDDW_FRONTEND_QT) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
inline void to_json(nlohmann::json &j, const QVariant &v)
{
    if (v.isNull()) {
        j = nullptr;
    } else if (v.typeId() == QMetaType::QString) {
        j = v.toString().toStdString();
    } else if (v.typeId() == QMetaType::Bool) {
        j = v.toBool();
    } else if (v.typeId() == QMetaType::Int) {
        j = v.toInt();
    } else if (v.typeId() == QMetaType::Double) {
        j = v.toDouble();
    } else if (v.typeId() == QMetaType::QStringList) {
        QStringList list = v.toStringList();
        j = nlohmann::json::array();
        for (const auto &s : list)
            j.push_back(s.toStdString());
    } else if (v.typeId() == QMetaType::QVariantMap) {
        j = v.toMap();
    } else if (v.typeId() == QMetaType::QVariantList) {
        QVariantList list = v.toList();
        j = nlohmann::json::array();
        for (const auto &item : list)
            j.push_back(item);
    } else {
        // fallback for unknown types
        j = v.toString().toStdString();
    }
}

inline void from_json(const nlohmann::json &j, QVariant &v)
{
    if (j.is_null()) {
        v = QVariant();
    } else if (j.is_string()) {
        v = QString::fromStdString(j.get<std::string>());
    } else if (j.is_boolean()) {
        v = j.get<bool>();
    } else if (j.is_number_integer()) {
        v = j.get<int>();
    } else if (j.is_number_float()) {
        v = j.get<double>();
    } else if (j.is_array()) {
        QVariantList list;
        list.reserve(j.size());
        for (const auto &item : j) {
            QVariant value;
            from_json(item, value);
            list.append(value);
        }
        v = list;
    } else if (j.is_object()) {
        QVariantMap map;
        for (auto it = j.begin(); it != j.end(); ++it) {
            QVariant value;
            from_json(it.value(), value);
            map.insert(QString::fromStdString(it.key()), value);
        }
        v = map;
    } else {
        v = QVariant();
    }
}

inline void to_json(nlohmann::json &j, const QVariantMap &map)
{
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        j[it.key().toStdString()] = it.value();
    }
}

inline void from_json(const nlohmann::json &j, QVariantMap &map)
{
    map.clear();
    if (j.is_object()) {
        for (auto it = j.begin(); it != j.end(); ++it) {
            QVariant value;
            from_json(it.value(), value);
            map.insert(QString::fromStdString(it.key()), value);
        }
    }
}

#endif

QT_END_NAMESPACE
