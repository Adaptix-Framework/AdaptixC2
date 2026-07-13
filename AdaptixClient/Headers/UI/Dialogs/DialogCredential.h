#ifndef DIALOGCREDENTIAL_H
#define DIALOGCREDENTIAL_H

#include <main.h>

class DialogCredential : public QDialog
{
    QVBoxLayout* mainLayout     = nullptr;
    QGroupBox*   credGroup      = nullptr;
    QGridLayout* credGrid       = nullptr;
    QGroupBox*   sourceGroup    = nullptr;
    QGridLayout* sourceGrid     = nullptr;
    QHBoxLayout* buttonLayout   = nullptr;
    QPushButton* createButton   = nullptr;
    QPushButton* cancelButton   = nullptr;

    QLineEdit*   usernameInput  = nullptr;
    QLineEdit*   passwordInput  = nullptr;
    QLineEdit*   realmInput     = nullptr;
    QComboBox*   typeCombo      = nullptr;
    QComboBox*   storageCombo   = nullptr;
    QLineEdit*   hostInput      = nullptr;
    QLineEdit*   tagInput       = nullptr;

    bool       valid    = false;
    QString    message  = "";
    qint64     credsId  = 0;
    CredentialData data = {};

    bool editMode = false;

    void createUI();

public:
    explicit DialogCredential();
    ~DialogCredential() override;

    void StartDialog();
    void SetEditmode(const CredentialData &credentialData);
    bool IsValid() const;
    QString GetMessage() const;
    CredentialData GetCredData() const;

protected Q_SLOTS:
    void onButtonCreate();
    void onButtonCancel();
};

#endif
