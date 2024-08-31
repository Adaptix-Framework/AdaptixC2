cmake_minimum_required(VERSION 3.29)
project(AdaptixClient)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package( OpenSSL REQUIRED )

include_directories( Headers )

find_package(Qt6
        COMPONENTS
        Core
        Gui
        Widgets
        Network
        WebSockets
        Sql
        REQUIRED
)

SET( HEADERS
        Headers/main.h
        Headers/Utils/Logs.h
        Headers/UI/Dialogs/DialogConnect.h
        Headers/Utils/Valid.h
        Headers/MainAdaptix.h
        Headers/Client/Storage.h
        Headers/Client/AuthProfile.h
        Headers/Utils/FileSystem.h
)

add_executable(AdaptixClient

        ${HEADERS}

        Source/main.cpp

        Resources/Resources.qrc
        Source/Utils/Logs.cpp
        Source/UI/Dialogs/DialogConnect.cpp
        Source/Utils/Valid.cpp
        Source/MainAdaptix.cpp
        Source/Client/Storage.cpp
        Source/Client/AuthProfile.cpp
        Source/Utils/FileSystem.cpp
)

target_link_libraries(AdaptixClient
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::WebSockets
        Qt6::Sql
        OpenSSL::Crypto
)

