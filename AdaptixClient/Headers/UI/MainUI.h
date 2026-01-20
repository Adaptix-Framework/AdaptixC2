#ifndef ADAPTIXCLIENT_MAINUI_H
#define ADAPTIXCLIENT_MAINUI_H

#include <main.h>
#include <functional>

class AuthProfile;
class AdaptixWidget;
class WebSocketWorker;

class MainUI : public QMainWindow
{
    QTabWidget* mainuiTabWidget = nullptr;

    QVector<AdaptixWidget*> AdaptixProjects;

    QMenu* menuProject   = nullptr;
    QMenu* menuExtensions  = nullptr;
    QMenu* menuSettings  = nullptr;
    QAction* extDocksSeparator = nullptr;

    QMap<QString, QAction*> extDockActions;

    void onOpenProjectDirectory();
    void onTabChanged(int index);
    void updateTabButton(int index, const QString& tabName, bool showButton = false);

public:
    explicit MainUI();
    ~MainUI() override;

    static void onNewProject();
    void onCloseProject();
    void onAxScriptConsole();

    static void onScriptManager();
    static void onSettings();

    void AddNewProject(AuthProfile* profile, QThread* channelThread, WebSocketWorker* channelWsWorker);

    bool AddNewExtension(ExtensionFile *extFile);
    bool SyncExtension(const QString &Project, ExtensionFile *extFile);
    void RemoveExtension(const ExtensionFile &extFile);

    void UpdateSessionsTableColumns();
    void UpdateGraphIcons();
    void UpdateTasksTableColumns();

    AuthProfile* GetCurrentProfile() const;

    QMenu* getMenuProject() const;
    QMenu* getMenuAxScript() const;
    QMenu* getMenuSettings() const;

    void addExtDockAction(const QString &id, const QString &title, bool checked, const std::function<void(bool)> &callback);
    void removeExtDockAction(const QString &id);
    void setExtDockChecked(const QString &id, bool checked);

protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif
