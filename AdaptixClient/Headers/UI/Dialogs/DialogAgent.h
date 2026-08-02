#ifndef ADAPTIXCLIENT_DIALOGAGENT_H
#define ADAPTIXCLIENT_DIALOGAGENT_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Utils/CustomElements/CardListWidget.h>
#include <Client/AuthProfile.h>

#include <oclero/qlementine/widgets/PopoverButton.hpp>
#include <oclero/qlementine/widgets/Popover.hpp>
#include <oclero/qlementine/widgets/Menu.hpp>

#include <QButtonGroup>
#include <QCheckBox>

class AxContainerWrapper;
class BuildWorker;

class DialogAgent : public QDialog
{
Q_OBJECT

    QLabel*         listenerLabel       = nullptr;
    oclero::qlementine::PopoverButton* listenerSelectBtn   = nullptr;
    oclero::qlementine::Popover*       listenerPopover     = nullptr;
    QListWidget*    listenerListWidget  = nullptr;
    QPushButton*    btnMoveUp           = nullptr;
    QPushButton*    btnMoveDown         = nullptr;
    QWidget*        listenerChipsContainer  = nullptr;
    QWidget*        listenerMultiField  = nullptr;
    QComboBox*      listenerCombobox    = nullptr;
    QLabel*         agentLabel          = nullptr;
    QComboBox*      agentCombobox       = nullptr;
    QLabel*         profileLabel        = nullptr;
    QAction*        actionSaveProfile   = nullptr;
    QLineEdit*      inputProfileName    = nullptr;
    bool            profileNameManuallyEdited = false;
    QPushButton*    buildButton         = nullptr;
    QPushButton*    cancelButton        = nullptr;
    QCheckBox*      storeCheck          = nullptr;
    QLineEdit*      inputDescription    = nullptr;
    QGroupBox*      agentConfigGroupbox = nullptr;
    QStackedWidget* configStackWidget   = nullptr;

    QPushButton*    configViewBtn       = nullptr;
    QPushButton*    logViewBtn          = nullptr;
    QButtonGroup*   viewButtonGroup     = nullptr;
    QPushButton*    fileChipButton      = nullptr;
    QStackedWidget* leftContentStack    = nullptr;
    QWidget*        buildLogPage        = nullptr;
    QTextEdit*      buildLogOutput      = nullptr;
    QThread*        buildThread         = nullptr;
    BuildWorker*    buildWorker         = nullptr;
    QString         buildFileName;
    QByteArray      buildFileContent;

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
    void rebuildListenerChips();
    void packDialogSize(int scriptW, int scriptH);
    void showConfigView();
    void showBuildLogView();

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
    void onStoreCheckToggled(bool checked);
    void onButtonBuild();
    void onBuildConnected();
    void onBuildMessage(const QString &msg);
    void onBuildFinished();
    void onSaveBuildFile();
    void stopBuild();
    void onListenerSelectionChanged(const QListWidgetItem *item);
    void onListenerComboChanged(int index);
    void onMoveListenerUp();
    void onMoveListenerDown();
    void showListenerPopup();
    void updateListenerDisplay();
};

#endif
