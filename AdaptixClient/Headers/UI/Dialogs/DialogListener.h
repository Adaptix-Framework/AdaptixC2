#ifndef ADAPTIXCLIENT_DIALOGLISTENER_H
#define ADAPTIXCLIENT_DIALOGLISTENER_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <UI/Dialogs/ProfileListDelegate.h>

class DialogListener : public QDialog
{
    QGridLayout*    mainGridLayout         = nullptr;
    QGridLayout*    stackGridLayout        = nullptr;
    QSpacerItem*    horizontalSpacer       = nullptr;
    QSpacerItem*    horizontalSpacer_2     = nullptr;
    QSpacerItem*    horizontalSpacer_3     = nullptr;
    QHBoxLayout*    hLayoutBottom          = nullptr;
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
    
    // Profile management block
    QWidget*             rightPanelWidget         = nullptr;
    QWidget*             collapsibleDivider       = nullptr;
    QPushButton*         buttonToggleRightPanel   = nullptr;
    QFrame*              line_2                   = nullptr;
    QGroupBox*           profilesGroupbox         = nullptr;
    QListWidget*         listWidgetProfiles      = nullptr;
    QMenu*               menuContext            = nullptr;
    ProfileListDelegate* profileDelegate        = nullptr;

    QList<RegListenerConfig> listeners;
    QMap<QString, AxUI> ax_uis;

    AuthProfile authProfile;
    bool        editMode = false;
    bool        rightPanelCollapsed = false;
    QSize       collapsedSize;
    int         panelWidth = 0;

    void createUI();
    void loadProfiles();
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void updateButtonPosition();

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
    void onButtonLoad();
    void onButtonSave();
    void onButtonCreate();
    void onButtonCancel();
    void onProfileSelected();
    void handleProfileContextMenu(const QPoint &pos);
    void onProfileRemove();
    void onSetBackgroundColor();
    void onToggleRightPanel();
};

#endif