#ifndef ADAPTIXCLIENT_MAINUI_H
#define ADAPTIXCLIENT_MAINUI_H

#include <main.h>

class AuthProfile;
class AdaptixWidget;
class WebSocketWorker;

class MainUI : public QMainWindow
{
    QTabWidget* mainuiTabWidget = nullptr;
    QPushButton* newProjectButton = nullptr;

    QVector<AdaptixWidget*> AdaptixProjects;

    void onOpenProjectDirectory();
    void onTabChanged(int index);
    void updateTabButton(int index, const QString& tabName, bool showButton = false);
    void onTabContextMenu(const QPoint &pos);

public:
    explicit MainUI();
    ~MainUI() override;

    static void onNewProject();
    void onCloseProject();
    void onCloseProjectRequested();
    void onProjectSubscriptions();
    void onScriptsDock();

    static void onSettings();

    void AddNewProject(AuthProfile* profile, QThread* channelThread, WebSocketWorker* channelWsWorker);

    bool AddNewExtension(ExtensionFile *extFile);
    bool SyncExtension(const QString &Project, ExtensionFile *extFile);
    void RemoveExtension(const ExtensionFile &extFile);

    void UpdateSessionsTableColumns();
    void UpdateConsolePrefs();
    void UpdateGraphIcons();
    void UpdateTasksTableColumns();
    void UpdateTargetsColumns();
    void UpdateCredentialsColumns();
    void UpdateFilesColumns();
    void ApplyFeedViewPreferences();
    void RebuildToolbars();

    AuthProfile* GetCurrentProfile() const;
    QVector<AdaptixWidget*> GetAdaptixProjects() const;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif
