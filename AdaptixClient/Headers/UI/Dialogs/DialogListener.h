#ifndef ADAPTIXCLIENT_DIALOGLISTENER_H
#define ADAPTIXCLIENT_DIALOGLISTENER_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>

class AxContainerWrapper;

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
    QLabel*         listenerLabel          = nullptr;
    QComboBox*      listenerCombobox       = nullptr;
    QLabel*         listenerTypeLabel      = nullptr;
    QComboBox*      listenerTypeCombobox   = nullptr;
    QPushButton*    buttonLoad             = nullptr;
    QPushButton*    buttonSave             = nullptr;
    QPushButton*    buttonCreate           = nullptr;
    QPushButton*    buttonCancel           = nullptr;
    QGroupBox*      listenerConfigGroupbox = nullptr;
    QStackedWidget* configStackWidget      = nullptr;

    QList<RegListenerConfig> listeners;
    QMap<QString, QWidget*>  widgets;
    QMap<QString, AxContainerWrapper*> containers;

    AuthProfile authProfile;
    bool        editMode = false;

    void createUI();

public:
    explicit DialogListener(QWidget *parent = nullptr);
    ~DialogListener() override;

    void AddExListeners(const QList<RegListenerConfig> &listeners, const QMap<QString, QWidget*> &widgets, const QMap<QString, AxContainerWrapper*> &containers);
    void SetProfile(const AuthProfile &profile);
    void Start();
    void SetEditMode(const QString &name);

protected slots:
    void changeConfig(const QString &fn);
    void changeType(const QString &type);
    void onButtonLoad();
    void onButtonSave();
    void onButtonCreate();
    void onButtonCancel();
};

#endif