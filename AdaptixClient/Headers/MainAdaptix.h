#ifndef ADAPTIXCLIENT_MAINADAPTIX_H
#define ADAPTIXCLIENT_MAINADAPTIX_H

#include <main.h>
#include <UI/MainUI.h>
#include <Client/Storage.h>
#include <Client/Extender.h>
#include <Client/Settings.h>

class Extender;
class Settings;

class MainAdaptix : public QWidget {

public:
    MainUI*   mainUI   = nullptr;
    Storage*  storage  = nullptr;
    Extender* extender = nullptr;
    Settings* settings = nullptr;

    explicit MainAdaptix();
    ~MainAdaptix() override;

    void         Start();
    void         Exit();
    void         NewProject();
    void         SetApplicationTheme();
    AuthProfile* Login();
};

extern MainAdaptix* GlobalClient;

#endif //ADAPTIXCLIENT_MAINADAPTIX_H
