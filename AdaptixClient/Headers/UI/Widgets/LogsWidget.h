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
#include <QPushButton>
#include <QPointer>
#include <QMap>
#include <QSet>
#include <QStackedWidget>

class AdaptixWidget;
class SegmentControl;

class LogsWidget : public DockTab
{
    enum Mode : int {
        ModeClient = 0,
        ModeServer = 1,
    };

    QGridLayout*     mainGridLayout      = nullptr;
    QWidget*         toolbarWidget       = nullptr;
    QHBoxLayout*     toolbarLayout       = nullptr;
    SegmentControl*  modeSegment         = nullptr;

    QStackedWidget*  contentStack        = nullptr;

    QGridLayout*     logsGridLayout      = nullptr;
    TextEditConsole* logsConsoleTextEdit = nullptr;
    QWidget*         logsWidget          = nullptr;

    QWidget*         serverLogsWidget    = nullptr;
    QGridLayout*     serverLogsLayout    = nullptr;
    QWidget*         serverLogsHost      = nullptr;
    TextEditConsole* serverLogsTextEdit  = nullptr;

    QWidget*                    serverFilterBar    = nullptr;
    QLabel*                     sourceLabel        = nullptr;
    QLabel*                     categoryLabel      = nullptr;

    QFrame*                     historyBar         = nullptr;
    QWidget*                    historyContent     = nullptr;
    QToolButton*                historyToggleBtn   = nullptr;
    QLabel*                     historyStatusLabel = nullptr;
    oclero::qlementine::Switch* autoLoadSwitch     = nullptr;
    QLabel*                     pageSizeLabel      = nullptr;
    QSpinBox*                   pageSizeSpin       = nullptr;
    QToolButton*                loadEarlierButton  = nullptr;
    QToolButton*                loadAllButton      = nullptr;
    QToolButton*                jumpLatestButton   = nullptr;
    QToolButton*                stopLoadButton     = nullptr;
    bool                        historyExpanded    = false;

    int  serverPageSize    = 50;
    int  serverLoadedCount = 0;
    int  serverTotalKnown  = 0;
    bool serverHasMore     = true;
    bool serverLoadingPage = false;
    bool serverLoadAllPending = false;
    bool autoLoadEarlier   = true;
    bool serverViewCleared = false;

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

    int  m_mode = ModeServer;
    int  m_clientUnread = 0;
    int  m_serverUnread = 0;

    const AdaptixWidget* adaptixWidget = nullptr;

    void createUI();
    void setMode(int mode);
    void updateModeChrome();
    void applyTheme();
    ConsoleThemeData getActiveTheme() const;
    void appendServerLogEntry(qint64 id, qint64 time, int status, int level, const QString& source, const QString& category, const QString& message);
    bool matchesSourceFilter(const QString& source, const QString& category) const;
    void loadInitialServerPage();
    void loadMoreServerPage();
    void loadAllServerPages();
    void stopLoadAllServer();
    void jumpToLatestServer();
    void reloadLatestServerPage();
    void clearServerLogsView();
    void updateHistoryBar();
    void applyHistoryBarStyle();
    void applyHistoryBarMetrics();
    void setHistoryBarExpanded(bool on);
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
    void Clear();
};

#endif
