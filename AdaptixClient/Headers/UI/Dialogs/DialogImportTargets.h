#ifndef DIALOGIMPORTTARGETS_H
#define DIALOGIMPORTTARGETS_H

#include <main.h>
#include <Utils/Import/TargetsImport.h>

#include <QPlainTextEdit>

class DialogImportTargets : public QDialog
{
Q_OBJECT
    QVBoxLayout*    mainLayout     = nullptr;
    QLabel*         helpLabel      = nullptr;
    QPlainTextEdit* textEdit       = nullptr;
    QHBoxLayout*    optionsLayout  = nullptr;
    QLabel*         tagLabel       = nullptr;
    QLineEdit*      tagInput       = nullptr;
    QLabel*         aliveLabel     = nullptr;
    QCheckBox*      aliveCheck     = nullptr;
    QLabel*         statusLabel    = nullptr;
    QHBoxLayout*    buttonLayout   = nullptr;
    QPushButton*    loadFileButton = nullptr;
    QPushButton*    importButton   = nullptr;
    QPushButton*    cancelButton   = nullptr;

    QList<TargetData> m_data;
    bool m_valid = false;

    void createUI();
    void updateStatus();

public:
    explicit DialogImportTargets(QWidget* parent = nullptr);
    ~DialogImportTargets() override;

    void StartDialog();
    bool IsValid() const { return m_valid; }
    QList<TargetData> GetTargets() const { return m_data; }

private Q_SLOTS:
    void onLoadFile();
    void onImport();
    void onCancel();
    void onTextChanged();
};

#endif
