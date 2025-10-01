/****************************************************************************
** Meta object code from reading C++ file 'DragControllerWayland_p.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtcommon/DragControllerWayland_p.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DragControllerWayland_p.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13KDDockWidgets4Core20StateDraggingWaylandE_t {};
} // unnamed namespace

template <> constexpr inline auto KDDockWidgets::Core::StateDraggingWayland::qt_create_metaobjectdata<qt_meta_tag_ZN13KDDockWidgets4Core20StateDraggingWaylandE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KDDockWidgets::Core::StateDraggingWayland"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<StateDraggingWayland, qt_meta_tag_ZN13KDDockWidgets4Core20StateDraggingWaylandE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KDDockWidgets::Core::StateDraggingWayland::staticMetaObject = { {
    QMetaObject::SuperData::link<StateDragging::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core20StateDraggingWaylandE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core20StateDraggingWaylandE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13KDDockWidgets4Core20StateDraggingWaylandE_t>.metaTypes,
    nullptr
} };

void KDDockWidgets::Core::StateDraggingWayland::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<StateDraggingWayland *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *KDDockWidgets::Core::StateDraggingWayland::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KDDockWidgets::Core::StateDraggingWayland::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core20StateDraggingWaylandE_t>.strings))
        return static_cast<void*>(this);
    return StateDragging::qt_metacast(_clname);
}

int KDDockWidgets::Core::StateDraggingWayland::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = StateDragging::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN13KDDockWidgets4Core15WaylandMimeDataE_t {};
} // unnamed namespace

template <> constexpr inline auto KDDockWidgets::Core::WaylandMimeData::qt_create_metaobjectdata<qt_meta_tag_ZN13KDDockWidgets4Core15WaylandMimeDataE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KDDockWidgets::Core::WaylandMimeData"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<WaylandMimeData, qt_meta_tag_ZN13KDDockWidgets4Core15WaylandMimeDataE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KDDockWidgets::Core::WaylandMimeData::staticMetaObject = { {
    QMetaObject::SuperData::link<QMimeData::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core15WaylandMimeDataE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core15WaylandMimeDataE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13KDDockWidgets4Core15WaylandMimeDataE_t>.metaTypes,
    nullptr
} };

void KDDockWidgets::Core::WaylandMimeData::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<WaylandMimeData *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *KDDockWidgets::Core::WaylandMimeData::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KDDockWidgets::Core::WaylandMimeData::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core15WaylandMimeDataE_t>.strings))
        return static_cast<void*>(this);
    return QMimeData::qt_metacast(_clname);
}

int KDDockWidgets::Core::WaylandMimeData::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMimeData::qt_metacall(_c, _id, _a);
    return _id;
}
QT_WARNING_POP
