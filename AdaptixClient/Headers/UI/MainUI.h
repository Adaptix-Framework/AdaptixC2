#ifndef ADAPTIXCLIENT_MAINUI_H
#define ADAPTIXCLIENT_MAINUI_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>

class MainUI : public QMainWindow
{
    QTabWidget* mainuiTabWidget   = nullptr;
    // AdaptixWidget* mainAdaptixWidget = nullptr;

    QMap<QString, AdaptixWidget*> AdaptixProjects;

public:
    explicit MainUI();
    ~MainUI() override;

    static void onNewProject();
    void onCloseProject();

    static void onExtender();

    static void onSettings();

    void AddNewProject(AuthProfile* profile);
    void AddNewExtension(const ExtensionFile &extFile);
    void RemoveExtension(const ExtensionFile &extFile);

protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif //ADAPTIXCLIENT_MAINUI_H
