#ifndef DIALOGIMPORTCREDS_H
#define DIALOGIMPORTCREDS_H

#include <main.h>
#include <Utils/Import/CredsImport.h>

#include <QPlainTextEdit>

class DialogImportCreds : public QDialog
{
Q_OBJECT
    QVBoxLayout*    mainLayout     = nullptr;
    QLabel*         helpLabel      = nullptr;
    QPlainTextEdit* textEdit       = nullptr;
    QHBoxLayout*    optionsLayout  = nullptr;
    QLabel*         tagLabel       = nullptr;
    QLineEdit*      tagInput       = nullptr;
    QLabel*         storageLabel   = nullptr;
    QComboBox*      storageCombo   = nullptr;
    QLabel*         statusLabel    = nullptr;
    QHBoxLayout*    buttonLayout   = nullptr;
    QPushButton*    loadFileButton = nullptr;
    QPushButton*    importButton   = nullptr;
    QPushButton*    cancelButton   = nullptr;

    QList<CredentialData> m_data;
    bool m_valid = false;

    void createUI();
    void updateStatus();

public:
    explicit DialogImportCreds(QWidget* parent = nullptr);
    ~DialogImportCreds() override;

    void StartDialog();
    bool IsValid() const { return m_valid; }
    QList<CredentialData> GetCreds() const { return m_data; }

private Q_SLOTS:
    void onLoadFile();
    void onImport();
    void onCancel();
    void onTextChanged();
};

#endif
