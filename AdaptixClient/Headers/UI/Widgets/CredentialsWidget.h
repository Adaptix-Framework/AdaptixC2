#ifndef CREDENTIALSWIDGET_H
#define CREDENTIALSWIDGET_H

#include <main.h>

class AdaptixWidget;
class ClickableLabel;

class CredentialsWidget : public QWidget
{
    AdaptixWidget* adaptixWidget  = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QTableWidget*  tableWidget    = nullptr;
    QShortcut*     shortcutSearch = nullptr;

    QWidget*        searchWidget    = nullptr;
    QHBoxLayout*    searchLayout    = nullptr;
    QLineEdit*      inputFilter     = nullptr;
    ClickableLabel* hideButton      = nullptr;

    int ColumnId       = 0;
    int ColumnUsername = 1;
    int ColumnPassword = 2;
    int ColumnRealm    = 3;
    int ColumnType     = 4;
    int ColumnTag      = 5;
    int ColumnDate     = 6;
    int ColumnStorage  = 7;
    int ColumnAgent    = 8;
    int ColumnHost     = 9;

    void createUI();
    bool filterItem(const CredentialData &credentials) const;
    void addTableItem(const CredentialData &newCredentials) const;

public:
    explicit CredentialsWidget(AdaptixWidget* w);
    ~CredentialsWidget() override;

    void Clear() const;
    void AddCredentialsItems(QList<CredentialData> credsList) const;
    void EditCredentialsItem(const CredentialData &newCredentials) const;
    void RemoveCredentialsItem(const QStringList &credsId) const;
    void CredsSetTag(const QStringList &credsIds, const QString &tag) const;

    void SetData() const;
    void ClearTableContent() const;

    void CredentialsAdd(QList<CredentialData> credsList);

public slots:
    void toggleSearchPanel() const;
    void onFilterUpdate() const;
    void handleCredentialsMenu( const QPoint &pos ) const;
    void onCreateCreds();
    void onEditCreds() const;
    void onRemoveCreds() const;
    void onSetTag() const;
    void onExportCreds() const;
};

#endif