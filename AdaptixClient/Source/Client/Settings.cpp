#include <UI/Dialogs/DialogSettings.h>
#include <Client/Settings.h>
#include <Client/Storage.h>
#include <MainAdaptix.h>

Settings::Settings(MainAdaptix* m)
{
    mainAdaptix = m;

    this->SetDefault();
    this->LoadFromDB();
}

Settings::~Settings() = default;

MainAdaptix* Settings::getMainAdaptix()
{
    return this->mainAdaptix;
}

DialogSettings* Settings::getDialogSettings()
{
    if (!dialogSettings) {
        dialogSettings = new DialogSettings(this);
    }
    return dialogSettings;
}

void Settings::SetDefault()
{
    this->data.MainTheme    = "Dark";
    this->data.FontFamily   = "Adaptix - DejaVu Sans Mono";
    this->data.FontSize     = 10;
    this->data.GraphVersion = "Version 1";
    this->data.RemoteTerminalBufferSize = 10000;

    this->data.ConsoleTime = true;
    this->data.ConsoleBufferSize = 50000;
    this->data.ConsoleNoWrap = true;
    this->data.ConsoleAutoScroll = false;

    for ( int i = 0; i < 15; i++)
        data.SessionsTableColumns[i] = true;

    this->data.CheckHealth = true;
    this->data.HealthCoaf = 2.0;
    this->data.HealthOffset = 40;

    for ( int i = 0; i < 11; i++)
        data.TasksTableColumns[i] = true;

    this->data.TabNotificationsEnabled = true;
}

void Settings::LoadFromDB()
{
    mainAdaptix->storage->SelectSettingsMain( &data );
    mainAdaptix->storage->SelectSettingsConsole( &data );
    mainAdaptix->storage->SelectSettingsSessions( &data );
    mainAdaptix->storage->SelectSettingsGraph( &data );
    mainAdaptix->storage->SelectSettingsTasks( &data );
    mainAdaptix->storage->SelectSettingsNotifications( &data );
}

void Settings::SaveToDB() const
{
    mainAdaptix->storage->UpdateSettingsMain( data );
    mainAdaptix->storage->UpdateSettingsConsole( data );
    mainAdaptix->storage->UpdateSettingsSessions( data );
    mainAdaptix->storage->UpdateSettingsGraph( data );
    mainAdaptix->storage->UpdateSettingsTasks( data );
    mainAdaptix->storage->UpdateSettingsNotifications( data );
}
