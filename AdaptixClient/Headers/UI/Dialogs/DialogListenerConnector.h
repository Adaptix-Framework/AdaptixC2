#ifndef ADAPTIXCLIENT_DIALOGLISTENERCONNECTOR_H
#define ADAPTIXCLIENT_DIALOGLISTENERCONNECTOR_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>

class AxContainerWrapper;

class DialogListenerConnector : public QDialog
{
Q_OBJECT
     QLabel*       labelListener      = nullptr;
     QLineEdit*    inputListenerName  = nullptr;
     QGroupBox*    connectorGroupbox  = nullptr;
     QPushButton*  buttonConnect      = nullptr;
     QPushButton*  buttonCancel       = nullptr;

     AdaptixWidget* adaptixWidget = nullptr;
     AuthProfile    authProfile;
     QString        listenerName;
     QString        listenerType;

     AxContainerWrapper*  container     = nullptr;
     QWidget*             panel         = nullptr;

     void createUI();

public:
     explicit DialogListenerConnector(AdaptixWidget* adaptixWidget, const QString &listenerName, const QString &listenerType);
     ~DialogListenerConnector() override;

     void SetProfile(const AuthProfile &profile);
     void SetConnectorUI(AxContainerWrapper* container, QWidget* panel, int height, int width);
     void Start();

protected Q_SLOTS:
     void onButtonConnect();
};

#endif
