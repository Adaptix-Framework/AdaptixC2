#ifndef FILESFEEDWIDGET_H
#define FILESFEEDWIDGET_H

#include <Utils/CustomElements/ListFeed.h>
#include <UI/Widgets/AbstractDock.h>
#include <Client/PagedTableHelper.h>

#include <main.h>

#include <QStackedWidget>

class AdaptixWidget;
class SegmentControl;

enum FilesFeedBlock {
    FFB_Id = 0,
    FFB_Main = 1,
    FFB_Info = 2,
    FFB_Tags = 3,
    FFB_Right = 4,
    FFB_Count = 5
};

class FilesFeedWidget : public QWidget
{
Q_OBJECT
    AdaptixWidget*  m_adaptixWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* m_dockWidget = nullptr;

    ListFeedWidget* dlFeed   = nullptr;
    ListFeedWidget* ulFeed   = nullptr;
    ListFeedWidget* syncFeed = nullptr;

    FeedListModel*  dlModel   = nullptr;
    FeedListModel*  ulModel   = nullptr;
    FeedListModel*  syncModel = nullptr;

    PagedTableHelper* pageHelperDl = nullptr;
    PagedTableHelper* pageHelperUl = nullptr;

    int     m_dlOffset    = 0;
    QString m_dlSortCol   = "Date";
    QString m_dlSortOrder = "desc";

    int     m_ulOffset    = 0;
    QString m_ulSortCol   = "Date";
    QString m_ulSortOrder = "desc";

    int m_currentSegment = 0;

    SegmentControl* m_segDl   = nullptr;
    SegmentControl* m_segUl   = nullptr;
    SegmentControl* m_segSync = nullptr;

    QStackedWidget* m_stack = nullptr;

    QHash<qint64, TransferData> m_dlCache;
    QHash<qint64, TransferData> m_ulCache;
    QHash<QString, SyncEntryData> m_syncCache;

    ListFeedWidget* activeFeed() const;
    FeedListModel*  activeModel() const;
    void loadCurrentPage();

    SegmentControl* createModeSegment(QWidget* parent, int selectedIdx);

public:
    explicit FilesFeedWidget(AdaptixWidget* w);
    ~FilesFeedWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock();

    void setSegment(int index);
    int currentSegment() const { return m_currentSegment; }

    void SetUpdatesEnabled(bool enabled);
    void Clear();
    void UpdateColumnsVisible();
    void setCompactMode(bool compact);

    void AddTransferItem(const TransferData& transfer);
    void EditTransferItem(int transferType, qint64 fileId, qint64 progress, int state);
    void RemoveTransferItem(int transferType, const QList<qint64>& filesId);
    void SetTransferTag(int transferType, const QList<qint64>& filesId, const QString& tag);

    void AddSyncEntry(const SyncEntryData& entry);
    void UpdateSyncEntry(const QString& id, qint64 progress, qint64 totalSize, double speed);
    void FinishSyncEntry(const QString& id, int state);

    void UpdateFilterComboBoxes() const;

    qint64 getSelectedFileId() const;
    QList<const TransferData*> getSelectedDownloads() const;
    QList<const TransferData*> getSelectedUploads() const;
    bool modelContainsTransfer(int transferType, qint64 fileId) const;

public Q_SLOTS:
    void handleFeedMenu(const QPoint& pos);
    void handleSyncMenu(const QPoint& pos);
    void handleDownloadsMenu(const QPoint& pos);
    void handleUploadsMenu(const QPoint& pos);
    void actionSync();
    void actionSyncMultiple();
    void actionSyncCurl();
    void actionSyncWget();
    void actionDelete();
    void actionDeleteUploads();
    void actionSetTag();

private Q_SLOTS:
    void onDlPageReady(const QJsonObject& response);
    void onDlPageError(const QString& message);
    void onUlPageReady(const QJsonObject& response);
    void onUlPageError(const QString& message);
};

#endif
