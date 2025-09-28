/*
  This file is part of KDDockWidgets.

  SPDX-FileCopyrightText: 2024 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Sérgio Martins <sergio.martins@kdab.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

// Yet another widget viewer. To be merged with ObjectViewer.
// Header only.
// Useful for applications not compatible with Gammaray.

#pragma once

#include <QtCore/QDebug>
#include <QtCore/QPointer>
#include <QtGui/QCursor>
#include <QtGui/QPainter>
#include <QtGui/QPalette>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QMainWindow>
#include <qmenu.h>

enum MyRole {
    WidgetRole = Qt::UserRole + 1,
    PixmapRole
};

namespace {

class DebugWidgetViewer : public QWidget
{
public:
    explicit DebugWidgetViewer(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto pickButton = new QPushButton(QStringLiteral("Pick widget"));
        pickButton->setObjectName(QStringLiteral("pick button"));
        auto vlay = new QVBoxLayout(this);
        auto hlay = new QHBoxLayout();

        m_tree.setMinimumWidth(700);
        m_tree.setModel(&m_model);
        m_tree.setContextMenuPolicy(Qt::CustomContextMenu);
        connect(&m_tree, &QTreeView::customContextMenuRequested, this, &DebugWidgetViewer::onCustomContextMenuRequested);

        hlay->addWidget(&m_tree);
        hlay->addWidget(&m_preview);
        vlay->addLayout(hlay);
        vlay->addWidget(pickButton);
        connect(pickButton, &QPushButton::clicked, this,
                &DebugWidgetViewer::pickWidget);

        connect(&m_model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &, const QModelIndex &) {
            renderTree();
        });

        connect(m_tree.selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &DebugWidgetViewer::onSelectionChanged);
    }

    ~DebugWidgetViewer();

    void pickWidget()
    {
        setPickingEnabled(true);
    }

    void loadPickedWidget(QWidget *widget)
    {
        m_model.clear();
        if (!widget)
            return;

        loadWidgetRecursively(widget->window(), m_model.invisibleRootItem());

        renderTree();
    }

    void loadWidgetRecursively(QWidget *w, QStandardItem *parentItem)
    {
        if (!w->isVisible())
            return;

        if (w->size().isEmpty()) {
            qDebug() << "Ignoring widget with empty size" << w << w->size();
            return;
        }

        QString name = QString::fromLatin1(w->metaObject()->className());
        if (!w->objectName().isEmpty()) {
            name += QStringLiteral("[%1]").arg(w->objectName());
        }

        QWidget *parentWidget = w->parentWidget();
        if (parentWidget && !w->isWindow() && !w->geometry().intersects(parentWidget->rect())) {
            qDebug() << "Widget out of bounds! w=" << w << "; geometry=" << w->geometry() << "; parentSize=" << parentWidget->size();
        }

        auto item = new QStandardItem(name);
        item->setData(QVariant::fromValue(w), WidgetRole);
        item->setData(pixmapForWidget(w), PixmapRole);
        item->setCheckable(true);
        item->setCheckState(Qt::Checked);

        const int indexToInsert = parentItem->rowCount();
        parentItem->insertRow(indexToInsert, item);

        for (auto child : w->children()) {
            if (auto childWidget = qobject_cast<QWidget *>(child)) {
                loadWidgetRecursively(childWidget, item);
            }
        }
    }

    /// Event filter for picking
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::MouseButtonPress)
            return false;

        if (auto w = qobject_cast<QWidget *>(watched)) {
            setPickingEnabled(false);
            loadPickedWidget(w);
            return true;
        }

        return false;
    }

    void setPickingEnabled(bool enabled)
    {
        if (enabled) {
            qGuiApp->setOverrideCursor(Qt::CrossCursor);
            qGuiApp->installEventFilter(this);
        } else {
            qGuiApp->removeEventFilter(this);
            qGuiApp->restoreOverrideCursor();
        }
    }

    QWidget *rootWidget() const
    {
        return m_model.item(0, 0)->data(WidgetRole).value<QWidget *>();
    }

    void renderTree()
    {
        QWidget *rw = rootWidget();
        m_preview.setFixedSize(rw->size());
        m_previewPx = QPixmap(m_preview.size());
        renderTree(m_model.invisibleRootItem(), rw);
        m_preview.setPixmap(m_previewPx);
    }

    void renderTree(QStandardItem *node, const QWidget *rootWidget)
    {
        QWidget *widget = node->data(WidgetRole).value<QWidget *>();
        if (widget && node->checkState() != Qt::Checked) {
            return;
        }

        if (widget) {
            const QPoint offset = widget->mapTo(rootWidget, QPoint(0, 0));
            widget->render(&m_previewPx, offset, {}, DrawWindowBackground);
        }

        const int count = node->rowCount();
        for (int i = 0; i < count; ++i) {
            renderTree(node->child(i, 0), rootWidget);
        }
    }

    QPixmap pixmapForWidget(QWidget *w)
    {
        QPixmap px(w->size());
        w->render(&px, {}, {}, DrawWindowBackground);
        return px;
    }

    void onSelectionChanged()
    {
        const QModelIndexList indexes = m_tree.selectionModel()->selectedIndexes();
        if (indexes.size() != 1)
            return;

        auto widget = indexes.first().data(WidgetRole).value<QWidget *>();
        if (!widget)
            return;

        qDebug() << "START PRINT" << widget << "---------------------";
        qDebug() << "  geometry=" << widget->geometry()
                 << "\n  minimumSize=" << widget->minimumSize()
                 << "\n  minimumSizeHint=" << widget->minimumSizeHint()
                 << "\n  maximumSize=" << widget->maximumSize()
                 << "\n  policy=" << widget->sizePolicy()
                 << "\n  visible/enabled=" << widget->isVisible()
                 << widget->isEnabled()
                 << "\n  backgroundRole=" << widget->backgroundRole()
                 << "\n  autoFillBackground=" << widget->autoFillBackground()
                 << "\n  Window=" << widget->palette().color(QPalette::Window)
                 << "\n  Base=" << widget->palette().color(QPalette::Base)
                 << "\n  Button=" << widget->palette().color(QPalette::Button)
                 << "\n  HWND=" << widget->windowHandle();

        if (auto mw = qobject_cast<QMainWindow *>(widget)) {
            qDebug() << "  centralWidget=" << mw->centralWidget();
        }

        qDebug() << "END PRINT-------------------";
    }

    void onCustomContextMenuRequested(QPoint pos)
    {
        const QModelIndex index = m_tree.indexAt(pos);
        if (!index.isValid())
            return;

        QMenu menu;
        QAction *a = menu.addAction(QStringLiteral("render to png"));
        connect(a, &QAction::triggered, this, [&index] {
            auto widget = index.data(WidgetRole).value<QWidget *>();
            Q_ASSERT(widget);
            QPixmap px(widget->size());
            widget->render(&px, {}, {}, DrawWindowBackground);
            px.save(QStringLiteral("widget.png"));
        });
        menu.exec(m_tree.mapToGlobal(pos));
    }

private:
    QLabel m_preview;
    QPixmap m_previewPx;
    QStandardItemModel m_model;
    QTreeView m_tree;
};

DebugWidgetViewer::~DebugWidgetViewer()
{
}

}
