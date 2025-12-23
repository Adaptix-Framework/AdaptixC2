#ifndef ADAPTIXCLIENT_DIALOGAGENT_H
#define ADAPTIXCLIENT_DIALOGAGENT_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>

class AxContainerWrapper;

class ProfileListDelegate;

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
    QWidget*        rightPanelWidget    = nullptr;
    QWidget*        collapsibleDivider   = nullptr;
    QPushButton*    buttonToggleRightPanel = nullptr;
    QFrame*         line_2              = nullptr;
    QGroupBox*      profilesGroupbox     = nullptr;
    QListWidget*    listWidgetProfiles  = nullptr;
    QMenu*          menuContext         = nullptr;
    QGroupBox*      agentConfigGroupbox = nullptr;
    QStackedWidget* configStackWidget   = nullptr;

    AuthProfile authProfile;
    QString     listenerName;
    QString     listenerType;
    QString     currentProfileName;
    bool        rightPanelCollapsed = false;
    QSize       collapsedSize;
    int         panelWidth = 0;

    QStringList agents;
    QMap<QString, AxUI> ax_uis;

    void createUI();
    void loadProfiles();
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void updateButtonPosition();

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
    void onButtonGenerateAgent();
    void onButtonClose();
    void onProfileSelected();
    void handleProfileContextMenu(const QPoint &pos);
    void onProfileRemove();
    void onSetBackgroundColor();
    void onToggleRightPanel();
};

#endif
