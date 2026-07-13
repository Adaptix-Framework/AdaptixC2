#ifndef ADAPTIXCLIENT_LOGSWIDGET_H
#define ADAPTIXCLIENT_LOGSWIDGET_H

#include <main.h>
#include <Client/ConsoleTheme.h>
#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements/TextEditConsole.h>
#include <Utils/CustomElements/ClickableLabel.h>
#include <Utils/CustomElements/SearchPanel.h>

class AdaptixWidget;

class LogsWidget : public DockTab
{
    QGridLayout*     mainGridLayout      = nullptr;
    QGridLayout*     logsGridLayout      = nullptr;
    TextEditConsole* logsConsoleTextEdit = nullptr;
    QLabel*          logsLabel           = nullptr;
    QSplitter*       mainHSplitter       = nullptr;
    QWidget*         logsWidget          = nullptr;
    QGridLayout*     serverLogsLayout    = nullptr;
    TextEditConsole* serverLogsTextEdit  = nullptr;
    QWidget*         serverLogsWidget    = nullptr;

    ClickableLabel*      loadEarlierButton = nullptr;
    int                  serverPageSize    = 50;
    int                  serverLoadedCount = 0;
    int                  serverTotalKnown  = 0;
    bool                 serverLoadingPage = false;

    bool                 serverLogsReady = false;
    int                  serverLogsEpoch = 0;
    QList<QJsonObject>   pendingServerLogs;
    QSet<qint64>         seenLogIds;
    qint64               oldestLoadedId  = 0;

    SearchPanel* searchPanel       = nullptr;
    SearchPanel* serverSearchPanel = nullptr;

    const AdaptixWidget* adaptixWidget = nullptr;

    void createUI();
    void applyTheme();
    ConsoleThemeData getActiveTheme() const;
    void appendServerLogEntry(qint64 id, qint64 time, int status, int level, const QString& source, const QString& message);
    void loadInitialServerPage();
    void loadMoreServerPage();
    void updateLoadEarlierVisibility();

public:
    explicit LogsWidget(const AdaptixWidget* w);
    ~LogsWidget() override;

    void SetUpdatesEnabled(bool enabled);

    void AddLogs(int type, qint64 time, const QString &Message);
    void AddServerLogBatch(const QJsonArray& items);
    void ReloadServerLogs();
    void ResetServerLogs();
    void Clear() const;
};

#endif
