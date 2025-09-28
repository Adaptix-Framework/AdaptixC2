/*
  This file is part of KDDockWidgets.

  SPDX-FileCopyrightText: 2019 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Sérgio Martins <sergio.martins@kdab.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "Platform.h"
#include "qtcommon/Window_p.h"
#include "qtcommon/View.h"
#include "core/ViewGuard.h"
#include "core/Utils_p.h"

#include <QString>
#include <QTimer>
#include <QDebug>
#include <QEventLoop>
#include <QWindow>
#include <QGuiApplication>
#include <QElapsedTimer>

#include <QTest>

using namespace KDDockWidgets;
using namespace KDDockWidgets::QtCommon;

namespace KDDockWidgets::Tests {

template<typename Func>
static bool waitFor(Func func, int timeout)
{
    QElapsedTimer timer;
    timer.start();

    while (!func()) {
        if (timer.elapsed() >= timeout)
            return false;

        QTest::qWait(100);
    }

    return true;
}


/// @brief Helper class to help us with tests
class EventFilter : public QObject
{
public:
    EventFilter(QEvent::Type type)
        : m_type(type)
    {
    }
    ~EventFilter() override;
    bool eventFilter(QObject *, QEvent *e) override
    {
        if (e->type() == m_type)
            m_got = true;

        return false;
    }

    const QEvent::Type m_type;
    bool m_got = false;
};

EventFilter::~EventFilter() = default;

}

bool Platform_qt::tests_wait(int ms) const
{
    QTest::qWait(ms);
    return true;
}

bool Platform_qt::tests_waitForWindowActive(Core::Window::Ptr window, int timeout) const
{
    Q_ASSERT(window);
    auto windowqt = static_cast<Window *>(window.get());
    QWindow *qwindow = windowqt->qtWindow();

    return Tests::waitFor([qwindow] {
        return qwindow && qwindow->isActive();
    },
                          timeout);
}

bool Platform_qt::tests_waitForEvent(Core::Object *w, QEvent::Type type, int timeout) const
{
    Tests::EventFilter filter(type);
    w->installEventFilter(&filter);
    QElapsedTimer time;
    time.start();

    while (!filter.m_got && time.elapsed() < timeout) {
        qGuiApp->processEvents();
        QTest::qWait(50);
    }

    return filter.m_got;
}

bool Platform_qt::tests_waitForEvent(Core::View *view, QEvent::Type type, int timeout) const
{
    return tests_waitForEvent(QtCommon::View_qt::asQObject(view), type, timeout);
}

bool Platform_qt::tests_waitForResize(Core::View *view, int timeout) const
{
    return tests_waitForEvent(QtCommon::View_qt::asQObject(view), QEvent::Resize, timeout);
}

bool Platform_qt::tests_waitForResize(Core::Controller *c, int timeout) const
{
    return tests_waitForResize(c->view(), timeout);
}

bool Platform_qt::tests_waitForDeleted(Core::View *view, int timeout) const
{
    QObject *o = view ? QtCommon::View_qt::asQObject(view) : nullptr;
    if (!o)
        return true;

    QPointer<QObject> ptr = o;
    QElapsedTimer time;
    time.start();

    while (ptr && time.elapsed() < timeout) {
        qGuiApp->processEvents();
        QTest::qWait(50);
    }

    const bool wasDeleted = !ptr;
    return wasDeleted;
}

bool Platform_qt::tests_waitForDeleted(Core::Controller *o, int timeout) const
{
    if (!o)
        return true;

    QPointer<QObject> ptr = o;
    QElapsedTimer time;
    time.start();

    while (ptr && time.elapsed() < timeout) {
        qGuiApp->processEvents();
        QTest::qWait(50);
    }

    const bool wasDeleted = !ptr;
    return wasDeleted;
}

void Platform_qt::tests_doubleClickOn(QPoint globalPos, Core::View *receiver)
{
    QCursor::setPos(globalPos);
    tests_pressOn(globalPos, receiver); // double-click involves an initial press

    MouseEvent ev(Event::MouseButtonDblClick, receiver->mapFromGlobal(globalPos),
                  receiver->rootView()->mapFromGlobal(globalPos), globalPos, Qt::LeftButton,
                  Qt::LeftButton, Qt::NoModifier);

    if (Platform::instance()->isQtQuick()) {
        // QtQuick case, we need to send the event to the mouse area
        auto actualReceiver = static_cast<KDDockWidgets::QtCommon::View_qt *>(receiver)->viewProperty("titleBarMouseArea").value<QObject *>();
        qGuiApp->sendEvent(actualReceiver, &ev);
    } else {
        // QtWidgets case
        Platform::instance()->sendEvent(receiver, &ev);
    }
}

void Platform_qt::tests_pressOn(QPoint globalPos, Core::View *receiver)
{
    setCursorPos(globalPos);
    MouseEvent ev(Event::MouseButtonPress, receiver->mapFromGlobal(globalPos),
                  receiver->rootView()->mapFromGlobal(globalPos), globalPos, Qt::LeftButton,
                  Qt::LeftButton, Qt::NoModifier);
    sendEvent(receiver, &ev);
}

void Platform_qt::tests_pressOn(QPoint globalPos, std::shared_ptr<Core::Window> receiver)
{
    Platform::instance()->setCursorPos(globalPos);
    MouseEvent ev(Event::MouseButtonPress, receiver->mapFromGlobal(globalPos),
                  receiver->mapFromGlobal(globalPos), globalPos, Qt::LeftButton, Qt::LeftButton,
                  Qt::NoModifier);
    qGuiApp->sendEvent(static_cast<Window *>(receiver.get())->qtWindow(), &ev);
}

bool Platform_qt::tests_releaseOn(QPoint globalPos, Core::View *receiver)
{
    MouseEvent ev(Event::MouseButtonRelease, receiver->mapFromGlobal(globalPos),
                  receiver->rootView()->mapFromGlobal(globalPos), globalPos, Qt::LeftButton,
                  Qt::LeftButton, Qt::NoModifier);
    Platform::instance()->sendEvent(receiver, &ev);

    return true;
}

void Platform_qt::tests_doubleClickOn(QPoint globalPos, std::shared_ptr<Core::Window> receiver)
{
    Platform::instance()->setCursorPos(globalPos);
    MouseEvent ev(Event::MouseButtonDblClick, receiver->mapFromGlobal(globalPos),
                  receiver->mapFromGlobal(globalPos), globalPos, Qt::LeftButton, Qt::LeftButton,
                  Qt::NoModifier);

    tests_pressOn(globalPos, receiver); // double-click involves an initial press
    qGuiApp->sendEvent(static_cast<Window *>(receiver.get())->qtWindow(), &ev);
}

bool Platform_qt::tests_mouseMove(QPoint globalPos, Core::View *receiver)
{
    Core::ViewGuard receiverP = receiver;
    Platform::instance()->setCursorPos(globalPos);
    MouseEvent ev(Event::MouseMove, receiver->mapFromGlobal(globalPos),
                  receiver->rootView()->mapFromGlobal(globalPos), globalPos, Qt::LeftButton,
                  Qt::LeftButton, Qt::NoModifier);

    if (!receiverP) {
        qWarning() << "Receiver was deleted";
        return false;
    }
    Platform::instance()->sendEvent(receiver, &ev);
    Platform::instance()->tests_wait(2);

    return true;
}
