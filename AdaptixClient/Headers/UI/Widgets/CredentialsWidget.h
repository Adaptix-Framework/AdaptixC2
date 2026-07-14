#ifndef CREDENTIALSWIDGET_H
#define CREDENTIALSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <UI/Widgets/CredentialWidgetIface.h>
#include <Client/PagedTableHelper.h>
#include <Utils/CustomElements/ClickableLabel.h>
#include <Utils/CustomElements/Delegates.h>
#include <Utils/CustomElements/PageNavBar.h>
#include <Utils/CustomElements/SearchPanel.h>

#include <QSortFilterProxyModel>

class AdaptixWidget;

enum CredsColumns {
    CC_Id,
    CC_Username,
    CC_Password,
    CC_Realm,
    CC_Type,
    CC_Tag,
    CC_Date,
    CC_Storage,
    CC_Agent,
    CC_Host,
    CC_ColumnCount
};



static QString sortKeyForCredSection(int section)
{
    switch (section) {
        case CC_Id:       return QStringLiteral("CredId");
        case CC_Username: return QStringLiteral("Username");
        case CC_Password: return QStringLiteral("Password");
        case CC_Realm:    return QStringLiteral("Realm");
        case CC_Type:     return QStringLiteral("Type");
        case CC_Tag:      return QStringLiteral("Tag");
        case CC_Date:     return QStringLiteral("Date");
        case CC_Storage:  return QStringLiteral("Storage");
        case CC_Agent:    return QStringLiteral("AgentId");
        case CC_Host:     return QStringLiteral("Host");
        default:          return {};
    }
}

static void sortCredsInPlace(QList<CredentialData>& creds, const QString& col, bool desc)
{
    auto cmp = [&](const CredentialData& a, const CredentialData& b) -> bool {
        int c = 0;
        if      (col == "CredId")   c = (a.CredId < b.CredId) ? -1 : (a.CredId > b.CredId ? 1 : 0);
        else if (col == "Username") c = QString::compare(a.Username, b.Username, Qt::CaseInsensitive);
        else if (col == "Password") c = QString::compare(a.Password, b.Password, Qt::CaseInsensitive);
        else if (col == "Realm")    c = QString::compare(a.Realm,    b.Realm,    Qt::CaseInsensitive);
        else if (col == "Type")     c = QString::compare(a.Type,     b.Type,     Qt::CaseInsensitive);
        else if (col == "Tag")      c = QString::compare(a.Tag,      b.Tag,      Qt::CaseInsensitive);
        else if (col == "Date")     c = (a.DateTimestamp < b.DateTimestamp) ? -1 : (a.DateTimestamp > b.DateTimestamp ? 1 : 0);
        else if (col == "Storage")  c = QString::compare(a.Storage,  b.Storage,  Qt::CaseInsensitive);
        else if (col == "AgentId")  c = (a.AgentId < b.AgentId) ? -1 : (a.AgentId > b.AgentId ? 1 : 0);
        else if (col == "Host")     c = QString::compare(a.Host,     b.Host,     Qt::CaseInsensitive);
        else                        c = (a.DateTimestamp < b.DateTimestamp) ? -1 : (a.DateTimestamp > b.DateTimestamp ? 1 : 0);
        return desc ? c > 0 : c < 0;
    };
    std::sort(creds.begin(), creds.end(), cmp);
}



class CredsTableModel : public QAbstractTableModel
{
Q_OBJECT
    QVector<CredentialData> creds;
    QHash<qint64, int>      idToRow;

    void rebuildIndex() {
        idToRow.clear();
        for (int i = 0; i < creds.size(); ++i)
            idToRow[creds[i].CredId] = i;
    }

public:
    explicit CredsTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex&) const override { return creds.size(); }
    int columnCount(const QModelIndex&) const override { return CC_ColumnCount; }

    bool containsId(qint64 id) const { return idToRow.contains(id); }

    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;

    void add(const CredentialData& item);
    void add(const QList<CredentialData>& list);
    void update(qint64 credId, const CredentialData& newCred);
    void remove(const QList<qint64>& credIds);
    void setTag(const QList<qint64> &credIds, const QString &tag);
    void clear();
    void reset(const QList<CredentialData>& newCreds);
};



class CredentialsWidget : public DockTab, public CredentialWidgetIface
{
Q_OBJECT
    AdaptixWidget* adaptixWidget  = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QTableView*    tableView      = nullptr;
    QShortcut*     shortcutSearch = nullptr;

    CredsTableModel*       credsModel = nullptr;
    QSortFilterProxyModel* proxyModel = nullptr;

    PagedTableHelper* pageHelper = nullptr;
    PageNavBar*       pageNavBar = nullptr;
    int               m_offset   = 0;
    QString           m_sortCol   = "Date";
    QString           m_sortOrder = "desc";

    bool bufferingEnabled = false;
    bool cachePrimed      = false;

    void createUI();
    void loadCurrentPage();
    void renderFromCache();
    void updateNavBar(int total);
    bool shouldUseCache() const;

public:
    explicit CredentialsWidget(AdaptixWidget* w);
    ~CredentialsWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock() override { return DockTab::dock(); }

    void SetUpdatesEnabled(const bool enabled) override;
    void Clear() override;

    void AddCredentialsItems(QList<CredentialData> credsList) override;
    void EditCredentialsItem(const CredentialData &newCredentials) override;
    void RemoveCredentialsItem(const QList<qint64> &credsId) override;
    void CredsSetTag(const QList<qint64> &credsIds, const QString &tag) override;

    void UpdateColumnsSize() const override;
    void UpdateColumnsVisible() override;
    void UpdateFilterComboBoxes() const override;

    void CredentialsAdd(QList<CredentialData> credsList) override;

    QWidget* asWidget() override { return this; }

public Q_SLOTS:
    void handleCredentialsMenu( const QPoint &pos ) const;
    void onCreateCreds();
    void onEditCreds();
    void onRemoveCreds() const;
    void onSetTag() const;
    void onExportCreds() const;
    void onCopyToClipboard() const;

private Q_SLOTS:
    void onPageReady(const QJsonObject& response);
    void onPageError(const QString& message);
};

#endif
