#ifndef DIALOGCREDENTIAL_H
#define DIALOGCREDENTIAL_H

#include <main.h>

class DialogCredential : public QDialog
{
    QGridLayout* mainGridLayout = nullptr;
    QLabel*      usernameLabel  = nullptr;
    QLineEdit*   usernameInput  = nullptr;
    QLabel*      passwordLabel  = nullptr;
    QLineEdit*   passwordInput  = nullptr;
    QLabel*      realmLabel     = nullptr;
    QLineEdit*   realmInput     = nullptr;
    QLabel*      typeLabel      = nullptr;
    QComboBox*   typeCombo      = nullptr;
    QLabel*      tagLabel       = nullptr;
    QLineEdit*   tagInput       = nullptr;
    QLabel*      storageLabel   = nullptr;
    QComboBox*   storageCombo   = nullptr;
    QLabel*      hostLabel      = nullptr;
    QLineEdit*   hostInput      = nullptr;
    QHBoxLayout* hLayoutBottom  = nullptr;
    QSpacerItem* spacer_1       = nullptr;
    QSpacerItem* spacer_2       = nullptr;
    QPushButton* createButton   = nullptr;
    QPushButton* cancelButton   = nullptr;

    bool       valid    = false;
    QString    message  = "";
    QString    credsId  = "";
    CredentialData data = {};

    void createUI();

public:
    explicit DialogCredential();
    ~DialogCredential() override;

    void StartDialog();
    void SetEditmode(const CredentialData &credentialData);
    bool IsValid() const;
    QString GetMessage() const;
    CredentialData GetCredData() const;

protected slots:
    void onButtonCreate();
    void onButtonCancel();
};

#endif
