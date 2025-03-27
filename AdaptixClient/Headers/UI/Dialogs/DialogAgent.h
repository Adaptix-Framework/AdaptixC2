#ifndef ADAPTIXCLIENT_DIALOGAGENT_H
#define ADAPTIXCLIENT_DIALOGAGENT_H

#include <main.h>
#include <Client/WidgetBuilder.h>
#include <Client/AuthProfile.h>

class DialogAgent : public QDialog
{
    QGridLayout*    mainGridLayout      = nullptr;
    QGridLayout*    stackGridLayout     = nullptr;
    QHBoxLayout*    hLayoutBottom       = nullptr;
    QFrame*         line_1              = nullptr;
    QSpacerItem*    horizontalSpacer    = nullptr;
    QSpacerItem*    horizontalSpacer_2  = nullptr;
    QSpacerItem*    horizontalSpacer_3  = nullptr;
    QLabel*         listenerLabel       = nullptr;
    QLineEdit*      listenerInput       = nullptr;
    QLabel*         agentLabel          = nullptr;
    QComboBox*      agentCombobox       = nullptr;
    QPushButton*    buttonLoad          = nullptr;
    QPushButton*    buttonSave          = nullptr;
    QPushButton*    closeButton         = nullptr;
    QPushButton*    generateButton      = nullptr;
    QGroupBox*      agentConfigGroupbox = nullptr;
    QStackedWidget* configStackWidget   = nullptr;

    QMap<QString, WidgetBuilder*> agentsUI;

    AuthProfile authProfile;
    QString     listenerName;
    QString     listenerType;

    void createUI();

public:
    explicit DialogAgent(const QString &listenerName, const QString &listenerType);
    ~DialogAgent() override;

    void AddExAgents(const QMap<QString, WidgetBuilder*> &agents);
    void SetProfile(const AuthProfile &profile);
    void Start();

protected slots:
    void changeConfig(const QString &fn);
    void onButtonLoad();
    void onButtonSave();
    void onButtonGenerate();
    void onButtonClose();
};

#endif //ADAPTIXCLIENT_DIALOGAGENT_H
