#ifndef ADAPTIXCLIENT_MAINADAPTIX_H
#define ADAPTIXCLIENT_MAINADAPTIX_H

#include <main.h>

class Extender;
class Settings;
class Storage;
class MainUI;
class AuthProfile;

class MainAdaptix : public QWidget {

public:
    MainUI*   mainUI   = nullptr;
    Storage*  storage  = nullptr;
    Extender* extender = nullptr;
    Settings* settings = nullptr;

    explicit MainAdaptix();
    ~MainAdaptix() override;

    static void Exit();

    void Start() const;
    void NewProject() const;
    void SetApplicationTheme() const;

    static AuthProfile* Login();
};

extern MainAdaptix* GlobalClient;

#endif
