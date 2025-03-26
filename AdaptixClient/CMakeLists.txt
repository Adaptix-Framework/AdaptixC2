cmake_minimum_required(VERSION 3.28)
project(AdaptixClient)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


if(UNIX)
    find_package( OpenSSL REQUIRED )
elseif(APPLE)
    find_package( OpenSSL REQUIRED )
elseif(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,windows")
endif()

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
        Headers/Agent/AgentTableWidgetItem.h
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
        Headers/Client/Settings.h
        Headers/UI/Widgets/TunnelsWidget.h
        Headers/Utils/KeyPressHandler.h
        Headers/Agent/Task.h
        Headers/Agent/TaskTableWidgetItem.h
        Headers/UI/Graph/SessionsGraph.h
        Headers/UI/Graph/GraphItem.h
        Headers/UI/Graph/GraphItemLink.h
        Headers/UI/Graph/GraphScene.h
        Headers/UI/Graph/LayoutTreeLeft.h
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
        Source/Agent/AgentTableWidgetItem.cpp
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
        Source/Client/Settings.cpp
        Source/UI/Widgets/TunnelsWidget.cpp
        Source/Agent/Task.cpp
        Source/Agent/TaskTableWidgetItem.cpp
        Source/UI/Graph/SessionsGraph.cpp
        Source/UI/Graph/GraphItem.cpp
        Source/UI/Graph/GraphItemLink.cpp
        Source/UI/Graph/GraphScene.cpp
        Source/UI/Graph/LayoutTreeLeft.cpp
)

if(UNIX)
    target_link_libraries( AdaptixClient
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            Qt6::WebSockets
            Qt6::Sql
            OpenSSL::Crypto
    )
elseif(APPLE)
    target_link_libraries( AdaptixClient
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            Qt6::WebSockets
            Qt6::Sql
            OpenSSL::Crypto
    )
else()
    target_link_libraries( AdaptixClient
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            Qt6::WebSockets
            Qt6::Sql
    )
endif()