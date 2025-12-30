#ifndef ADAPTIXCLIENT_DIALOGAGENT_H
#define ADAPTIXCLIENT_DIALOGAGENT_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <Utils/CustomElements.h>

class AxContainerWrapper;

class DialogAgent : public QDialog
{
Q_OBJECT

    QLabel*         listenerLabel       = nullptr;
    QLineEdit*      listenerInput       = nullptr;
    QLabel*         agentLabel          = nullptr;
    QComboBox*      agentCombobox       = nullptr;
    QLabel*         profileLabel        = nullptr;
    QAction*        actionSaveProfile   = nullptr;
    QLineEdit*      inputProfileName    = nullptr;
    bool            profileNameManuallyEdited = false;
    QPushButton*    generateButton      = nullptr;
    QGroupBox*      agentConfigGroupbox = nullptr;
    QStackedWidget* configStackWidget   = nullptr;

    QLabel*           label_Profiles    = nullptr;
    CardListWidget*   cardWidget        = nullptr;
    QMenu*            menuContext       = nullptr;
    QPushButton*      buttonNewProfile  = nullptr;
    QPushButton*      buttonLoad        = nullptr;
    QPushButton*      buttonSave        = nullptr;

    AuthProfile authProfile;
    QString     listenerName;
    QString     listenerType;

    QStringList agents;
    QMap<QString, AxUI> ax_uis;

    void createUI();
    void loadProfiles();
    void saveProfile(const QString &profileName, const QString &agentName, const QString &configData);
    QString generateUniqueProfileName(const QString &baseName);

public:
    explicit DialogAgent(const QString &listenerName, const QString &listenerType);
    ~DialogAgent() override;

    void AddExAgents(const QStringList &agents, const QMap<QString, AxUI> &uis);
    void SetProfile(const AuthProfile &profile);
    void Start();

protected Q_SLOTS:
    void onButtonLoad();
    void onButtonSave();
    void changeConfig(const QString &agentName);
    void onButtonGenerate();
    void onButtonNewProfile();
    void onProfileSelected();
    void handleProfileContextMenu(const QPoint &pos);
    void onProfileRemove();
    void onProfileRename();
    void onProfileNameEdited(const QString &text);
    void onSaveProfileToggled(bool checked);
};

#endif
