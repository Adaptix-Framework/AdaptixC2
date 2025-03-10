#ifndef ADAPTIXCLIENT_DIALOGLISTENER_H
#define ADAPTIXCLIENT_DIALOGLISTENER_H

#include <main.h>
#include <Client/WidgetBuilder.h>
#include <Client/AuthProfile.h>

class DialogListener : public QDialog
{
    QGridLayout*    mainGridLayout         = nullptr;
    QGridLayout*    stackGridLayout        = nullptr;
    QSpacerItem*    horizontalSpacer       = nullptr;
    QSpacerItem*    horizontalSpacer_2     = nullptr;
    QSpacerItem*    horizontalSpacer_3     = nullptr;
    QHBoxLayout*    hLayoutBottom          = nullptr;
    QFrame*         line_1                 = nullptr;
    QLabel*         listenerNameLabel      = nullptr;
    QLineEdit*      inputListenerName      = nullptr;
    QLabel*         listenerTypeLabel      = nullptr;
    QComboBox*      listenerTypeCombobox   = nullptr;
    QPushButton*    buttonLoad             = nullptr;
    QPushButton*    buttonSave             = nullptr;
    QPushButton*    buttonCreate           = nullptr;
    QPushButton*    buttonCancel           = nullptr;
    QGroupBox*      listenerConfigGroupbox = nullptr;
    QStackedWidget* configStackWidget      = nullptr;

    QMap<QString, WidgetBuilder*> listenersUI;
    AuthProfile                   authProfile;
    bool                          editMode = false;

    void createUI();

public:
    explicit DialogListener();
    ~DialogListener() override;

    void AddExListeners(const QMap<QString, WidgetBuilder*> &listeners);
    void SetProfile(const AuthProfile &profile);
    void Start();
    void SetEditMode(const QString &name);

protected slots:
    void changeConfig(const QString &fn);
    void onButtonLoad();
    void onButtonSave();
    void onButtonCreate();
    void onButtonCancel();
};

#endif //ADAPTIXCLIENT_DIALOGLISTENER_H