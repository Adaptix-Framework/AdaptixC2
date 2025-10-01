/****************************************************************************
** Meta object code from reading C++ file 'FloatingWindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtwidgets/views/FloatingWindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'FloatingWindow.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13KDDockWidgets9QtWidgets14FloatingWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto KDDockWidgets::QtWidgets::FloatingWindow::qt_create_metaobjectdata<qt_meta_tag_ZN13KDDockWidgets9QtWidgets14FloatingWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KDDockWidgets::QtWidgets::FloatingWindow",
        "numGroupsChanged",
        ""
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'numGroupsChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<FloatingWindow, qt_meta_tag_ZN13KDDockWidgets9QtWidgets14FloatingWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KDDockWidgets::QtWidgets::FloatingWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<View<QWidget>::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets14FloatingWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets14FloatingWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets14FloatingWindowE_t>.metaTypes,
    nullptr
} };

void KDDockWidgets::QtWidgets::FloatingWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FloatingWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->numGroupsChanged(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (FloatingWindow::*)()>(_a, &FloatingWindow::numGroupsChanged, 0))
            return;
    }
}

const QMetaObject *KDDockWidgets::QtWidgets::FloatingWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KDDockWidgets::QtWidgets::FloatingWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets14FloatingWindowE_t>.strings))
        return static_cast<void*>(this);
    return View<QWidget>::qt_metacast(_clname);
}

int KDDockWidgets::QtWidgets::FloatingWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = View<QWidget>::qt_metacall(_c, _id, _a);
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

// SIGNAL 0
void KDDockWidgets::QtWidgets::FloatingWindow::numGroupsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
