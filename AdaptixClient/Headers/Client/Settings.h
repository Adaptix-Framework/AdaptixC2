#ifndef ADAPTIXCLIENT_SETTINGS_H
#define ADAPTIXCLIENT_SETTINGS_H

#include <main.h>

class DialogSettings;
class MainAdaptix;

class Settings
{
    MainAdaptix*    mainAdaptix    = nullptr;
    DialogSettings* dialogSettings = nullptr;

public:
    explicit Settings(MainAdaptix* m);
    ~Settings();

    SettingsData data;

    MainAdaptix*    getMainAdaptix();
    DialogSettings* getDialogSettings();
    void SetDefault();
    void LoadFromDB();
    void SaveToDB() const;
};

#endif
