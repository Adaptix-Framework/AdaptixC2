/****************************************************************************
** Meta object code from reading C++ file 'TabBar.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtwidgets/views/TabBar.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'TabBar.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN13KDDockWidgets9QtWidgets6TabBarE_t {};
} // unnamed namespace

template <> constexpr inline auto KDDockWidgets::QtWidgets::TabBar::qt_create_metaobjectdata<qt_meta_tag_ZN13KDDockWidgets9QtWidgets6TabBarE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KDDockWidgets::QtWidgets::TabBar",
        "dockWidgetInserted",
        "",
        "index",
        "dockWidgetRemoved",
        "countChanged",
        "currentDockWidgetChanged",
        "KDDockWidgets::Core::DockWidget*"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'dockWidgetInserted'
        QtMocHelpers::SignalData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Signal 'dockWidgetRemoved'
        QtMocHelpers::SignalData<void(int)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Signal 'countChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'currentDockWidgetChanged'
        QtMocHelpers::SignalData<void(KDDockWidgets::Core::DockWidget *)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 7, 2 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<TabBar, qt_meta_tag_ZN13KDDockWidgets9QtWidgets6TabBarE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KDDockWidgets::QtWidgets::TabBar::staticMetaObject = { {
    QMetaObject::SuperData::link<View<QTabBar>::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets6TabBarE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets6TabBarE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets6TabBarE_t>.metaTypes,
    nullptr
} };

void KDDockWidgets::QtWidgets::TabBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<TabBar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->dockWidgetInserted((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->dockWidgetRemoved((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->countChanged(); break;
        case 3: _t->currentDockWidgetChanged((*reinterpret_cast< std::add_pointer_t<KDDockWidgets::Core::DockWidget*>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (TabBar::*)(int )>(_a, &TabBar::dockWidgetInserted, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (TabBar::*)(int )>(_a, &TabBar::dockWidgetRemoved, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (TabBar::*)()>(_a, &TabBar::countChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (TabBar::*)(KDDockWidgets::Core::DockWidget * )>(_a, &TabBar::currentDockWidgetChanged, 3))
            return;
    }
}

const QMetaObject *KDDockWidgets::QtWidgets::TabBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KDDockWidgets::QtWidgets::TabBar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets6TabBarE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "Core::TabBarViewInterface"))
        return static_cast< Core::TabBarViewInterface*>(this);
    return View<QTabBar>::qt_metacast(_clname);
}

int KDDockWidgets::QtWidgets::TabBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = View<QTabBar>::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void KDDockWidgets::QtWidgets::TabBar::dockWidgetInserted(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void KDDockWidgets::QtWidgets::TabBar::dockWidgetRemoved(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void KDDockWidgets::QtWidgets::TabBar::countChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void KDDockWidgets::QtWidgets::TabBar::currentDockWidgetChanged(KDDockWidgets::Core::DockWidget * _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}
QT_WARNING_POP
