#ifndef ADAPTIXCLIENT_LOGSWIDGET_H
#define ADAPTIXCLIENT_LOGSWIDGET_H

#include <main.h>
#include <Client/ConsoleTheme.h>
#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements/TextEditConsole.h>
#include <Utils/CustomElements/SearchPanel.h>

#include <oclero/qlementine/widgets/Switch.hpp>

#include <QLineEdit>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QSpinBox>
#include <QToolButton>
#include <QPointer>
#include <QMap>
#include <QSet>

class AdaptixWidget;

class LogsWidget : public DockTab
{
    QGridLayout*     mainGridLayout      = nullptr;
    QGridLayout*     logsGridLayout      = nullptr;
    TextEditConsole* logsConsoleTextEdit = nullptr;
    QSplitter*       mainHSplitter       = nullptr;
    QWidget*         logsWidget          = nullptr;

    QWidget*         serverLogsWidget    = nullptr;
    QGridLayout*     serverLogsLayout    = nullptr;
    QWidget*         serverLogsHost      = nullptr;
    TextEditConsole* serverLogsTextEdit  = nullptr;

    QFrame*                     historyBar         = nullptr;
    QLabel*                     historyStatusLabel = nullptr;
    oclero::qlementine::Switch* autoLoadSwitch     = nullptr;
    QLabel*                     pageSizeLabel      = nullptr;
    QSpinBox*                   pageSizeSpin       = nullptr;
    QToolButton*                loadEarlierButton  = nullptr;
    QToolButton*                loadAllButton      = nullptr;
    QToolButton*                jumpLatestButton   = nullptr;
    QToolButton*                stopLoadButton     = nullptr;

    int  serverPageSize    = 50;
    int  serverLoadedCount = 0;
    int  serverTotalKnown  = 0;
    bool serverHasMore     = true;
    bool serverLoadingPage = false;
    bool serverLoadAllPending = false;
    bool autoLoadEarlier   = true;

    bool                 serverLogsReady = false;
    int                  serverLogsEpoch = 0;
    QList<QJsonObject>   pendingServerLogs;
    QSet<qint64>         seenLogIds;
    qint64               oldestLoadedId  = 0;

    SearchPanel* searchPanel       = nullptr;
    SearchPanel* serverSearchPanel = nullptr;

    QComboBox* sourceCombo   = nullptr;
    QComboBox* categoryCombo = nullptr;
    QLineEdit* searchEdit    = nullptr;

    QString filterOrigin;
    QString filterCategory;
    QString filterContains;

    QMap<QString, QSet<QString>> originCategories;
    QSet<QString> knownOrigins;

    const AdaptixWidget* adaptixWidget = nullptr;

    void createUI();
    void applyTheme();
    ConsoleThemeData getActiveTheme() const;
    void appendServerLogEntry(qint64 id, qint64 time, int status, int level, const QString& source, const QString& category, const QString& message);
    bool matchesSourceFilter(const QString& source, const QString& category) const;
    void loadInitialServerPage();
    void loadMoreServerPage();
    void loadAllServerPages();
    void stopLoadAllServer();
    void jumpToLatestServer();
    void updateHistoryBar();
    void applyHistoryBarStyle();
    void applyHistoryBarMetrics();
    void positionHistoryBar();
    void positionSearchPanel();
    void positionServerOverlays();
    int  effectiveLoadLimit() const;
    void finishBulkLoad();

    void applyFiltersFromUi(bool reload);
    void rebuildSourceCombo();
    void rebuildCategoryCombo();
    void noteSource(const QString& source, const QString& category);
    void applyCatalogFromResponse(const QJsonObject& response);
    static QString displaySource(const QString& source, const QString& category);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

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
