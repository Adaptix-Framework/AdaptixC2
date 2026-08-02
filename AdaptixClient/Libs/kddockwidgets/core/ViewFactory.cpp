/*
  This file is part of KDDockWidgets.

  SPDX-FileCopyrightText: 2019 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Sérgio Martins <sergio.martins@kdab.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "ViewFactory.h"

using namespace KDDockWidgets;
using namespace KDDockWidgets::Core;

DropIndicatorType ViewFactory::s_dropIndicatorType = DropIndicatorType::Classic;

ViewFactory::~ViewFactory()
{
}

QString ViewFactory::classicIndicatorsPath() const
{
    return QStringLiteral(":/img/classic_indicators");
}

View *ViewFactory::createGroup(Core::Group *controller, View *parent, int userType) const
{
    Q_UNUSED(userType);
    return createGroup(controller, parent);
}

View *ViewFactory::createTitleBar(Core::TitleBar *controller, View *parent, int userType) const
{
    Q_UNUSED(userType);
    return createTitleBar(controller, parent);
}

View *ViewFactory::createTabBar(Core::TabBar *tabBar, View *parent, int userType) const
{
    Q_UNUSED(userType);
    return createTabBar(tabBar, parent);
}

View *ViewFactory::createFloatingWindow(Core::FloatingWindow *controller, Core::MainWindow *parent,
                                        Qt::WindowFlags windowFlags, int userType) const
{
    Q_UNUSED(userType);
    return createFloatingWindow(controller, parent, windowFlags);
}
