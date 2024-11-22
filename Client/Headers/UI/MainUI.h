#ifndef ADAPTIXCLIENT_MAINUI_H
#define ADAPTIXCLIENT_MAINUI_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>

class MainUI : public QMainWindow
{
    QWidget* mainWidget = nullptr;

public:
    explicit MainUI();
    ~MainUI();

    void addNewProject(AuthProfile* profile);
};

#endif //ADAPTIXCLIENT_MAINUI_H
