cmake_minimum_required(VERSION 3.29)
project(CmdChecker)

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
        REQUIRED)

add_executable(CmdChecker

        Resources/Resources.qrc

        Source/main.cpp
        Headers/main.h
        Source/Classes/MainCmd.cpp
        Headers/Classes/MainCmd.h
        Headers/Classes/Utils.h
        Source/Classes/Utils.cpp
        Headers/Classes/Commander.h
        Source/Classes/Commander.cpp
)

target_link_libraries(CmdChecker
        Qt::Core
        Qt::Gui
        Qt::Widgets
)

