cmake_minimum_required(VERSION 3.29)
project(AXchecker)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include_directories( Headers )

find_package(Qt6
        COMPONENTS
        Core
        Gui
        Widgets
        REQUIRED
)

add_executable(AXchecker
        Source/main.cpp

        Resources/Resources.qrc

        Source/Classes/MainAX.cpp
        Headers/Classes/MainAX.h
        Headers/main.h
        Source/Classes/Utils.cpp
        Headers/Classes/Utils.h
        Source/Classes/WidgetBuilder.cpp
        Headers/Classes/WidgetBuilder.h
        Source/Classes/DialogView.cpp
        Headers/Classes/DialogView.h
)

target_link_libraries(AXchecker
        Qt::Core
        Qt::Gui
        Qt::Widgets
)