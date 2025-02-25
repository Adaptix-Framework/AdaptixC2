#ifndef ADAPTIXCLIENT_MAINUI_H
#define ADAPTIXCLIENT_MAINUI_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>

class MainUI : public QMainWindow
{
    QTabWidget*    mainuiTabWidget   = nullptr;
    // AdaptixWidget* mainAdaptixWidget = nullptr;

    QMap<QString, AdaptixWidget*> AdaptixProjects;

public:
    explicit MainUI();
    ~MainUI();

    void onNewProject();
    void onCloseProject();
    void onExtender();
    void onSettings();

    void AddNewProject(AuthProfile* profile);
    void AddNewExtension(ExtensionFile extFile);
    void RemoveExtension(ExtensionFile extFile);
};

#endif //ADAPTIXCLIENT_MAINUI_H
