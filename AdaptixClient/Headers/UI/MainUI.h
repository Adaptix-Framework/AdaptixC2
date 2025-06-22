#ifndef ADAPTIXCLIENT_MAINUI_H
#define ADAPTIXCLIENT_MAINUI_H

#include <main.h>

class AuthProfile;
class AdaptixWidget;
class WebSocketWorker;

class MainUI : public QMainWindow
{
    QTabWidget* mainuiTabWidget = nullptr;

    QMap<QString, AdaptixWidget*> AdaptixProjects;

public:
    explicit MainUI();
    ~MainUI() override;

    static void onNewProject();
    void onCloseProject();

    static void onExtender();
    static void onSettings();

    void AddNewProject(AuthProfile* profile, QThread* channelThread, WebSocketWorker* channelWsWorker);

    void AddNewExtension(const ExtensionFile &extFile);
    void RemoveExtension(const ExtensionFile &extFile);

    void UpdateSessionsTableColumns();
    void UpdateGraphIcons();
    void UpdateTasksTableColumns();

protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif //ADAPTIXCLIENT_MAINUI_H
