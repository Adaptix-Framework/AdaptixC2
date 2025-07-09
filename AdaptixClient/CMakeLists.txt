cmake_minimum_required(VERSION 3.29)
project(AdaptixClient LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(UNIX)
    find_package( OpenSSL REQUIRED )
elseif(APPLE)
    find_package( OpenSSL REQUIRED )
elseif(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,windows")
endif()

find_package(Qt6
        REQUIRED COMPONENTS
        Core
        Gui
        Widgets
        Network
        WebSockets
        Sql
        Qml
)

include_directories(
        Headers
        Libs
)

### KONSOLE

set(KONSOLE_SOURCES

        Libs/Konsole/res.qrc

        Libs/Konsole/util/utf8proc.c
        Libs/Konsole/util/CharWidth.cpp
        Libs/Konsole/util/ColorScheme.cpp
        Libs/Konsole/util/Filter.cpp
        Libs/Konsole/util/History.cpp
        Libs/Konsole/util/HistorySearch.cpp
        Libs/Konsole/util/KeyboardTranslator.cpp
        Libs/Konsole/util/SearchBar.cpp
        Libs/Konsole/util/TerminalCharacterDecoder.cpp
        Libs/Konsole/Emulation.cpp
        Libs/Konsole/Vt102Emulation.cpp
        Libs/Konsole/Screen.cpp
        Libs/Konsole/ScreenWindow.cpp
        Libs/Konsole/TerminalDisplay.cpp
        Libs/Konsole/konsole.cpp
)

set_source_files_properties(
        Libs/Konsole/util/utf8proc.c
        PROPERTIES LANGUAGE C
)

set(KONSOLE_HEADERS
        Libs/Konsole/util/utf8proc.h
        Libs/Konsole/util/CharWidth.h
        Libs/Konsole/util/CharacterColor.h
        Libs/Konsole/util/Character.h
        Libs/Konsole/util/ColorScheme.h
        Libs/Konsole/util/Filter.h
        Libs/Konsole/util/History.h
        Libs/Konsole/util/HistorySearch.h
        Libs/Konsole/util/KeyboardTranslator.h
        Libs/Konsole/util/SearchBar.h
        Libs/Konsole/util/TerminalCharacterDecoder.h
        Libs/Konsole/Emulation.h
        Libs/Konsole/Vt102Emulation.h
        Libs/Konsole/Screen.h
        Libs/Konsole/ScreenWindow.h
        Libs/Konsole/TerminalDisplay.h
        Libs/Konsole/konsole.h
)

###

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
        Headers/Workers/WebSocketWorker.h
        Headers/UI/Dialogs/DialogSyncPacket.h
        Headers/UI/Widgets/ListenersWidget.h
        Headers/UI/Dialogs/DialogListener.h
        Headers/UI/Widgets/SessionsTableWidget.h
        Headers/UI/Dialogs/DialogAgent.h
        Headers/UI/Widgets/ConsoleWidget.h
        Headers/Agent/Agent.h
        Headers/Agent/AgentTableWidgetItem.h
        Headers/Workers/LastTickWorker.h
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
        Headers/UI/Widgets/ScreenshotsWidget.h
        Headers/UI/Dialogs/DialogTunnel.h
        Headers/Client/TunnelEndpoint.h
        Headers/Workers/TunnelWorker.h
        Headers/UI/Widgets/TerminalWidget.h
        Headers/Workers/TerminalWorker.h
        Headers/UI/Dialogs/DialogDownloader.h
        Headers/Workers/DownloaderWorker.h
        Headers/Workers/UploaderWorker.h
        Headers/UI/Dialogs/DialogUploader.h
        Headers/Client/AxScript/AxScriptManager.h
        Headers/Client/AxScript/AxScriptEngine.h
        Headers/Client/AxScript/BridgeApp.h
        Headers/Client/AxScript/BridgeEvent.h
        Headers/Client/AxScript/BridgeForm.h
        Headers/Client/AxScript/BridgeMenu.h
        Headers/Client/AxScript/AxCommandWrappers.h
        Headers/Client/AxScript/AxElementWrappers.h
        Headers/UI/Widgets/CredentialsWidget.h
        Headers/UI/Dialogs/DialogCredential.h
)

add_executable(AdaptixClient
        ${KONSOLE_SOURCES}
        ${KONSOLE_HEADERS}

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
        Source/Workers/WebSocketWorker.cpp
        Source/UI/Dialogs/DialogSyncPacket.cpp
        Source/Client/ProcessSyncPacket.cpp
        Source/UI/Widgets/ListenersWidget.cpp
        Source/UI/Dialogs/DialogListener.cpp
        Source/UI/Widgets/SessionsTableWidget.cpp
        Source/UI/Dialogs/DialogAgent.cpp
        Source/UI/Widgets/ConsoleWidget.cpp
        Source/Agent/Agent.cpp
        Source/Agent/AgentTableWidgetItem.cpp
        Source/Workers/LastTickWorker.cpp
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
        Source/UI/Widgets/ScreenshotsWidget.cpp
        Source/UI/Dialogs/DialogTunnel.cpp
        Source/Client/TunnelEndpoint.cpp
        Source/Workers/TunnelWorker.cpp
        Source/UI/Widgets/TerminalWidget.cpp
        Source/Workers/TerminalWorker.cpp
        Source/UI/Dialogs/DialogDownloader.cpp
        Source/Workers/DownloaderWorker.cpp
        Source/Workers/UploaderWorker.cpp
        Source/UI/Dialogs/DialogUploader.cpp
        Source/Client/AxScript/AxScriptManager.cpp
        Source/Client/AxScript/AxScriptEngine.cpp
        Source/Client/AxScript/BridgeApp.cpp
        Source/Client/AxScript/BridgeEvent.cpp
        Source/Client/AxScript/BridgeForm.cpp
        Source/Client/AxScript/BridgeMenu.cpp
        Source/Client/AxScript/AxCommandWrappers.cpp
        Source/Client/AxScript/AxElementWrappers.cpp
        Source/UI/Widgets/CredentialsWidget.cpp
        Source/UI/Dialogs/DialogCredential.cpp
)

target_compile_definitions(AdaptixClient PRIVATE QT_DEPRECATED_WARNINGS )

if(UNIX)
    target_link_libraries( AdaptixClient
            PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            Qt6::WebSockets
            Qt6::Sql
            Qt6::Qml
            OpenSSL::Crypto

            pthread dl
    )
elseif(APPLE)
    target_link_libraries( AdaptixClient
            PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            Qt6::WebSockets
            Qt6::Sql
            Qt6::Qml
            OpenSSL::Crypto

            pthread dl
    )
else()
    target_link_libraries( AdaptixClient
            PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            Qt6::WebSockets
            Qt6::Sql
            Qt6::Qml

            wsock32 ws2_32 crypt32 iphlpapi netapi32 version winmm userenv
    )
endif()