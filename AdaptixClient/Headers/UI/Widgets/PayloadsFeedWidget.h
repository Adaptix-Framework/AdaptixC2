#ifndef ADAPTIXCLIENT_PAYLOADSFEEDWIDGET_H
#define ADAPTIXCLIENT_PAYLOADSFEEDWIDGET_H

#include <UI/Widgets/AbstractDock.h>
#include <Client/PagedTableHelper.h>
#include <main.h>

#include <QHash>
#include <QStandardItemModel>
#include <QHBoxLayout>
#include <QTableView>
#include <QLineEdit>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include <QStackedWidget>

class AdaptixWidget;
class PaginationBar;

class PayloadsFeedWidget : public QWidget
{
Q_OBJECT
    AdaptixWidget* m_adaptixWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* dockWidget = nullptr;

    QTableView*         table            = nullptr;
    QStandardItemModel* model            = nullptr;
    QWidget*            toolbarWidget    = nullptr;
    QHBoxLayout*        toolbarLayout    = nullptr;
    QLineEdit*          searchEdit       = nullptr;
    QToolButton*        showHiddenButton = nullptr;
    QPushButton*        generateButton   = nullptr;
    QPushButton*        uploadButton     = nullptr;
    QLabel*             emptyLabel       = nullptr;
    QStackedWidget*     contentStack     = nullptr;
    PaginationBar*      pagination       = nullptr;
    PagedTableHelper*   pageHelper       = nullptr;

    QHash<qint64, PayloadData> m_items;
    int  m_offset = 0;
    bool m_cachePrimed = false;
    QString m_sortCol = QStringLiteral("Created");
    QString m_sortOrder = QStringLiteral("desc");

    enum Col {
        ColId = 0,
        ColName,
        ColDescription,
        ColType,
        ColArtifact,
        ColListeners,
        ColSize,
        ColCreator,
        ColCreated,
        ColUid,
        ColTag,
        ColMd5,
        ColSha1,
        ColSha256,
        ColCount
    };

    void setupPagination();
    void loadCurrentPage();
    void applyPage(const QJsonObject& response);
    void updateEmptyState();
    void fitColumns();
    void updatePaginationChrome(int shown, int total);
    QList<qint64> selectedIds() const;
    PayloadData* findById(qint64 id);
    bool showHidden() const;
    void openPayloadDialog(qint64 id, bool configTab = false);
    static QString formatSize(qint64 bytes);
    static QString formatArtifact(const QString& artifact, const QString& arch);
    static QString truncHash(const QString& h, int n = 12);
    static PayloadData parsePayloadObject(const QJsonObject& o);
    static QList<QStandardItem*> makeRow(const PayloadData& p);

public:
    explicit PayloadsFeedWidget(AdaptixWidget* w);
    ~PayloadsFeedWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock();

    void SetUpdatesEnabled(bool enabled);
    void Clear();
    void AddPayloadItem(const PayloadData& p);
    void UpdatePayloadItem(const PayloadData& p);
    void UpdatePayloadHidden(const QList<qint64>& ids, bool hidden);
    void UpdatePayloadTag(const QList<qint64>& ids, const QString& tag);
    void RemovePayloadItems(const QList<qint64>& ids);
    void UpdateColumnsVisible();

public Q_SLOTS:
    void actionGenerate();
    void actionImport();
    void actionRemove();
    void actionDownload();
    void actionCopyHashes();
    void actionCopyHash(const QString& which);
    void actionHide();
    void actionUnhide();
    void actionOpenDetails();
    void actionHardDelete();
    void actionItemColor();
    void actionTextColor();
    void actionColorReset();
    void actionSetTag();
    void handleContextMenu(const QPoint& pos);
    void onGenerateFromToolbar();

private Q_SLOTS:
    void onSearchChanged(const QString&);
    void onShowHiddenToggled(bool);
    void onPageReady(const QJsonObject& response);
    void onPageError(const QString& message);
    void onRowDoubleClicked(const QModelIndex& index);
};

#endif
