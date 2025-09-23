cmake_minimum_required(VERSION 3.15)
project(KDDockWidgets LANGUAGES CXX C)

option(KDDockWidgets_XLib "On Linux, link against XLib, for a more robust window z-order detection." OFF)

add_definitions( -DQT_USE_QSTRINGBUILDER -DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT -DQT_STRICT_ITERATORS -DQT_NO_KEYWORDS -DQT_NO_FOREACH ) # -DQT_NO_SIGNALS_SLOTS_KEYWORDS

include(${CMAKE_CURRENT_SOURCE_DIR}/kdbindings.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/nlohmann.cmake)

set(KDDW_LAYOUTING_SRCS
        core/layouting/Item.cpp
        core/layouting/ItemFreeContainer.cpp
        core/Logging.cpp
        KDDockWidgets.cpp
)

set(KDDW_BACKEND_SRCS
        Config.cpp
        LayoutSaver.cpp
        core/Position.cpp
        core/DelayedCall.cpp
        core/Draggable.cpp
        core/WindowBeingDragged.cpp
        core/DragController.cpp
        core/WidgetResizeHandler.cpp
        core/Action.cpp
        core/DockRegistry.cpp
        core/FocusScope.cpp
        core/DockWidget.cpp
        core/DropArea.cpp
        core/DropIndicatorOverlay.cpp
        core/FloatingWindow.cpp
        core/Group.cpp
        core/Layout.cpp
        core/MainWindow.cpp
        core/MDILayout.cpp
        core/Separator.cpp
        core/SideBar.cpp
        core/Stack.cpp
        core/TitleBar.cpp
        core/TabBar.cpp
        core/ViewFactory.cpp
        core/Window.cpp
        core/Screen.cpp
        core/ViewGuard.cpp
        core/Controller.cpp
        core/Platform.cpp
        core/View.cpp
        core/indicators/NullDropIndicatorOverlay.cpp
        core/indicators/ClassicDropIndicatorOverlay.cpp
        core/indicators/SegmentedDropIndicatorOverlay.cpp
        core/views/ClassicIndicatorWindowViewInterface.cpp
        core/views/MainWindowMDIViewInterface.cpp
        core/views/MainWindowViewInterface.cpp
        core/views/GroupViewInterface.cpp
        core/views/DockWidgetViewInterface.cpp
        core/views/StackViewInterface.cpp
        core/views/TabBarViewInterface.cpp
        core/views/TitleBarViewInterface.cpp
        core/views/SideBarViewInterface.cpp
)

set(KDDW_QTCOMMON_SRCS
        qtcommon/Platform.cpp
        qtcommon/Window.cpp
        qtcommon/View.cpp
        qtcommon/Screen.cpp
        qtcommon/ViewWrapper.cpp
        qtcommon/CustomFrameHelper.cpp
        qtcommon/DragControllerWayland_p.cpp
)

set(KDDW_FRONTEND_QTWIDGETS_SRCS
        qtwidgets/Action.cpp
        qtwidgets/Window.cpp
        qtwidgets/Platform.cpp
        qtwidgets/views/View.cpp
        qtwidgets/views/ViewWrapper.cpp
        qtwidgets/views/FloatingWindow.cpp
        qtwidgets/views/DockWidget.cpp
        qtwidgets/views/DropArea.cpp
        qtwidgets/views/MDILayout.cpp
        qtwidgets/views/Group.cpp
        qtwidgets/views/MainWindow.cpp
        qtwidgets/views/MainWindowMDI.cpp
        qtwidgets/views/MDIArea.cpp
        qtwidgets/views/RubberBand.cpp
        qtwidgets/views/Separator.cpp
        qtwidgets/views/TitleBar.cpp
        qtwidgets/views/TabBar.cpp
        qtwidgets/views/Stack.cpp
        qtwidgets/views/SideBar.cpp
        qtwidgets/views/ClassicIndicatorsWindow.cpp
        qtwidgets/views/SegmentedDropIndicatorOverlay.cpp
        qtwidgets/ViewFactory.cpp
        qtwidgets/DebugWindow.cpp
        qtwidgets/ObjectViewer.cpp
)

set(KDDW_FRONTEND_QTCOMPAT_SRCS qtcompat/Object.cpp)

set(KDDW_PUBLIC_HEADERS
        docks_export.h
        Config.h
        KDDockWidgets.h
        LayoutSaver.h
        Qt5Qt6Compat_p.h
        QtCompat_p.h
)

set(DOCKSLIBS_SRCS
        ${KDDW_LAYOUTING_SRCS}
        ${KDDW_FRONTEND_QTWIDGETS_SRCS}
        ${KDDW_BACKEND_SRCS}
        ${KDDW_QTCOMMON_SRCS}
)

add_library(kddockwidgets ${DOCKSLIBS_SRCS} ${KDDW_PUBLIC_HEADERS})

target_compile_definitions(
        kddockwidgets
        PUBLIC KDDW_FRONTEND_QTWIDGETS KDDW_FRONTEND_QT
        PRIVATE BUILDING_DOCKS_LIBRARY
)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(Qt6 COMPONENTS GuiPrivate WidgetsPrivate QUIET)

if (TARGET Qt6::GuiPrivate)
    target_link_libraries(kddockwidgets PRIVATE Qt6::GuiPrivate)
    target_include_directories(kddockwidgets PRIVATE $<TARGET_PROPERTY:Qt6::GuiPrivate,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

if (TARGET Qt6::WidgetsPrivate)
    target_link_libraries(kddockwidgets PRIVATE Qt6::WidgetsPrivate)
endif()

target_link_libraries(kddockwidgets PUBLIC Qt6::Core Qt6::Widgets)

if(KDDockWidgets_XLib)
    message(STATUS "KDDockWidgets_XLib enabled: will link to X11 and define KDDockWidgets_XLIB")
    add_definitions(-DKDDockWidgets_XLIB)
    find_package(X11 REQUIRED)
    target_link_libraries(kddockwidgets PRIVATE X11::X11)
else()
    message(STATUS "KDDockWidgets_XLib disabled (default). X11-specific code will be inactive.")
endif()

set(FWD_HEADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fwd_headers/kddockwidgets)
file(MAKE_DIRECTORY ${FWD_HEADERS_DIR})

set(PUBLIC_HEADERS_TO_FORWARD
        qtwidgets/ViewFactory.h
        qtwidgets/views/DockWidget.h
        qtwidgets/views/DropArea.h
        qtwidgets/views/FloatingWindow.h
        qtwidgets/views/MDILayout.h
        qtwidgets/views/Group.h
        qtwidgets/views/MainWindow.h
        qtwidgets/views/MainWindowMDI.h
        qtwidgets/views/MDIArea.h
        qtwidgets/views/RubberBand.h
        qtwidgets/views/Separator.h
        qtwidgets/views/SideBar.h
        qtwidgets/views/Stack.h
        qtwidgets/views/TabBar.h
        qtwidgets/views/TitleBar.h
        qtwidgets/views/ClassicIndicatorsWindow.h
        qtwidgets/views/SegmentedDropIndicatorOverlay.h
)

foreach(hdr IN LISTS PUBLIC_HEADERS_TO_FORWARD)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/${hdr} DESTINATION ${FWD_HEADERS_DIR})
endforeach()

target_include_directories(kddockwidgets
        PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/fwd_headers
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/core/views
        ${CMAKE_CURRENT_SOURCE_DIR}/core/indicators
        ${CMAKE_CURRENT_SOURCE_DIR}/qtcommon
        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/nlohmann
)

kddw_link_to_kdbindings(kddockwidgets)
link_to_nlohman(kddockwidgets)

include(GenerateExportHeader)
generate_export_header(kddockwidgets EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/kddockwidgets_export.h")
