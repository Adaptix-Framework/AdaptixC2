#ifndef ADAPTIXCLIENT_MAINADAPTIX_H
#define ADAPTIXCLIENT_MAINADAPTIX_H

#include <main.h>
#include <Client/Storage.h>
#include <UI/MainUI.h>

class MainAdaptix : public QWidget {

public:
    Storage* storage   = nullptr;
    MainUI*  mainUI = nullptr;

    explicit MainAdaptix();
    ~MainAdaptix() override;

    void Start();
    void Exit();
    void SetApplicationTheme();
};

extern MainAdaptix* GlobalClient;

#endif //ADAPTIXCLIENT_MAINADAPTIX_H
