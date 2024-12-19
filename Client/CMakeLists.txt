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
        Headers/Utils/Convert.h
        Headers/MainAdaptix.h
        Headers/Client/Storage.h
        Headers/Client/AuthProfile.h
        Headers/Utils/FileSystem.h
        Headers/Client/Requestor.h
        Headers/UI/MainUI.h
        Headers/UI/Widgets/AdaptixWidget.h
        Headers/UI/Widgets/LogsWidget.h
        Headers/Client/WebSocketWorker.h
        Headers/UI/Dialogs/DialogSyncPacket.h
        Headers/UI/Widgets/ListenersWidget.h
        Headers/UI/Dialogs/DialogListener.h
        Headers/Client/WidgetBuilder.h
        Headers/UI/Widgets/SessionsTableWidget.h
        Headers/UI/Dialogs/DialogAgent.h
        Headers/UI/Widgets/ConsoleWidget.h
        Headers/Agent/Agent.h
        Headers/Agent/TableWidgetItemAgent.h
        Headers/Client/LastTickWorker.h
        Headers/Agent/Commander.h
        Headers/UI/Widgets/DownloadsWidget.h
        Headers/UI/Widgets/BrowserFilesWidget.h
        Headers/UI/Widgets/BrowserProcessWidget.h
        Headers/UI/Widgets/TasksWidget.h
        Headers/UI/Dialogs/DialogSettings.h
        Headers/Client/Extender.h
        Headers/UI/Dialogs/DialogExtender.h
        Headers/Utils/CustomElements.h
)

add_executable(AdaptixClient

        ${HEADERS}

        Source/main.cpp

        Resources/Resources.qrc
        Source/Utils/Logs.cpp
        Source/UI/Dialogs/DialogConnect.cpp
        Source/Utils/Convert.cpp
        Source/MainAdaptix.cpp
        Source/Client/Storage.cpp
        Source/Client/AuthProfile.cpp
        Source/Utils/FileSystem.cpp
        Source/Client/Requestor.cpp
        Source/UI/MainUI.cpp
        Source/UI/Widgets/AdaptixWidget.cpp
        Source/UI/Widgets/LogsWidget.cpp
        Source/Client/WebSocketWorker.cpp
        Source/UI/Dialogs/DialogSyncPacket.cpp
        Source/Client/ProcessSyncPacket.cpp
        Source/UI/Widgets/ListenersWidget.cpp
        Source/UI/Dialogs/DialogListener.cpp
        Source/Client/WidgetBuilder.cpp
        Source/UI/Widgets/SessionsTableWidget.cpp
        Source/UI/Dialogs/DialogAgent.cpp
        Source/UI/Widgets/ConsoleWidget.cpp
        Source/Agent/Agent.cpp
        Source/Agent/TableWidgetItemAgent.cpp
        Source/Client/LastTickWorker.cpp
        Source/Agent/Commander.cpp
        Source/UI/Widgets/DownloadsWidget.cpp
        Source/UI/Widgets/BrowserFilesWidget.cpp
        Source/UI/Widgets/BrowserProcessWidget.cpp
        Source/UI/Widgets/TasksWidget.cpp
        Source/UI/Dialogs/DialogSettings.cpp
        Source/Client/Extender.cpp
        Source/UI/Dialogs/DialogExtender.cpp
        Source/Utils/CustomElements.cpp
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

