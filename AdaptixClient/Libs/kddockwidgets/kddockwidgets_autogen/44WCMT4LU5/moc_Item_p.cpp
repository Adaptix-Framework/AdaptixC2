/****************************************************************************
** Meta object code from reading C++ file 'Item_p.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../core/layouting/Item_p.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Item_p.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13KDDockWidgets4Core4ItemE_t {};
} // unnamed namespace

template <> constexpr inline auto KDDockWidgets::Core::Item::qt_create_metaobjectdata<qt_meta_tag_ZN13KDDockWidgets4Core4ItemE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KDDockWidgets::Core::Item",
        "onWidgetLayoutRequested",
        ""
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onWidgetLayoutRequested'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Item, qt_meta_tag_ZN13KDDockWidgets4Core4ItemE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KDDockWidgets::Core::Item::staticMetaObject = { {
    QMetaObject::SuperData::link<Core::Object::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core4ItemE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core4ItemE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13KDDockWidgets4Core4ItemE_t>.metaTypes,
    nullptr
} };

void KDDockWidgets::Core::Item::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Item *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onWidgetLayoutRequested(); break;
        default: ;
        }
    }
    (void)_a;
}

const QMetaObject *KDDockWidgets::Core::Item::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KDDockWidgets::Core::Item::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core4ItemE_t>.strings))
        return static_cast<void*>(this);
    return Core::Object::qt_metacast(_clname);
}

int KDDockWidgets::Core::Item::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Core::Object::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 1;
    }
    return _id;
}
namespace {
struct qt_meta_tag_ZN13KDDockWidgets4Core13ItemContainerE_t {};
} // unnamed namespace

template <> constexpr inline auto KDDockWidgets::Core::ItemContainer::qt_create_metaobjectdata<qt_meta_tag_ZN13KDDockWidgets4Core13ItemContainerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KDDockWidgets::Core::ItemContainer"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ItemContainer, qt_meta_tag_ZN13KDDockWidgets4Core13ItemContainerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KDDockWidgets::Core::ItemContainer::staticMetaObject = { {
    QMetaObject::SuperData::link<Item::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core13ItemContainerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core13ItemContainerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13KDDockWidgets4Core13ItemContainerE_t>.metaTypes,
    nullptr
} };

void KDDockWidgets::Core::ItemContainer::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ItemContainer *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *KDDockWidgets::Core::ItemContainer::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KDDockWidgets::Core::ItemContainer::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core13ItemContainerE_t>.strings))
        return static_cast<void*>(this);
    return Item::qt_metacast(_clname);
}

int KDDockWidgets::Core::ItemContainer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = Item::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN13KDDockWidgets4Core16ItemBoxContainerE_t {};
} // unnamed namespace

template <> constexpr inline auto KDDockWidgets::Core::ItemBoxContainer::qt_create_metaobjectdata<qt_meta_tag_ZN13KDDockWidgets4Core16ItemBoxContainerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KDDockWidgets::Core::ItemBoxContainer"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ItemBoxContainer, qt_meta_tag_ZN13KDDockWidgets4Core16ItemBoxContainerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KDDockWidgets::Core::ItemBoxContainer::staticMetaObject = { {
    QMetaObject::SuperData::link<ItemContainer::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core16ItemBoxContainerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core16ItemBoxContainerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13KDDockWidgets4Core16ItemBoxContainerE_t>.metaTypes,
    nullptr
} };

void KDDockWidgets::Core::ItemBoxContainer::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ItemBoxContainer *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *KDDockWidgets::Core::ItemBoxContainer::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KDDockWidgets::Core::ItemBoxContainer::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets4Core16ItemBoxContainerE_t>.strings))
        return static_cast<void*>(this);
    return ItemContainer::qt_metacast(_clname);
}

int KDDockWidgets::Core::ItemBoxContainer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = ItemContainer::qt_metacall(_c, _id, _a);
    return _id;
}
QT_WARNING_POP
