#ifndef ADAPTIXCLIENT_DIALOGAGENT_H
#define ADAPTIXCLIENT_DIALOGAGENT_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>

class AxContainerWrapper;

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

    AuthProfile authProfile;
    QString     listenerName;
    QString     listenerType;

    QStringList agents;
    QMap<QString, QWidget*> widgets;
    QMap<QString, AxContainerWrapper*> containers;

    void createUI();

public:
    explicit DialogAgent(const QString &listenerName, const QString &listenerType);
    ~DialogAgent() override;

    void AddExAgents(const QStringList &agents, const QMap<QString, QWidget*> &widgets, const QMap<QString, AxContainerWrapper*> &containers);
    void SetProfile(const AuthProfile &profile);
    void Start();

protected slots:
    void onButtonLoad();
    void onButtonSave();
    void changeConfig(const QString &agentName);
    void onButtonGenerate();
    void onButtonClose();
};

#endif
