cmake_minimum_required(VERSION 3.29)
project(AdaptixClient)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package( OpenSSL REQUIRED )

find_package(Qt6
        COMPONENTS
        Core
        Gui
        Widgets
        Network
        WebSockets
        REQUIRED
)

add_executable(AdaptixClient

        Source/main.cpp

        Resources/Resources.qrc
)

target_link_libraries(AdaptixClient
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt6::Network
        Qt6::WebSockets
        OpenSSL::Crypto
)

