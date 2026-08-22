#ifndef ADAPTIXCLIENT_SCRIPTSDOCKWIDGET_H
#define ADAPTIXCLIENT_SCRIPTSDOCKWIDGET_H

#include <UI/Widgets/AbstractDock.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/PagedTableHelper.h>

#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

class QStackedWidget;
class AdaptixWidget;
class QComboBox;
class QTableView;
class QLineEdit;
class QLabel;
class QWidget;
class QPushButton;
class QAbstractItemModel;
class QModelIndex;
class QListWidget;
class QSplitter;
class PaginationBar;

class SegmentControl;

class ScriptsTableFilter : public QSortFilterProxyModel
{
Q_OBJECT
    QString m_searchText;
    QString m_originFilter; // empty = all, "local" | "server"
public:
    explicit ScriptsTableFilter(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}
    void setSearchText(const QString& text) {
        m_searchText = text;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        endFilterChange();
#else
        invalidateFilter();
#endif
    }
    void setOriginFilter(const QString& origin) {
        m_originFilter = origin;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        endFilterChange();
#else
        invalidateFilter();
#endif
    }
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

class ScriptsWidget : public DockTab
{
Q_OBJECT
    AdaptixWidget* adaptixWidget = nullptr;

    QWidget*            m_scriptsPanel = nullptr;
    QTableView*         m_scriptsTable = nullptr;
    QStandardItemModel* m_scriptsModel = nullptr;
    ScriptsTableFilter* m_scriptsFilter = nullptr;
    QLineEdit*          m_scriptsSearch = nullptr;
    QComboBox*          m_scriptsOriginCombo = nullptr;
    QLabel*             m_scriptsEmpty = nullptr;
    QPushButton*        m_scriptsAddBtn = nullptr;

    QWidget*            m_eventsPanel = nullptr;
    QSplitter*          m_eventsSplit = nullptr;
    QListWidget*        m_eventsTypeList = nullptr;
    QTableView*         m_eventsTable = nullptr;
    QStandardItemModel* m_eventsModel = nullptr;
    QLineEdit*          m_eventsSearch = nullptr;
    QComboBox*          m_eventsSourceCombo = nullptr;
    PaginationBar*      m_eventsPagination = nullptr;
    QLabel*             m_eventsEmpty = nullptr;
    QPushButton*        m_createHandlerBtn = nullptr;

    QList<EventHandlerInfo> m_eventHandlers;
    QSet<QString>           m_mutedEvents;
    QString                 m_selectedEventType; // empty = all

    PagedTableHelper* m_eventsPageHelper = nullptr;
    int               m_eventsOffset     = 0;
    int               m_eventsPageSize   = 50;
    int               m_eventsTotal      = 0;

    QStackedWidget* m_stack = nullptr;
    SegmentControl* m_segScripts = nullptr;
    SegmentControl* m_segEvents  = nullptr;
    int m_currentSegment = 0;

    void setupScriptsTable();
    void setupEventsTable();
    void setupEventsPagination();
    void loadEventsPage();
    void loadEventMutes();
    void rebuildEventTypeList();
    void applyEventsPage(const QJsonObject& response);
    void updateEventsPageChrome();
    void updateEmptyState(QLabel* empty, QAbstractItemModel* model) const;
    QString nextDefaultHandlerName() const;
    enum class EventHttpOp { Enable, Disable, Remove, Mute, Unmute };
    void runEventHttpBatch(const QStringList& keys, const QString& failTitle, EventHttpOp op);

    struct ScriptRowSel {
        QString kind; // "local" | "server"
        QString id;   // path or server name
    };
    QList<ScriptRowSel> selectedScriptRows() const;
    QStringList selectedIds(QTableView* table, int idColumn = 0) const;

public:
    explicit ScriptsWidget(AdaptixWidget* w);
    ~ScriptsWidget() override;

    void setSegment(int segment, const QString& originFilter = QString(), bool applyOriginFilter = false);

    void refreshScripts();
    void refreshEventHandlers();

private Q_SLOTS:
    void onScriptsMenu(const QPoint& pos);
    void onScriptsDoubleClicked(const QModelIndex& index);
    void onScriptsLoad();
    void onScriptsReload(const QList<ScriptRowSel>& rows);
    void onScriptsEnable(const QList<ScriptRowSel>& rows);
    void onScriptsDisable(const QList<ScriptRowSel>& rows);
    void onScriptsRemove(const QList<ScriptRowSel>& rows);
    void onEventsMenu(const QPoint& pos);
    void onEventEnable(const QStringList& ids);
    void onEventDisable(const QStringList& ids);
    void onEventRemove(const QStringList& ids);
    void onEventMute(const QStringList& events);
    void onEventUnmute(const QStringList& events);
    void onCreateEventHandler();
    void onEditEventHandler(const QString& id);
    void onEventsDoubleClicked(const QModelIndex& index);
    void onEventsPageReady(const QJsonObject& response);
    void onEventsPageError(const QString& message);
    void onEventsFilterChanged();
    void onEventTypeListCurrentChanged();
    void onEventTypeListMenu(const QPoint& pos);
    void onEventTypeListDoubleClicked();
};

#endif
