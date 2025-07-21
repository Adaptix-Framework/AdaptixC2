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

    void createUI();
    bool filterItem(const CredentialData &credentials) const;
    void addTableItem(const CredentialData &newCredentials) const;

public:
    explicit CredentialsWidget(AdaptixWidget* w);
    ~CredentialsWidget() override;

    void Clear() const;
    void AddCredentialsItem(const CredentialData &newCredentials) const;
    void EditCredentialsItem(const CredentialData &newCredentials) const;
    void RemoveCredentialsItem(const QString &credId) const;

    void SetData() const;
    void ClearTableContent() const;

    void CredentialsAdd(const QString &username, const QString &password, const QString &realm, const QString &type, const QString &tag, const QString &storage, const QString &host);

public slots:
    void toggleSearchPanel() const;
    void onFilterUpdate() const;
    void handleCredentialsMenu( const QPoint &pos ) const;
    void onCreateCreds();
    void onEditCreds() const;
    void onRemoveCreds() const;
    void onExportCreds() const;
};

#endif