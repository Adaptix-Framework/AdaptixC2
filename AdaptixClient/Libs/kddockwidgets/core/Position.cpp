/*
  This file is part of KDDockWidgets.

  SPDX-FileCopyrightText: 2020 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Sérgio Martins <sergio.martins@kdab.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

/**
 * @file Helper class so dockwidgets can be restored to their previous position.
 *
 * @author Sérgio Martins \<sergio.martins@kdab.com\>
 */

#include "Position_p.h"
#include "LayoutSaver_p.h"
#include "Logging_p.h"
#include "Group.h"
#include "DockWidget_p.h"
#include "ScopedValueRollback_p.h"
#include "core/layouting/Item_p.h"
#include "core/layouting/LayoutingHost_p.h"
#include "kddockwidgets/core/FloatingWindow.h"
#include "kddockwidgets/core/Layout.h"
#include "kddockwidgets/core/MainWindow.h"
#include "kddockwidgets/core/DockRegistry.h"

#include <algorithm>
#include <utility>

using namespace KDDockWidgets;

Positions::~Positions()
{
    m_placeholders.clear();
}

void Positions::addPlaceholderItem(Core::Item *placeholder)
{
    assert(placeholder);

    // 1. Already exists, nothing to do
    if (containsPlaceholder(placeholder))
        return;

    if (DockRegistry::self()->itemIsInMainWindow(placeholder)) {
        // 2. We only support 1 main window placeholder for now
        removeMainWindowPlaceholders();
    }

    // Make sure our list only contains valid placeholders. We save the result so we can disconnect
    // from the lambda, since the Item might outlive Position
    auto conn = placeholder->deleted.connect([this, placeholder] { removePlaceholder(placeholder); });

    m_placeholders.push_back(std::make_unique<ItemRef>(conn, placeholder));

    // NOTE: We use a list instead of simply two variables to keep the placeholders, because
    // a placeholder from a FloatingWindow might become a MainWindow one without we knowing,
    // like when dragging a floating window into a MainWindow. So, isInMainWindow() won't return
    // the same value always, hence we just shove them into a list, instead of giving them
    // meaningful names in separated variables
}

bool Positions::itemIsBeingDestroyed(Core::Item *item) const
{
    if (item->m_inDtor)
        return true;

    Core::Layout *layout = DockRegistry::self()->layoutForItem(item);
    if (layout) {
        /// FloatingWindow get destroyed with delete later
        /// its layout items aren't suitable to dock back in as they will be destroyed
        if (Core::FloatingWindow *fw = layout->floatingWindow()) {
            if (fw->beingDeleted())
                return true;
        }
    }

    return false;
}

Core::Item *Positions::lastItem(Core::Item *current) const
{
    for (auto it = m_placeholders.rbegin(), e = m_placeholders.rend(); it != e; ++it) {
        auto item = (*it)->item;

        if (item == current || item == Core::Group::s_inFloatHack || itemIsBeingDestroyed(item)) {
            continue;
        }

        return item;
    }

    return nullptr;
}


Core::Item *Positions::lastItem(const Core::DockWidget *current) const
{
    Core::Item *currentItem = current ? current->d->item() : nullptr;
    return lastItem(currentItem);
}

bool Positions::containsPlaceholder(Core::Item *item) const
{
    return std::any_of(m_placeholders.cbegin(), m_placeholders.cend(), [item](const auto &itemRef) {
        return itemRef->item == item;
    });
}

void Positions::removePlaceholders()
{
    ScopedValueRollback clearGuard(m_clearing, true);
    m_placeholders.clear();
}

void Positions::removePlaceholders(const Core::LayoutingHost *host)
{
    m_placeholders.erase(std::remove_if(m_placeholders.begin(), m_placeholders.end(),
                                        [host](const std::unique_ptr<ItemRef> &itemref) {
                                            if (!itemref->item)
                                                return true;
                                            return host == itemref->item->host();
                                        }),
                         m_placeholders.end());
}

void Positions::removeNonMainWindowPlaceholders()
{
    auto it = m_placeholders.begin();
    while (it != m_placeholders.end()) {
        ItemRef *itemref = it->get();
        if (!itemref->isInMainWindow())
            it = m_placeholders.erase(it);
        else
            ++it;
    }
}

void Positions::removeMainWindowPlaceholders()
{
    auto it = m_placeholders.begin();
    while (it != m_placeholders.end()) {
        ItemRef *itemref = it->get();
        if (itemref->isInMainWindow())
            it = m_placeholders.erase(it);
        else
            ++it;
    }
}

void Positions::removePlaceholder(Core::Item *placeholder)
{
    if (m_clearing) // reentrancy guard
        return;

    m_placeholders.erase(std::remove_if(m_placeholders.begin(), m_placeholders.end(),
                                        [placeholder](const std::unique_ptr<ItemRef> &itemref) {
                                            return itemref->item == placeholder || !itemref->item;
                                        }),
                         m_placeholders.end());
}

int Positions::placeholderCount() const
{
    return int(m_placeholders.size());
}

void Positions::deserialize(const LayoutSaver::Position &lp)
{
    m_lastFloatingGeometry = lp.lastFloatingGeometry;
    m_lastOverlayedGeometries = lp.lastOverlayedGeometries;

    for (const auto &placeholder : std::as_const(lp.placeholders)) {
        Core::Layout *layout = nullptr;
        int itemIndex = placeholder.itemIndex;
        if (placeholder.isFloatingWindow) {
            const int index = placeholder.indexOfFloatingWindow;
            if (index == -1) {
                continue; // Skip
            } else {
                assert(LayoutSaver::Layout::s_currentLayoutBeingRestored);
                if (!LayoutSaver::Layout::s_currentLayoutBeingRestored) {
                    /// Doesn't happen, but let's guard it
                    KDDW_WARN("Positions::deserialize: Current layout is null, report a bug!");
                    continue;
                }
                auto serializedFw =
                    LayoutSaver::Layout::s_currentLayoutBeingRestored->floatingWindowForIndex(
                        index);
                if (serializedFw.isValid()) {
                    if (auto fw = serializedFw.floatingWindowInstance) {
                        layout = fw->layout();
                    } else {
                        continue;
                    }
                } else {
                    KDDW_ERROR("Invalid floating window position to restore. index={}", index);
                    continue;
                }
            }
        } else {
            Core::MainWindow *mainWindow =
                DockRegistry::self()->mainWindowByName(placeholder.mainWindowUniqueName);
            layout = mainWindow->layout();
        }

        const Core::Item::List &items = layout->items();
        if (itemIndex >= 0 && itemIndex < items.size()) {
            Core::Item *item = items.at(itemIndex);
            addPlaceholderItem(item);
        } else {
            // Shouldn't happen, maybe even assert
            KDDW_ERROR("Couldn't find item index {}", itemIndex);
        }
    }

    m_tabIndex = lp.tabIndex;
    m_wasFloating = lp.wasFloating;
}

LayoutSaver::Position Positions::serialize() const
{
    LayoutSaver::Position l;

    for (auto &itemRef : m_placeholders) {
        LayoutSaver::Placeholder p;

        Core::Item *item = itemRef->item;
        Core::Layout *layout = DockRegistry::self()->layoutForItem(item);
        const auto itemIndex = layout->items().indexOf(item);

        auto fw = layout->floatingWindow();
        auto mainWindow = layout->mainWindow(/*honourNesting=*/true);
        assert(mainWindow || fw);
        p.isFloatingWindow = fw;

        if (p.isFloatingWindow) {
            p.indexOfFloatingWindow = fw->beingDeleted()
                ? -1
                : DockRegistry::self()->floatingWindows().indexOf(
                      fw);
        } else {
            p.mainWindowUniqueName = mainWindow->uniqueName();
            assert(!p.mainWindowUniqueName.isEmpty());
        }

        p.itemIndex = itemIndex;
        l.placeholders.push_back(p);
    }

    l.tabIndex = m_tabIndex;
    l.wasFloating = m_wasFloating;

    l.lastFloatingGeometry = lastFloatingGeometry();
    l.lastOverlayedGeometries = m_lastOverlayedGeometries;

    return l;
}

Positions::ItemRef::ItemRef(KDBindings::ConnectionHandle conn, Core::Item *it)
    : item(it)
    , connection(std::move(conn))
{
    item->ref();
}

Positions::ItemRef::~ItemRef()
{
    if (item && !item->m_inDtor) {
        connection.disconnect();
        item->unref();
    }
}

bool Positions::ItemRef::isInMainWindow() const
{
    return item && DockRegistry::self()->itemIsInMainWindow(item);
}
