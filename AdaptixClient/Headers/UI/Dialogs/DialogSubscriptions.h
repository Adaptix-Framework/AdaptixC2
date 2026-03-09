#ifndef ADAPTIXCLIENT_DIALOGSUBSCRIPTIONS_H
#define ADAPTIXCLIENT_DIALOGSUBSCRIPTIONS_H

#include <main.h>
#include <oclero/qlementine/widgets/Switch.hpp>

class AdaptixWidget;
class AuthProfile;

class DialogSubscriptions : public QDialog
{
Q_OBJECT

    AdaptixWidget* adaptixWidget = nullptr;
    QListWidget*   historyListWidget = nullptr;
    QListWidget*   realtimeListWidget = nullptr;
    oclero::qlementine::Switch* consoleTeammodeCheck = nullptr;
    oclero::qlementine::Switch* agentsOnlyActiveCheck = nullptr;
    oclero::qlementine::Switch* tasksOnlyJobsCheck = nullptr;
    QPushButton*   syncInactiveAgentsButton = nullptr;
    QPushButton*   applyButton = nullptr;
    QPushButton*   closeButton = nullptr;

    bool agentsOnlyActiveLocked = false;
    bool syncInactiveAgentsTriggered = false;

    QStringList dataCategories;
    QStringList agentCategories;
    QStringList currentSubscriptions;
    QStringList registeredCategories;

    void createUI();
    void updateItemState(QListWidgetItem *item, bool active, bool registered);
    void updateSyncInactiveAgentsButtonState();

public:
    explicit DialogSubscriptions(AdaptixWidget* adaptixWidget, QWidget* parent = nullptr);
    ~DialogSubscriptions() override = default;

    void SetCurrentSubscriptions(const QStringList &subs);
    void SetRegisteredCategories(const QStringList &cats);
    void SetConsoleMultiuser(bool multiuser);

private Q_SLOTS:
    void onApply();
    void onItemChanged(QListWidgetItem *item);
    void onSyncInactiveAgents();
};

#endif
