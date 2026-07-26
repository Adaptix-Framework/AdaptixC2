cmake_minimum_required(VERSION 3.28) # 3.15
project(AdaptixClient LANGUAGES C CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(Qt6_DIR "C:/msys64/mingw64/lib/cmake")
endif()
set(QT_NO_PRIVATE_MODULE_WARNING ON)

add_subdirectory(Libs/kddockwidgets)
add_subdirectory(Libs/qlementine)

if(WIN32 AND MINGW)
    set(OPENSSL_USE_STATIC_LIBS TRUE)
endif()
find_package( OpenSSL REQUIRED )

if(WIN32 AND MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,windows")
endif()

if(APPLE)
    execute_process(
            COMMAND brew --prefix qt@6
            OUTPUT_VARIABLE HOMEBREW_QT6_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE BREW_RESULT
    )
    if(BREW_RESULT EQUAL 0 AND EXISTS "${HOMEBREW_QT6_PREFIX}")
        list(PREPEND CMAKE_PREFIX_PATH "${HOMEBREW_QT6_PREFIX}")
    else()
        message(FATAL_ERROR "Qt6 not found via Homebrew. Install with: brew install qt@6")
    endif()
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
        Libs/CodeEditor
        Libs/CodeEditor/highlighters
        Libs/CodeEditor/completers
)

### KDDOCKWIDGETS

set(KDDOCKWIDGETS_RESOURCES Libs/kddockwidgets/kddockwidgets_resources.qrc)

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

set(CODEEDITOR_SOURCES
        Libs/CodeEditor/resources/codeeditor_resources.qrc

        Libs/CodeEditor/SyntaxStyle.cpp
        Libs/CodeEditor/StyleSyntaxHighlighter.cpp
        Libs/CodeEditor/Language.cpp
        Libs/CodeEditor/LineNumberArea.cpp
        Libs/CodeEditor/FrameHighlight.cpp
        Libs/CodeEditor/CodeFolding.cpp
        Libs/CodeEditor/ErrorIndicator.cpp
        Libs/CodeEditor/Minimap.cpp
        Libs/CodeEditor/CodeEditor.cpp
        Libs/CodeEditor/EditorTabWidget.cpp
        Libs/CodeEditor/FileBrowser.cpp
        Libs/CodeEditor/BuildPanel.cpp
        Libs/CodeEditor/FindReplace.cpp
        Libs/CodeEditor/CodeEditorView.cpp
        Libs/CodeEditor/highlighters/CXXHighlighter.cpp
        Libs/CodeEditor/highlighters/AxScriptHighlighter.cpp
        Libs/CodeEditor/completers/CXXCompleter.cpp
        Libs/CodeEditor/completers/AxScriptCompleter.cpp
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
        Headers/Client/HttpRequestManager.h
        Headers/UI/MainUI.h
        Headers/UI/Widgets/AdaptixWidget.h
        Headers/UI/Widgets/LogsWidget.h
        Headers/Workers/WebSocketWorker.h
        Headers/UI/Dialogs/DialogSyncPacket.h
        Headers/UI/Dialogs/DialogListener.h
        Headers/UI/Dialogs/DialogListenerConnector.h
        Headers/UI/Dialogs/DialogAgent.h
        Headers/UI/Widgets/ConsoleWidget.h
        Headers/Agent/Agent.h
        Headers/Workers/LastTickWorker.h
        Headers/Agent/Commander.h
        Headers/UI/Widgets/BrowserFilesWidget.h
        Headers/UI/Widgets/BrowserProcessWidget.h
        Headers/UI/Dialogs/DialogSettings.h
        Headers/Client/Extender.h
        Headers/Client/CodeEditorProfileManager.h
        Headers/Client/Settings.h
        Headers/Client/ConsoleTheme.h
        Headers/Utils/KeyPressHandler.h
        Headers/UI/Graph/SessionsGraph.h
        Headers/UI/Graph/GraphItem.h
        Headers/UI/Graph/GraphItemLink.h
        Headers/UI/Graph/GraphScene.h
        Headers/UI/Graph/LayoutTreeLeft.h
        Headers/UI/Graph/LayoutTreeTop.h
        Headers/UI/Dialogs/DialogTunnel.h
        Headers/Client/TunnelEndpoint.h
        Headers/Workers/TunnelWorker.h
        Headers/Workers/SocksHandshakeWorker.h
        Headers/UI/Widgets/TerminalContainerWidget.h
        Headers/Workers/TerminalWorker.h
        Headers/Workers/BuildWorker.h
        Headers/UI/Dialogs/DialogDownloader.h
        Headers/Workers/DownloaderWorker.h
        Headers/Workers/UploaderWorker.h
        Headers/UI/Dialogs/DialogUploader.h
        Headers/Client/AxScript/AxScriptManager.h
        Headers/Client/AxScript/AxScriptEngine.h
        Headers/Client/AxScript/AxScriptWorker.h
        Headers/Client/AxScript/AxUiFactory.h
        Headers/Client/AxScript/BridgeApp.h
        Headers/Client/AxScript/BridgeEvent.h
        Headers/Client/AxScript/BridgeForm.h
        Headers/Client/AxScript/BridgeMenu.h
        Headers/Client/AxScript/AxCommandWrappers.h
        Headers/Client/AxScript/AxElementWrappers.h
        Headers/UI/Dialogs/DialogCredential.h
        Headers/UI/Dialogs/DialogTarget.h
        Headers/UI/Dialogs/DialogSaveTask.h
        Headers/UI/Widgets/ChatWidget.h
        Headers/UI/Widgets/AbstractDock.h
        Headers/Utils/NonBlockingDialogs.h
        Headers/UI/Dialogs/DialogSubscriptions.h
        Headers/Utils/FontManager.h
        Headers/Utils/TitleBarStyle.h
        Headers/UI/Widgets/DockWidgetRegister.h
        Headers/Client/FilterExprParser.h
        Headers/Client/PagedTableHelper.h
        Headers/Utils/FilterExpression.h
        Headers/UI/Models/GroupingProxyModel.h
        Headers/Utils/CustomElements/BoldHeaderView.h
        Headers/Utils/CustomElements/ClickableLabel.h
        Headers/Utils/CustomElements/Delegates.h
        Headers/Utils/CustomElements/GroupManagerPopup.h
        Headers/Utils/CustomElements/PageNavBar.h
        Headers/Utils/CustomElements/SearchPanel.h
        Headers/Utils/CustomElements/SpinTable.h
        Headers/Utils/CustomElements/TextEditConsole.h
        Headers/Utils/CustomElements/VerticalTabWidget.h
        Headers/Utils/CustomElements/ListFeed.h
        Headers/UI/Widgets/SessionsTableWidget.h
        Headers/UI/Widgets/SessionsFeedWidget.h
        Headers/UI/Widgets/SessionWidgetIface.h
        Headers/UI/Widgets/TasksFeedWidget.h
        Headers/UI/Widgets/ListenersFeedWidget.h
        Headers/UI/Widgets/TunnelsFeedWidget.h
        Headers/UI/Widgets/ScreenshotsFeedWidget.h
        Headers/UI/Widgets/FilesFeedWidget.h
        Headers/UI/Graph/GraphControlPanel.h
        Headers/UI/Widgets/CodeEditorWidget.h
        Headers/UI/Widgets/ScriptsWidget.h
        Headers/Client/CodeEditorProfileManager.h
        Headers/Utils/CustomElements/ConnectionStatusWidget.h
        Headers/UI/Widgets/CredentialsWidget.h
        Headers/UI/Widgets/CredentialsFeedWidget.h
        Headers/UI/Widgets/CredentialWidgetIface.h
        Headers/UI/Widgets/TargetsWidget.h
        Headers/UI/Widgets/TargetsFeedWidget.h
        Headers/UI/Widgets/TargetWidgetIface.h
        Headers/UI/Dialogs/DialogConsoleSearch.h
        Headers/UI/Models/AgentsFilterProxyModel.h
        Headers/UI/Widgets/TaskOutputWidget.h
        Headers/UI/Dialogs/DialogImportCreds.h
        Headers/UI/Dialogs/DialogImportTargets.h
        Headers/Utils/Import/CredsImport.h
        Headers/Utils/Import/TargetsImport.h
        Headers/Utils/ChatLayoutHelpers.h
        Headers/Utils/CustomElements/ControlCard.h
        Headers/Utils/CustomElements/CardListWidget.h
        Headers/Client/AxScript/BridgeProcess.h
)

add_executable(AdaptixClient
        ${KONSOLE_SOURCES}
        ${KONSOLE_HEADERS}

        ${CODEEDITOR_SOURCES}

        ${KDDOCKWIDGETS_RESOURCES}

        ${HEADERS}

        Source/main.cpp

        Resources/Resources.qrc

        Source/Utils/Logs.cpp
        Source/UI/Dialogs/DialogConnect.cpp
        Source/UI/Dialogs/DialogSubscriptions.cpp
        Source/Utils/Convert.cpp
        Source/MainAdaptix.cpp
        Source/Client/Storage.cpp
        Source/Client/AuthProfile.cpp
        Source/Utils/FileSystem.cpp
        Source/Client/Requestor.cpp
        Source/Client/HttpRequestManager.cpp
        Source/UI/MainUI.cpp
        Source/UI/Widgets/AdaptixWidget.cpp
        Source/UI/Widgets/LogsWidget.cpp
        Source/Workers/WebSocketWorker.cpp
        Source/UI/Dialogs/DialogSyncPacket.cpp
        Source/Client/ProcessSyncPacket.cpp
        Source/UI/Dialogs/DialogListener.cpp
        Source/UI/Dialogs/DialogListenerConnector.cpp
        Source/UI/Dialogs/DialogAgent.cpp
        Source/UI/Dialogs/DialogAgentData.cpp
        Source/UI/Widgets/ConsoleWidget.cpp
        Source/Agent/Agent.cpp
        Source/Workers/LastTickWorker.cpp
        Source/Agent/Commander.cpp
        Source/UI/Widgets/BrowserFilesWidget.cpp
        Source/UI/Widgets/BrowserProcessWidget.cpp
        Source/UI/Dialogs/DialogSettings.cpp
        Source/Client/Extender.cpp
        Source/Client/CodeEditorProfileManager.cpp
        Source/Client/Settings.cpp
        Source/Client/ConsoleTheme.cpp
        Source/UI/Graph/SessionsGraph.cpp
        Source/UI/Graph/GraphItem.cpp
        Source/UI/Graph/GraphItemLink.cpp
        Source/UI/Graph/GraphScene.cpp
        Source/UI/Graph/LayoutTreeLeft.cpp
        Source/UI/Graph/LayoutTreeTop.cpp
        Source/UI/Dialogs/DialogTunnel.cpp
        Source/Client/TunnelEndpoint.cpp
        Source/Workers/TunnelWorker.cpp
        Source/Workers/SocksHandshakeWorker.cpp
        Source/UI/Widgets/TerminalContainerWidget.cpp
        Source/Workers/TerminalWorker.cpp
        Source/Workers/BuildWorker.cpp
        Source/UI/Dialogs/DialogDownloader.cpp
        Source/Workers/DownloaderWorker.cpp
        Source/Workers/UploaderWorker.cpp
        Source/UI/Dialogs/DialogUploader.cpp
        Source/Client/AxScript/AxScriptManager.cpp
        Source/Client/AxScript/AxScriptEngine.cpp
        Source/Client/AxScript/AxScriptWorker.cpp
        Source/Client/AxScript/AxUiFactory.cpp
        Source/Client/AxScript/BridgeApp.cpp
        Source/Client/AxScript/BridgeEvent.cpp
        Source/Client/AxScript/BridgeForm.cpp
        Source/Client/AxScript/BridgeMenu.cpp
        Source/Client/AxScript/AxCommandWrappers.cpp
        Source/Client/AxScript/AxElementWrappers.cpp
        Source/UI/Dialogs/DialogCredential.cpp
        Source/UI/Dialogs/DialogTarget.cpp
        Source/UI/Dialogs/DialogSaveTask.cpp
        Source/UI/Widgets/ChatWidget.cpp
        Source/Utils/NonBlockingDialogs.cpp
        Source/Utils/FontManager.cpp
        Source/Utils/TitleBarStyle.cpp
        Source/Client/FilterExprParser.cpp
        Source/Client/PagedTableHelper.cpp
        Source/UI/Models/GroupingProxyModel.cpp
        Source/Utils/CustomElements/BoldHeaderView.cpp
        Source/Utils/CustomElements/CardListWidget.cpp
        Source/Utils/CustomElements/GroupManagerPopup.cpp
        Source/Utils/CustomElements/PageNavBar.cpp
        Source/Utils/CustomElements/SearchPanel.cpp
        Source/Utils/CustomElements/SpinTable.cpp
        Source/Utils/CustomElements/ConsoleHighlighter.cpp
        Source/Utils/CustomElements/VerticalTabWidget.cpp
        Source/Utils/CustomElements/ListFeed.cpp
        Source/UI/Widgets/SessionsTableWidget.cpp
        Source/UI/Widgets/SessionsFeedWidget.cpp
        Source/UI/Widgets/TasksFeedWidget.cpp
        Source/UI/Widgets/ListenersFeedWidget.cpp
        Source/UI/Widgets/TunnelsFeedWidget.cpp
        Source/UI/Widgets/ScreenshotsFeedWidget.cpp
        Source/UI/Widgets/FilesFeedWidget.cpp
        Source/UI/Graph/GraphControlPanel.cpp
        Source/UI/Widgets/CodeEditorWidget.cpp
        Source/UI/Widgets/ScriptsWidget.cpp
        Source/Client/CodeEditorProfileManager.cpp
        Source/Utils/CustomElements/ConnectionStatusWidget.cpp
        Source/UI/Widgets/CredentialsWidget.cpp
        Source/UI/Widgets/CredentialsFeedWidget.cpp
        Source/UI/Widgets/TargetsWidget.cpp
        Source/UI/Widgets/TargetsFeedWidget.cpp
        Source/UI/Dialogs/DialogConsoleSearch.cpp
        Source/UI/Widgets/TaskOutputWidget.cpp
        Source/UI/Dialogs/DialogImportCreds.cpp
        Source/UI/Dialogs/DialogImportTargets.cpp
        Source/Utils/Import/CredsImport.cpp
        Source/Utils/Import/TargetsImport.cpp
        Source/Utils/CustomElements/ControlCard.cpp
        Source/Client/AxScript/BridgeProcess.cpp
)

target_compile_definitions(
        AdaptixClient
        PUBLIC KDDW_FRONTEND_QTWIDGETS KDDW_FRONTEND_QT BUILDING_DOCKS_LIBRARY
        PRIVATE QT_DEPRECATED_WARNINGS QT_NO_KEYWORDS
        #        QT_NO_SIGNALS_SLOTS_KEYWORDS
)

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

        kddockwidgets
        qlementine
)

if(WIN32)
    target_link_libraries( AdaptixClient
            PRIVATE
            wsock32
            ws2_32
            crypt32
            iphlpapi
            netapi32
            version
            winmm
            userenv
            dwmapi
    )
    target_link_options( AdaptixClient PRIVATE -static-libgcc -static-libstdc++ )
    target_link_libraries( AdaptixClient PRIVATE -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic )
elseif(UNIX)
    target_link_libraries( AdaptixClient
            PRIVATE
            pthread
            dl
    )
endif()