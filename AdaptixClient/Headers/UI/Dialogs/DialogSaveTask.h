#ifndef ADAPTIXCLIENT_DIALOGSAVETASK_H
#define ADAPTIXCLIENT_DIALOGSAVETASK_H

#include <main.h>

class DialogSaveTask : public QDialog
{
    QGridLayout* mainGridLayout   = nullptr;
    QLabel*      commandLineLabel = nullptr;
    QLineEdit*   commandLineInput = nullptr;
    QLabel*      messageLabel     = nullptr;
    QComboBox*   messageCombo     = nullptr;
    QLineEdit*   messageInput     = nullptr;
    QLabel*      textLabel        = nullptr;
    QTextEdit*   textEdit         = nullptr;
    QHBoxLayout* hLayoutBottom    = nullptr;
    QSpacerItem* spacer_1         = nullptr;
    QSpacerItem* spacer_2         = nullptr;
    QPushButton* createButton     = nullptr;
    QPushButton* cancelButton     = nullptr;

    bool    valid   = false;
    QString message = "";
    QString credsId = "";
    TaskData data   = {};

    void createUI();

public:
    explicit DialogSaveTask();
    ~DialogSaveTask() override;

    void     StartDialog(const QString &text);
    bool     IsValid() const;
    QString  GetMessage() const;
    TaskData GetData() const;

protected slots:
    void onButtonSave();
    void onButtonCancel();
};

#endif //ADAPTIXCLIENT_DIALOGSAVETASK_H