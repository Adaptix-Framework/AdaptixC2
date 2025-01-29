#ifndef ADAPTIXCLIENT_DIALOGAGENT_H
#define ADAPTIXCLIENT_DIALOGAGENT_H

#include <main.h>
#include <Client/WidgetBuilder.h>
#include <Client/AuthProfile.h>

class DialogAgent : public QDialog
{
    QGridLayout*    mainGridLayout;
    QGridLayout*    stackGridLayout;
    QHBoxLayout*    hLayoutBottom;
    QFrame*         line_1;
    QSpacerItem*    horizontalSpacer;
    QSpacerItem*    horizontalSpacer_2;
    QSpacerItem*    horizontalSpacer_3;
    QLabel*         listenerLabel;
    QLineEdit*      listenerInput;
    QLabel*         agentLabel;
    QComboBox*      agentCombobox;
    QPushButton*    buttonLoad;
    QPushButton*    buttonSave;
    QPushButton*    closeButton;
    QPushButton*    generateButton;
    QGroupBox*      agentConfigGroupbox;
    QStackedWidget* configStackWidget;

    QMap<QString, WidgetBuilder*> agentsUI;

    AuthProfile authProfile;
    QString     listenerName;
    QString     listenerType;

    void createUI();

public:
    explicit DialogAgent(QString listenerName, QString listenerType);
    ~DialogAgent();

    void AddExAgents(QMap<QString, WidgetBuilder*> agents);
    void SetProfile(AuthProfile profile);
    void Start();

protected slots:
    void changeConfig(QString fn);
    void onButtonLoad();
    void onButtonSave();
    void onButtonGenerate();
    void onButtonClose();
};

#endif //ADAPTIXCLIENT_DIALOGAGENT_H
