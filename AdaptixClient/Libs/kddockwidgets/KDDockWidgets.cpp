/*
  This file is part of KDDockWidgets.

  SPDX-FileCopyrightText: 2019 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Sérgio Martins <sergio.martins@kdab.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "Config.h"
#include "kddockwidgets/KDDockWidgets.h"
#include "core/layouting/Item_p.h"

#ifdef KDDW_FRONTEND_QTWIDGETS
#include "qtwidgets/Platform.h"
#endif

using namespace KDDockWidgets;

#ifdef KDDW_FRONTEND_QT

void KDDockWidgets::initFrontend(FrontendType type)
{
    if (Core::Platform::instance())
        return;

    switch (type) {
    case FrontendType::QtWidgets:
#ifdef KDDW_FRONTEND_QTWIDGETS
        new QtWidgets::Platform();
#endif
        break;
    case FrontendType::QtQuick:
        break;
    case FrontendType::Flutter:
        break;
    }
}

#endif

const char *KDDockWidgets::spdlogLoggerName()
{
    return "com.kdab.dockwidgets";
}

NeighbourSqueezeStrategy InitialOption::s_defaultNeighbourSqueezeStrategy = NeighbourSqueezeStrategy::AllNeighbours;

InitialOption::InitialOption()
{
}

InitialOption::InitialOption(InitialVisibilityOption v)
    : visibility(v)
{
}

InitialOption::InitialOption(Size size)
    : preferredSize(size)
{
}

InitialOption::InitialOption(InitialVisibilityOption v, Size size)
    : visibility(v)
    , preferredSize(size)
{
}

InitialOption::InitialOption(DefaultSizeMode mode)
    : sizeMode(mode)
{
}
