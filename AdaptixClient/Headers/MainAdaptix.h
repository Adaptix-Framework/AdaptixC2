#ifndef ADAPTIXCLIENT_MAINADAPTIX_H
#define ADAPTIXCLIENT_MAINADAPTIX_H

#include <main.h>
#include <UI/MainUI.h>
#include <Client/Storage.h>
#include <Client/Extender.h>

class Extender;

class MainAdaptix : public QWidget {

public:
    MainUI*   mainUI   = nullptr;
    Storage*  storage  = nullptr;
    Extender* extender = nullptr;

    explicit MainAdaptix();
    ~MainAdaptix() override;

    void Start();
    void Exit();
    void SetApplicationTheme();
};

extern MainAdaptix* GlobalClient;

#endif //ADAPTIXCLIENT_MAINADAPTIX_H
