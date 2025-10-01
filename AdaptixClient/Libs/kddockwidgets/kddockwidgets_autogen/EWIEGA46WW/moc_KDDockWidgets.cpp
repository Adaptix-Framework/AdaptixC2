/****************************************************************************
** Meta object code from reading C++ file 'KDDockWidgets.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../KDDockWidgets.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'KDDockWidgets.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13KDDockWidgetsE_t {};
} // unnamed namespace

template <> constexpr inline auto KDDockWidgets::qt_create_metaobjectdata<qt_meta_tag_ZN13KDDockWidgetsE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KDDockWidgets",
        "Location",
        "Location_None",
        "Location_OnLeft",
        "Location_OnTop",
        "Location_OnRight",
        "Location_OnBottom",
        "MainWindowOptions",
        "MainWindowOption",
        "MainWindowOption_None",
        "MainWindowOption_HasCentralGroup",
        "MainWindowOption_HasCentralFrame",
        "MainWindowOption_MDI",
        "MainWindowOption_HasCentralWidget",
        "MainWindowOption_QDockWidgets",
        "MainWindowOption_ManualInit",
        "MainWindowOption_CentralWidgetGetsAllExtraSpace",
        "DockWidgetOptions",
        "DockWidgetOption",
        "DockWidgetOption_None",
        "DockWidgetOption_NotClosable",
        "DockWidgetOption_NotDockable",
        "DockWidgetOption_DeleteOnClose",
        "DockWidgetOption_MDINestable",
        "IconPlace",
        "TitleBar",
        "TabBar",
        "ToggleAction",
        "All",
        "FrontendType",
        "QtWidgets",
        "QtQuick",
        "Flutter",
        "DefaultSizeMode",
        "ItemSize",
        "Fair",
        "FairButFloor",
        "NoDefaultSizeMode",
        "AddingOption",
        "AddingOption_None",
        "AddingOption_StartHidden",
        "InitialVisibilityOption",
        "StartVisible",
        "StartHidden",
        "PreserveCurrentTab",
        "NeighbourSqueezeStrategy",
        "AllNeighbours",
        "ImmediateNeighboursFirst",
        "RestoreOptions",
        "RestoreOption",
        "RestoreOption_None",
        "RestoreOption_RelativeToMainWindow",
        "RestoreOption_AbsoluteFloatingDockWindows",
        "DropIndicatorType",
        "Classic",
        "Segmented",
        "None",
        "SuggestedGeometryHint",
        "SuggestedGeometryHint_None",
        "SuggestedGeometryHint_PreserveCenter",
        "SuggestedGeometryHint_GeometryIsFromDocked",
        "TitleBarButtonType",
        "Close",
        "Float",
        "Minimize",
        "Maximize",
        "Normal",
        "AutoHide",
        "UnautoHide",
        "AllTitleBarButtonTypes",
        "DropLocation",
        "DropLocation_None",
        "DropLocation_Left",
        "DropLocation_Top",
        "DropLocation_Right",
        "DropLocation_Bottom",
        "DropLocation_Center",
        "DropLocation_OutterLeft",
        "DropLocation_OutterTop",
        "DropLocation_OutterRight",
        "DropLocation_OutterBottom",
        "DropLocation_Inner",
        "DropLocation_Outter",
        "DropLocation_Horizontal",
        "DropLocation_Vertical",
        "CursorPosition",
        "CursorPosition_Undefined",
        "CursorPosition_Left",
        "CursorPosition_Right",
        "CursorPosition_Top",
        "CursorPosition_Bottom",
        "CursorPosition_TopLeft",
        "CursorPosition_TopRight",
        "CursorPosition_BottomRight",
        "CursorPosition_BottomLeft",
        "CursorPosition_Horizontal",
        "CursorPosition_Vertical",
        "CursorPosition_All",
        "FrameOptions",
        "FrameOption",
        "FrameOption_None",
        "FrameOption_AlwaysShowsTabs",
        "FrameOption_IsCentralFrame",
        "FrameOption_IsOverlayed",
        "FrameOption_NonDockable",
        "StackOptions",
        "StackOption",
        "StackOption_None",
        "StackOption_DocumentMode"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'Location'
        QtMocHelpers::EnumData<Location>(1, 1, QMC::EnumFlags{}).add({
            {    2, Location::Location_None },
            {    3, Location::Location_OnLeft },
            {    4, Location::Location_OnTop },
            {    5, Location::Location_OnRight },
            {    6, Location::Location_OnBottom },
        }),
        // enum 'MainWindowOptions'
        QtMocHelpers::EnumData<MainWindowOptions>(7, 8, QMC::EnumFlags{}).add({
            {    9, MainWindowOption::MainWindowOption_None },
            {   10, MainWindowOption::MainWindowOption_HasCentralGroup },
            {   11, MainWindowOption::MainWindowOption_HasCentralFrame },
            {   12, MainWindowOption::MainWindowOption_MDI },
            {   13, MainWindowOption::MainWindowOption_HasCentralWidget },
            {   14, MainWindowOption::MainWindowOption_QDockWidgets },
            {   15, MainWindowOption::MainWindowOption_ManualInit },
            {   16, MainWindowOption::MainWindowOption_CentralWidgetGetsAllExtraSpace },
        }),
        // enum 'DockWidgetOptions'
        QtMocHelpers::EnumData<DockWidgetOptions>(17, 18, QMC::EnumFlags{}).add({
            {   19, DockWidgetOption::DockWidgetOption_None },
            {   20, DockWidgetOption::DockWidgetOption_NotClosable },
            {   21, DockWidgetOption::DockWidgetOption_NotDockable },
            {   22, DockWidgetOption::DockWidgetOption_DeleteOnClose },
            {   23, DockWidgetOption::DockWidgetOption_MDINestable },
        }),
        // enum 'IconPlace'
        QtMocHelpers::EnumData<IconPlace>(24, 24, QMC::EnumIsScoped).add({
            {   25, IconPlace::TitleBar },
            {   26, IconPlace::TabBar },
            {   27, IconPlace::ToggleAction },
            {   28, IconPlace::All },
        }),
        // enum 'FrontendType'
        QtMocHelpers::EnumData<FrontendType>(29, 29, QMC::EnumIsScoped).add({
            {   30, FrontendType::QtWidgets },
            {   31, FrontendType::QtQuick },
            {   32, FrontendType::Flutter },
        }),
        // enum 'DefaultSizeMode'
        QtMocHelpers::EnumData<DefaultSizeMode>(33, 33, QMC::EnumIsScoped).add({
            {   34, DefaultSizeMode::ItemSize },
            {   35, DefaultSizeMode::Fair },
            {   36, DefaultSizeMode::FairButFloor },
            {   37, DefaultSizeMode::NoDefaultSizeMode },
        }),
        // enum 'AddingOption'
        QtMocHelpers::EnumData<AddingOption>(38, 38, QMC::EnumFlags{}).add({
            {   39, AddingOption::AddingOption_None },
            {   40, AddingOption::AddingOption_StartHidden },
        }),
        // enum 'InitialVisibilityOption'
        QtMocHelpers::EnumData<InitialVisibilityOption>(41, 41, QMC::EnumIsScoped).add({
            {   42, InitialVisibilityOption::StartVisible },
            {   43, InitialVisibilityOption::StartHidden },
            {   44, InitialVisibilityOption::PreserveCurrentTab },
        }),
        // enum 'NeighbourSqueezeStrategy'
        QtMocHelpers::EnumData<NeighbourSqueezeStrategy>(45, 45, QMC::EnumIsScoped).add({
            {   46, NeighbourSqueezeStrategy::AllNeighbours },
            {   47, NeighbourSqueezeStrategy::ImmediateNeighboursFirst },
        }),
        // enum 'RestoreOptions'
        QtMocHelpers::EnumData<RestoreOptions>(48, 49, QMC::EnumFlags{}).add({
            {   50, RestoreOption::RestoreOption_None },
            {   51, RestoreOption::RestoreOption_RelativeToMainWindow },
            {   52, RestoreOption::RestoreOption_AbsoluteFloatingDockWindows },
        }),
        // enum 'DropIndicatorType'
        QtMocHelpers::EnumData<DropIndicatorType>(53, 53, QMC::EnumIsScoped).add({
            {   54, DropIndicatorType::Classic },
            {   55, DropIndicatorType::Segmented },
            {   56, DropIndicatorType::None },
        }),
        // enum 'SuggestedGeometryHint'
        QtMocHelpers::EnumData<SuggestedGeometryHint>(57, 57, QMC::EnumFlags{}).add({
            {   58, SuggestedGeometryHint::SuggestedGeometryHint_None },
            {   59, SuggestedGeometryHint::SuggestedGeometryHint_PreserveCenter },
            {   60, SuggestedGeometryHint::SuggestedGeometryHint_GeometryIsFromDocked },
        }),
        // enum 'TitleBarButtonType'
        QtMocHelpers::EnumData<TitleBarButtonType>(61, 61, QMC::EnumIsScoped).add({
            {   62, TitleBarButtonType::Close },
            {   63, TitleBarButtonType::Float },
            {   64, TitleBarButtonType::Minimize },
            {   65, TitleBarButtonType::Maximize },
            {   66, TitleBarButtonType::Normal },
            {   67, TitleBarButtonType::AutoHide },
            {   68, TitleBarButtonType::UnautoHide },
            {   69, TitleBarButtonType::AllTitleBarButtonTypes },
        }),
        // enum 'DropLocation'
        QtMocHelpers::EnumData<DropLocation>(70, 70, QMC::EnumFlags{}).add({
            {   71, DropLocation::DropLocation_None },
            {   72, DropLocation::DropLocation_Left },
            {   73, DropLocation::DropLocation_Top },
            {   74, DropLocation::DropLocation_Right },
            {   75, DropLocation::DropLocation_Bottom },
            {   76, DropLocation::DropLocation_Center },
            {   77, DropLocation::DropLocation_OutterLeft },
            {   78, DropLocation::DropLocation_OutterTop },
            {   79, DropLocation::DropLocation_OutterRight },
            {   80, DropLocation::DropLocation_OutterBottom },
            {   81, DropLocation::DropLocation_Inner },
            {   82, DropLocation::DropLocation_Outter },
            {   83, DropLocation::DropLocation_Horizontal },
            {   84, DropLocation::DropLocation_Vertical },
        }),
        // enum 'CursorPosition'
        QtMocHelpers::EnumData<CursorPosition>(85, 85, QMC::EnumFlags{}).add({
            {   86, CursorPosition::CursorPosition_Undefined },
            {   87, CursorPosition::CursorPosition_Left },
            {   88, CursorPosition::CursorPosition_Right },
            {   89, CursorPosition::CursorPosition_Top },
            {   90, CursorPosition::CursorPosition_Bottom },
            {   91, CursorPosition::CursorPosition_TopLeft },
            {   92, CursorPosition::CursorPosition_TopRight },
            {   93, CursorPosition::CursorPosition_BottomRight },
            {   94, CursorPosition::CursorPosition_BottomLeft },
            {   95, CursorPosition::CursorPosition_Horizontal },
            {   96, CursorPosition::CursorPosition_Vertical },
            {   97, CursorPosition::CursorPosition_All },
        }),
        // enum 'FrameOptions'
        QtMocHelpers::EnumData<FrameOptions>(98, 99, QMC::EnumFlags{}).add({
            {  100, FrameOption::FrameOption_None },
            {  101, FrameOption::FrameOption_AlwaysShowsTabs },
            {  102, FrameOption::FrameOption_IsCentralFrame },
            {  103, FrameOption::FrameOption_IsOverlayed },
            {  104, FrameOption::FrameOption_NonDockable },
        }),
        // enum 'StackOptions'
        QtMocHelpers::EnumData<StackOptions>(105, 106, QMC::EnumFlags{}).add({
            {  107, StackOption::StackOption_None },
            {  108, StackOption::StackOption_DocumentMode },
        }),
    };
    return QtMocHelpers::metaObjectData<void, qt_meta_tag_ZN13KDDockWidgetsE_t>(QMC::PropertyAccessInStaticMetaCall, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}

static constexpr auto qt_staticMetaObjectContent_ZN13KDDockWidgetsE =
    KDDockWidgets::qt_create_metaobjectdata<qt_meta_tag_ZN13KDDockWidgetsE_t>();
static constexpr auto qt_staticMetaObjectStaticContent_ZN13KDDockWidgetsE =
    qt_staticMetaObjectContent_ZN13KDDockWidgetsE.staticData;
static constexpr auto qt_staticMetaObjectRelocatingContent_ZN13KDDockWidgetsE =
    qt_staticMetaObjectContent_ZN13KDDockWidgetsE.relocatingData;

Q_CONSTINIT const QMetaObject KDDockWidgets::staticMetaObject = { {
    nullptr,
    qt_staticMetaObjectStaticContent_ZN13KDDockWidgetsE.stringdata,
    qt_staticMetaObjectStaticContent_ZN13KDDockWidgetsE.data,
    nullptr,
    nullptr,
    qt_staticMetaObjectRelocatingContent_ZN13KDDockWidgetsE.metaTypes,
    nullptr
} };

QT_WARNING_POP
