#ifndef ADAPTIXCLIENT_CONSOLEWIDGET_H
#define ADAPTIXCLIENT_CONSOLEWIDGET_H

#include <main.h>
#include <Client/ConsoleTheme.h>
#include <UI/Widgets/AbstractDock.h>
#include <Utils/KeyPressHandler.h>
#include <Utils/CustomElements/SearchPanel.h>
#include <Utils/CustomElements/TextEditConsole.h>
#include <Agent/Commander.h>

#include <oclero/qlementine/widgets/Switch.hpp>

#include <QPointer>
#include <QToolButton>


class Agent;
class AdaptixWidget;
class DialogConsoleSearch;

#define CONSOLE_OUT_LOCAL         1
#define CONSOLE_OUT_LOCAL_INFO    2
#define CONSOLE_OUT_LOCAL_ERROR   3
#define CONSOLE_OUT_LOCAL_SUCCESS 4
#define CONSOLE_OUT_INFO          5
#define CONSOLE_OUT_ERROR         6
#define CONSOLE_OUT_SUCCESS       7
#define CONSOLE_OUT               10

class ConsoleWidget : public DockTab
{
    AdaptixWidget* adaptixWidget = nullptr;

    QGridLayout*      MainGridLayout   = nullptr;
    QLabel*           CmdLabel         = nullptr;
    QLabel*           InfoLabel        = nullptr;
    QLabel*           StatusLabel      = nullptr;
    QLineEdit*        InputLineEdit    = nullptr;
    TextEditConsole*  OutputTextEdit   = nullptr;
    QCompleter*       CommandCompleter = nullptr;
    QStringListModel* completerModel   = nullptr;

    SearchPanel* searchPanel = nullptr;

    QWidget*                    consoleHost        = nullptr;
    QFrame*                     historyBar         = nullptr;
    QLabel*                     historyStatusLabel = nullptr;
    oclero::qlementine::Switch* autoLoadSwitch     = nullptr;
    QLabel*                     pageSizeLabel      = nullptr;
    QSpinBox*                   pageSizeSpin       = nullptr;
    QToolButton*                loadEarlierButton  = nullptr;
    QToolButton*                loadAllButton      = nullptr;
    QToolButton*                jumpLatestButton   = nullptr;
    QToolButton*                stopLoadButton     = nullptr;

    int  pageSize        = 50;
    int  loadedItemCount = 0;
    int  totalKnown      = 0;
    bool hasMore         = true;
    bool loadingPage     = false;
    bool initialLoaded   = false;
    bool loadAllPending  = false;
    bool autoLoadEarlier = true;
    qint64 oldestLoadedId = 0;

    QPointer<DialogConsoleSearch> searchDialog;

    void loadInitialPage();
    void loadMorePage();
    void loadAllPages();
    void stopLoadAll();
    void jumpToLatest();
    void loadAroundHit(qint64 centerId, int limit = 0);
    void applyConsolePacket(const QJsonObject& obj);
    void updateHistoryBar();
    void applyPageItems(const QJsonArray& items, bool prepend);
    void openHistorySearch();
    void finishBulkLoad();
    int  effectiveLoadLimit() const;

    bool userSelectedCompletion = false;

    Agent*     agent     = nullptr;
    Commander* commander = nullptr;
    KPH_ConsoleInput* kphInputLineEdit = nullptr;

    void createUI();
    void applyTheme();
    void applyHistoryBarStyle();
    void applyHistoryBarMetrics();
    void positionHistoryBar();
    void positionSearchPanel();
    void positionConsoleOverlays();
    ConsoleThemeData getActiveTheme() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

    void cleanupHooksOnError(const QString& hookId, const QString& handlerId, bool hasHook, bool hasHandler);
    void processFileUploads(const QList<QPair<QString, QString>>& fileTasks, int index, QJsonObject data, const QString& commandLine, bool UI, const QString& hookId, const QString& handlerId, bool hasHook, bool hasHandler);

public:
    explicit ConsoleWidget(AdaptixWidget* w, Agent* a, Commander* c);
    ~ConsoleWidget() override;

    void clearAgent() { agent = nullptr; }

    void SetCommander(Commander* c);

    void SetUpdatesEnabled(const bool enabled);

    void ProcessCmdResult(const QString &commandLine, const CommanderResult &cmdResult, bool UI);

    void InputFocus() const;
    void LoadInitialPage();
    void AddToHistory(const QString& command);
    void SetInput(const QString &command);
    void Clear();
    void UpdateInfoLabel();
    void UpdateStatusLabel();

    void ApplyConsolePrefs();

    void ConsoleOutputMessage(qint64 timestamp, const QString &taskId, int type, const QString &message, const QString &text, bool completed);
    void ConsoleOutputPrompt(qint64 timestamp, const QString &taskId, const QString &user, const QString &commandLine) const;

public Q_SLOTS:
    void upgradeCompleter() const;
    void processInput();
    void onCompletionSelected(const QString &selectedText);
    void handleShowHistory();
};

#endif
