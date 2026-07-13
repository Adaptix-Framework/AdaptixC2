#ifndef ADAPTIXCLIENT_CREDENTIALSFEEDWIDGET_H
#define ADAPTIXCLIENT_CREDENTIALSFEEDWIDGET_H

#include <Utils/CustomElements/ListFeed.h>
#include <UI/Widgets/AbstractDock.h>
#include <Client/PagedTableHelper.h>

#include <main.h>

class AdaptixWidget;

class CredentialsFeedWidget : public ListFeedWidget
{
Q_OBJECT
    AdaptixWidget* m_adaptixWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* dockWidget = nullptr;

    FeedListModel*    feedBlockModel = nullptr;
    PagedTableHelper* pageHelper     = nullptr;
    int               m_offset       = 0;
    QString           m_sortCol      = "Date";
    QString           m_sortOrder    = "desc";
    bool              cachePrimed    = false;
    QHash<qint64, CredentialData> m_credCache;

    void setupPagination();
    void loadCurrentPage();

    struct CredInfo { qint64 credId = 0; QString realm; QString username; QString password; bool valid = false; };
    CredInfo currentCredInfo() const;

public:
    explicit CredentialsFeedWidget(AdaptixWidget* w);
    ~CredentialsFeedWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock();

    void SetUpdatesEnabled(bool enabled);
    void Clear();

    void AddCredentialsItems(QList<CredentialData> credsList);
    void EditCredentialsItem(const CredentialData& newCredentials);
    void RemoveCredentialsItem(const QList<qint64>& credsId);
    void CredsSetTag(const QList<qint64>& credsIds, const QString& tag);

    void CredentialsAdd(QList<CredentialData> credsList);

    void UpdateColumnsSize() const {}
    void UpdateColumnsVisible();
    void UpdateFilterComboBoxes() const {}


protected:
    void onFilterChanged() override;
    void onSortingChanged(int index) override;

public Q_SLOTS:
    void handleFeedMenu(const QPoint& pos);
    void onItemDoubleClicked(const QModelIndex& index);
    void onCreateCreds();
    void onEditCreds();
    void onRemoveCreds();
    void onSetTag();
    void onExportCreds();
    void onCopyToClipboard();

private Q_SLOTS:
    void onPageReady(const QJsonObject& response);
    void onPageError(const QString& message);
};

#endif
