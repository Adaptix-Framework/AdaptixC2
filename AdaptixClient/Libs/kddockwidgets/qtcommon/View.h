/*
  This file is part of KDDockWidgets.

  SPDX-FileCopyrightText: 2020 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Sérgio Martins <sergio.martins@kdab.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#pragma once

#include "kddockwidgets/core/View.h"

QT_BEGIN_NAMESPACE
class QWidget;
class QQuickItem;
QT_END_NAMESPACE

namespace KDDockWidgets::Core {
class Controller;
}

namespace KDDockWidgets::QtCommon {

class DOCKS_EXPORT View_qt : public Core::View
{
public:
    explicit View_qt(Core::Controller *controller, Core::ViewType type, QObject *thisObj);
    ~View_qt() override;

    QObject *thisObject() const;

    Core::HANDLE handle() const override;
    void setViewName(const QString &name) override;

    static QObject *asQObject(View *);

#ifdef KDDW_FRONTEND_QTWIDGETS
    static QWidget *asQWidget(Core::View *);
    static QWidget *asQWidget(Core::Controller *);
#endif

    virtual QVariant viewProperty(const char *name) const
    {
        return m_thisObj->property(name);
    }

protected:
    class EventFilter;
    EventFilter *const m_eventFilter;
    QObject *const m_thisObj;
    Q_DISABLE_COPY(View_qt)
};

}
