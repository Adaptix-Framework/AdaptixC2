/****************************************************************************
** Meta object code from reading C++ file 'DockWidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtwidgets/views/DockWidget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DockWidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13KDDockWidgets9QtWidgets10DockWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto KDDockWidgets::QtWidgets::DockWidget::qt_create_metaobjectdata<qt_meta_tag_ZN13KDDockWidgets9QtWidgets10DockWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KDDockWidgets::QtWidgets::DockWidget",
        "optionsChanged",
        "",
        "KDDockWidgets::DockWidgetOptions",
        "guestViewChanged",
        "isFocusedChanged",
        "isFloatingChanged",
        "isOpenChanged",
        "windowActiveAboutToChange",
        "isCurrentTabChanged"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'optionsChanged'
        QtMocHelpers::SignalData<void(KDDockWidgets::DockWidgetOptions)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 2 },
        }}),
        // Signal 'guestViewChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'isFocusedChanged'
        QtMocHelpers::SignalData<void(bool)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 2 },
        }}),
        // Signal 'isFloatingChanged'
        QtMocHelpers::SignalData<void(bool)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 2 },
        }}),
        // Signal 'isOpenChanged'
        QtMocHelpers::SignalData<void(bool)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 2 },
        }}),
        // Signal 'windowActiveAboutToChange'
        QtMocHelpers::SignalData<void(bool)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 2 },
        }}),
        // Signal 'isCurrentTabChanged'
        QtMocHelpers::SignalData<void(bool)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 2 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DockWidget, qt_meta_tag_ZN13KDDockWidgets9QtWidgets10DockWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KDDockWidgets::QtWidgets::DockWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QtWidgets::View<QWidget>::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets10DockWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets10DockWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets10DockWidgetE_t>.metaTypes,
    nullptr
} };

void KDDockWidgets::QtWidgets::DockWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DockWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->optionsChanged((*reinterpret_cast< std::add_pointer_t<KDDockWidgets::DockWidgetOptions>>(_a[1]))); break;
        case 1: _t->guestViewChanged(); break;
        case 2: _t->isFocusedChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->isFloatingChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->isOpenChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->windowActiveAboutToChange((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->isCurrentTabChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (DockWidget::*)(KDDockWidgets::DockWidgetOptions )>(_a, &DockWidget::optionsChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (DockWidget::*)()>(_a, &DockWidget::guestViewChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (DockWidget::*)(bool )>(_a, &DockWidget::isFocusedChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (DockWidget::*)(bool )>(_a, &DockWidget::isFloatingChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (DockWidget::*)(bool )>(_a, &DockWidget::isOpenChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (DockWidget::*)(bool )>(_a, &DockWidget::windowActiveAboutToChange, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (DockWidget::*)(bool )>(_a, &DockWidget::isCurrentTabChanged, 6))
            return;
    }
}

const QMetaObject *KDDockWidgets::QtWidgets::DockWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KDDockWidgets::QtWidgets::DockWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets10DockWidgetE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "Core::DockWidgetViewInterface"))
        return static_cast< Core::DockWidgetViewInterface*>(this);
    return QtWidgets::View<QWidget>::qt_metacast(_clname);
}

int KDDockWidgets::QtWidgets::DockWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QtWidgets::View<QWidget>::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void KDDockWidgets::QtWidgets::DockWidget::optionsChanged(KDDockWidgets::DockWidgetOptions _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void KDDockWidgets::QtWidgets::DockWidget::guestViewChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void KDDockWidgets::QtWidgets::DockWidget::isFocusedChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void KDDockWidgets::QtWidgets::DockWidget::isFloatingChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void KDDockWidgets::QtWidgets::DockWidget::isOpenChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void KDDockWidgets::QtWidgets::DockWidget::windowActiveAboutToChange(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void KDDockWidgets::QtWidgets::DockWidget::isCurrentTabChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}
QT_WARNING_POP
