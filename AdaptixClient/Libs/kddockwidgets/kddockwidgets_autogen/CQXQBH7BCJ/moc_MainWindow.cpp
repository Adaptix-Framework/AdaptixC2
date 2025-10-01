/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../qtwidgets/views/MainWindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13KDDockWidgets9QtWidgets10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto KDDockWidgets::QtWidgets::MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN13KDDockWidgets9QtWidgets10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KDDockWidgets::QtWidgets::MainWindow",
        "groupCountChanged",
        ""
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'groupCountChanged'
        QtMocHelpers::SignalData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 2 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN13KDDockWidgets9QtWidgets10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KDDockWidgets::QtWidgets::MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<View<QMainWindow>::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets10MainWindowE_t>.metaTypes,
    nullptr
} };

void KDDockWidgets::QtWidgets::MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->groupCountChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (MainWindow::*)(int )>(_a, &MainWindow::groupCountChanged, 0))
            return;
    }
}

const QMetaObject *KDDockWidgets::QtWidgets::MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KDDockWidgets::QtWidgets::MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets9QtWidgets10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "Core::MainWindowViewInterface"))
        return static_cast< Core::MainWindowViewInterface*>(this);
    return View<QMainWindow>::qt_metacast(_clname);
}

int KDDockWidgets::QtWidgets::MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = View<QMainWindow>::qt_metacall(_c, _id, _a);
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
void KDDockWidgets::QtWidgets::MainWindow::groupCountChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}
QT_WARNING_POP
