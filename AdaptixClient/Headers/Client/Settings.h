#ifndef ADAPTIXCLIENT_SETTINGS_H
#define ADAPTIXCLIENT_SETTINGS_H

#include <main.h>
#include <UI/Dialogs/DialogSettings.h>
#include <MainAdaptix.h>

class DialogSettings;
class MainAdaptix;

class Settings
{
    MainAdaptix* mainAdaptix = nullptr;

public:
    Settings(MainAdaptix* m);
    ~Settings();

    DialogSettings* dialogSettings = nullptr;
    SettingsData    data;

    void SetDefault();
    void LoadFromDB();
    void SaveToDB();
};

#endif //ADAPTIXCLIENT_SETTINGS_H
