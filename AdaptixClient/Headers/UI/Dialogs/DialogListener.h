#ifndef ADAPTIXCLIENT_DIALOGLISTENER_H
#define ADAPTIXCLIENT_DIALOGLISTENER_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <Utils/CustomElements.h>

class AxContainerWrapper;

class DialogListener : public QDialog
{
Q_OBJECT

    QGridLayout*    mainGridLayout         = nullptr;
    QGridLayout*    stackGridLayout        = nullptr;
    QLabel*         listenerNameLabel      = nullptr;
    QLineEdit*      inputListenerName      = nullptr;
    QLabel*         profileLabel           = nullptr;
    QAction*        actionSaveProfile      = nullptr;
    QLineEdit*      inputProfileName       = nullptr;
    bool            profileNameManuallyEdited = false;
    QLabel*         listenerLabel          = nullptr;
    QComboBox*      listenerCombobox       = nullptr;
    QLabel*         listenerTypeLabel      = nullptr;
    QComboBox*      listenerTypeCombobox   = nullptr;
    QPushButton*    buttonCreate           = nullptr;
    QPushButton*    buttonNewProfile       = nullptr;
    QPushButton*    buttonLoad             = nullptr;
    QPushButton*    buttonSave             = nullptr;
    QGroupBox*      listenerConfigGroupbox = nullptr;
    QStackedWidget* configStackWidget      = nullptr;

    // Profile management block
    QLabel*           label_Profiles     = nullptr;
    CardListWidget*   cardWidget         = nullptr;
    QMenu*            menuContext        = nullptr;

    QList<RegListenerConfig> listeners;
    QMap<QString, AxUI> ax_uis;

    AuthProfile authProfile;
    bool        editMode = false;
    QString     editProfileName;

    void createUI();
    void loadProfiles();
    void saveProfile(const QString &profileName, const QString &configName, const QString &configType, const QString &configData);

public:
    explicit DialogListener(QWidget *parent = nullptr);
    ~DialogListener() override;

    void AddExListeners(const QList<RegListenerConfig> &listeners, const QMap<QString, AxUI> &uis);
    void SetProfile(const AuthProfile &profile);
    void Start();
    void SetEditMode(const QString &name);

protected Q_SLOTS:
    void changeConfig(const QString &fn);
    void changeType(const QString &type);
    void onButtonCreate();
    void onButtonNewProfile();
    void onButtonLoad();
    void onButtonSave();
    void onProfileSelected();
    void handleProfileContextMenu(const QPoint &pos);
    void onProfileRemove();
    void onProfileRename();
    void onListenerNameChanged(const QString &text);
    void onProfileNameEdited(const QString &text);
    void onSaveProfileToggled(bool checked);
};

#endif