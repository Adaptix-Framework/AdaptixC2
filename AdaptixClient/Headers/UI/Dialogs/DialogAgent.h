#ifndef ADAPTIXCLIENT_DIALOGAGENT_H
#define ADAPTIXCLIENT_DIALOGAGENT_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <Utils/CustomElements.h>
#include <oclero/qlementine/widgets/PopoverButton.hpp>
#include <oclero/qlementine/widgets/Popover.hpp>
#include <oclero/qlementine/widgets/Menu.hpp>
#include <oclero/qlementine/widgets/Expander.hpp>

class AxContainerWrapper;
class BuildWorker;

class DialogAgent : public QDialog
{
Q_OBJECT

    QLabel*         listenerLabel       = nullptr;
    QLineEdit*      listenerInput       = nullptr;
    QLineEdit*      listenerDisplayEdit = nullptr;
    oclero::qlementine::PopoverButton* listenerSelectBtn   = nullptr;
    oclero::qlementine::Popover*       listenerPopover     = nullptr;
    QListWidget*    listenerListWidget  = nullptr;
    QPushButton*    btnMoveUp           = nullptr;
    QPushButton*    btnMoveDown         = nullptr;
    QWidget*        listenerSelectionWidget = nullptr;
    QLabel*         agentLabel          = nullptr;
    QComboBox*      agentCombobox       = nullptr;
    QLabel*         profileLabel        = nullptr;
    QAction*        actionSaveProfile   = nullptr;
    QLineEdit*      inputProfileName    = nullptr;
    bool            profileNameManuallyEdited = false;
    QPushButton*    buildButton         = nullptr;
    QGroupBox*      agentConfigGroupbox = nullptr;
    QStackedWidget* configStackWidget   = nullptr;

    QWidget*        buildLogPanel       = nullptr;
    QPushButton*    collapseButton      = nullptr;
    oclero::qlementine::Expander* buildLogExpander = nullptr;
    QTextEdit*      buildLogOutput      = nullptr;
    QThread*        buildThread         = nullptr;
    BuildWorker*    buildWorker         = nullptr;

    QLabel*           label_Profiles    = nullptr;
    CardListWidget*   cardWidget        = nullptr;
    oclero::qlementine::Menu* menuContext = nullptr;
    QPushButton*      buttonNewProfile  = nullptr;
    QPushButton*      buttonLoad        = nullptr;
    QPushButton*      buttonSave        = nullptr;

    AdaptixWidget* adaptixWidget = nullptr;
    AuthProfile authProfile;
    QString     listenerName;
    QString     listenerType;
    QVector<ListenerData> availableListeners;
    QMap<QString, AgentTypeInfo> agentTypes;

    QStringList agents;
    QMap<QString, AxUI> ax_uis;

    void regenerateAgentUI(const QString &agentName, const QStringList &selectedListeners);

    void createUI();
    void loadProfiles();
    void saveProfile(const QString &profileName, const QString &agentName, const QString &configData);
    QString generateUniqueProfileName(const QString &baseName);

public:
    explicit DialogAgent(AdaptixWidget* adaptixWidget, const QString &listenerName, const QString &listenerType);
    ~DialogAgent() override;

    void AddExAgents(const QStringList &agents, const QMap<QString, AxUI> &uis);
    void SetProfile(const AuthProfile &profile);
    void SetAvailableListeners(const QVector<ListenerData> &listeners);
    void SetAgentTypes(const QMap<QString, AgentTypeInfo> &types);
    void Start();

protected Q_SLOTS:
    void onButtonLoad();
    void onButtonSave();
    void changeConfig(const QString &agentName);
    void onButtonNewProfile();
    void onProfileSelected();
    void handleProfileContextMenu(const QPoint &pos);
    void onProfileRemove();
    void onProfileRename();
    void onProfileNameEdited(const QString &text);
    void onSaveProfileToggled(bool checked);
    void onButtonBuild();
    void onBuildConnected();
    void onBuildMessage(const QString &msg);
    void onBuildFinished();
    void stopBuild();
    void onListenerSelectionChanged(const QListWidgetItem *item);
    void onMoveListenerUp();
    void onMoveListenerDown();
    void showListenerPopup();
    void updateListenerDisplay();
};

#endif
