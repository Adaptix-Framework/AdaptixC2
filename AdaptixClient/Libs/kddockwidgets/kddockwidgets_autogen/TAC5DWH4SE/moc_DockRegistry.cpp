/****************************************************************************
** Meta object code from reading C++ file 'DockRegistry.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../core/DockRegistry.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'DockRegistry.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13KDDockWidgets12DockRegistryE_t {};
} // unnamed namespace

template <> constexpr inline auto KDDockWidgets::DockRegistry::qt_create_metaobjectdata<qt_meta_tag_ZN13KDDockWidgets12DockRegistryE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KDDockWidgets::DockRegistry",
        "focusedDockWidget",
        "KDDockWidgets::Core::DockWidget*",
        "",
        "containsDockWidget",
        "uniqueName",
        "containsMainWindow",
        "dockByName",
        "KDDockWidgets::DockRegistry::DockByNameFlags",
        "mainWindowByName",
        "KDDockWidgets::Core::MainWindow*",
        "hasFloatingWindows",
        "clear",
        "affinities"
    };

    QtMocHelpers::UintData qt_methods {
        // Method 'focusedDockWidget'
        QtMocHelpers::MethodData<KDDockWidgets::Core::DockWidget *() const>(1, 3, QMC::AccessPublic, 0x80000000 | 2),
        // Method 'containsDockWidget'
        QtMocHelpers::MethodData<bool(const QString &) const>(4, 3, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 5 },
        }}),
        // Method 'containsMainWindow'
        QtMocHelpers::MethodData<bool(const QString &) const>(6, 3, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::QString, 5 },
        }}),
        // Method 'dockByName'
        QtMocHelpers::MethodData<KDDockWidgets::Core::DockWidget *(const QString &, KDDockWidgets::DockRegistry::DockByNameFlags) const>(7, 3, QMC::AccessPublic, 0x80000000 | 2, {{
            { QMetaType::QString, 3 }, { 0x80000000 | 8, 3 },
        }}),
        // Method 'dockByName'
        QtMocHelpers::MethodData<KDDockWidgets::Core::DockWidget *(const QString &) const>(7, 3, QMC::AccessPublic | QMC::MethodCloned, 0x80000000 | 2, {{
            { QMetaType::QString, 3 },
        }}),
        // Method 'mainWindowByName'
        QtMocHelpers::MethodData<KDDockWidgets::Core::MainWindow *(const QString &) const>(9, 3, QMC::AccessPublic, 0x80000000 | 10, {{
            { QMetaType::QString, 3 },
        }}),
        // Method 'hasFloatingWindows'
        QtMocHelpers::MethodData<bool() const>(11, 3, QMC::AccessPublic, QMetaType::Bool),
        // Method 'clear'
        QtMocHelpers::MethodData<void(const QVector<QString> &)>(12, 3, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QStringList, 13 },
        }}),
        // Method 'clear'
        QtMocHelpers::MethodData<void()>(12, 3, QMC::AccessPublic | QMC::MethodCloned, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<DockRegistry, qt_meta_tag_ZN13KDDockWidgets12DockRegistryE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KDDockWidgets::DockRegistry::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets12DockRegistryE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets12DockRegistryE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13KDDockWidgets12DockRegistryE_t>.metaTypes,
    nullptr
} };

void KDDockWidgets::DockRegistry::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<DockRegistry *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: { KDDockWidgets::Core::DockWidget* _r = _t->focusedDockWidget();
            if (_a[0]) *reinterpret_cast< KDDockWidgets::Core::DockWidget**>(_a[0]) = std::move(_r); }  break;
        case 1: { bool _r = _t->containsDockWidget((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 2: { bool _r = _t->containsMainWindow((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 3: { KDDockWidgets::Core::DockWidget* _r = _t->dockByName((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<KDDockWidgets::DockRegistry::DockByNameFlags>>(_a[2])));
            if (_a[0]) *reinterpret_cast< KDDockWidgets::Core::DockWidget**>(_a[0]) = std::move(_r); }  break;
        case 4: { KDDockWidgets::Core::DockWidget* _r = _t->dockByName((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< KDDockWidgets::Core::DockWidget**>(_a[0]) = std::move(_r); }  break;
        case 5: { KDDockWidgets::Core::MainWindow* _r = _t->mainWindowByName((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])));
            if (_a[0]) *reinterpret_cast< KDDockWidgets::Core::MainWindow**>(_a[0]) = std::move(_r); }  break;
        case 6: { bool _r = _t->hasFloatingWindows();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 7: _t->clear((*reinterpret_cast< std::add_pointer_t<QList<QString>>>(_a[1]))); break;
        case 8: _t->clear(); break;
        default: ;
        }
    }
}

const QMetaObject *KDDockWidgets::DockRegistry::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KDDockWidgets::DockRegistry::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KDDockWidgets12DockRegistryE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "Core::EventFilterInterface"))
        return static_cast< Core::EventFilterInterface*>(this);
    return QObject::qt_metacast(_clname);
}

int KDDockWidgets::DockRegistry::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}
QT_WARNING_POP
