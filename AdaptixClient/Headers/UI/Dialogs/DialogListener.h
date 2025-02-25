#ifndef ADAPTIXCLIENT_DIALOGLISTENER_H
#define ADAPTIXCLIENT_DIALOGLISTENER_H

#include <main.h>
#include <Client/WidgetBuilder.h>
#include <Client/AuthProfile.h>

class DialogListener : public QDialog
{
    QGridLayout*    mainGridLayout;
    QGridLayout*    stackGridLayout;
    QSpacerItem*    horizontalSpacer;
    QSpacerItem*    horizontalSpacer_2;
    QSpacerItem*    horizontalSpacer_3;
    QHBoxLayout*    hLayoutBottom;
    QFrame*         line_1;
    QLabel*         listenerNameLabel;
    QLineEdit*      inputListenerName;
    QLabel*         listenerTypeLabel;
    QComboBox*      listenerTypeCombobox;
    QPushButton*    buttonLoad;
    QPushButton*    buttonSave;
    QPushButton*    buttonCreate;
    QPushButton*    buttonCancel;
    QGroupBox*      listenerConfigGroupbox;
    QStackedWidget* configStackWidget;

    QMap<QString, WidgetBuilder*> listenersUI;
    AuthProfile                   authProfile;
    bool                          editMode = false;

    void createUI();

public:
    explicit DialogListener();
    ~DialogListener();

    void AddExListeners(QMap<QString, WidgetBuilder*> listeners);
    void SetProfile(AuthProfile profile);
    void Start();
    void SetEditMode(QString name);

protected slots:
    void changeConfig(QString fn);
    void onButtonLoad();
    void onButtonSave();
    void onButtonCreate();
    void onButtonCancel();
};

#endif //ADAPTIXCLIENT_DIALOGLISTENER_H