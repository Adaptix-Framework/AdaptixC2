#ifndef ADAPTIXCLIENT_DIALOGAGENT_H
#define ADAPTIXCLIENT_DIALOGAGENT_H

#include <main.h>
#include <Client/WidgetBuilder.h>
#include <Client/AuthProfile.h>

class DialogAgent : public QDialog
{
    QGridLayout*    mainGridLayout;
    QGridLayout*    stackGridLayout;
    QSpacerItem*    horizontalSpacer;
    QSpacerItem*    horizontalSpacer_2;
    QSpacerItem*    horizontalSpacer_3;
    QSpacerItem*    horizontalSpacer_4;
    QSpacerItem*    horizontalSpacer_5;
    QLabel*         listenerLabel;
    QLineEdit*      listenerInput;
    QLabel*         agentLabel;
    QComboBox*      agentCombobox;
    QPushButton*    closeButton;
    QPushButton*    generateButton;
    QGroupBox*      agentConfigGroupbox;
    QStackedWidget* configStackWidget;

    QMap<QString, WidgetBuilder*> agentsUI;
    AuthProfile                   authProfile;

    void createUI();

public:
    explicit DialogAgent();
    ~DialogAgent();

    void AddExAgents(QMap<QString, WidgetBuilder*> agents);
    void SetProfile(AuthProfile profile);
    void Start();

protected slots:
    void changeConfig(QString fn);
    void onButtonGenerate();
    void onButtonClose();
};

#endif //ADAPTIXCLIENT_DIALOGAGENT_H
