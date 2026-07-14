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
    this->data.MainTheme    = "Adaptix_Dark_Emerald";
    this->data.FontFamily   = "Adaptix - JetBrains Mono";
    this->data.FontSize     = 10;
    this->data.GraphVersion = "Version 1";
    this->data.GraphAutoHideInactive = true;
    this->data.GraphAutoHideNoChilds = false;
    this->data.RemoteTerminalBufferSize = 10000;
    this->data.ToolbarPosition = 0;

    this->data.ConsoleTime = true;
    this->data.ConsoleBufferSize = 50000;
    this->data.ConsoleNoWrap = true;
    this->data.ConsoleAutoScroll = false;
    this->data.ConsoleShowBackground = true;
    this->data.ConsoleUseAppTheme = true;
    this->data.ConsoleBgImagePath = ":/Back";
    this->data.ConsoleBgDimming = 80;
    this->data.ConsoleTheme = "console_adaptix_dark";
    this->data.ConsoleAutoLoadEarlier = true;
    this->data.ConsolePageSize = 50;

    for ( int i = 0; i < 17; i++) {
        data.SessionsTableColumns[i] = true;
        data.SessionsColumnOrder[i] = i;
    }

    this->data.CheckHealth = true;
    this->data.HealthCoaf = 2.0;
    this->data.HealthOffset = 40;
    this->data.DeadLightnessShift = 0.15;
    this->data.SessionsViewMode = 1;
    this->data.SessionsAutoHideInactive = false;
    this->data.SessionsCompactMode = false;

    for ( int i = 0; i < 11; i++)
        data.TasksTableColumns[i] = true;
    this->data.TasksViewMode = 1; // feed only
    this->data.TasksInProcessOnly = false;
    this->data.TasksCompactMode = false;

    for (int i = 0; i < 10; i++)
        data.TargetsTableColumns[i] = true;
    this->data.TargetsViewMode = 1;
    this->data.TargetsCompactMode = false;

    for (int i = 0; i < 10; i++)
        data.CredentialsTableColumns[i] = true;
    this->data.CredentialsViewMode = 1;
    this->data.CredentialsCompactMode = false;

    for (int i = 0; i < 11; i++)
        data.FilesTableColumns[i] = true;
    this->data.FilesCompactMode = false;

    this->data.PageSize = 100;

    this->data.TabBlinkEnabled = true;
}

void Settings::LoadFromDB()
{
    mainAdaptix->storage->SelectSettingsMain( &data );
    mainAdaptix->storage->SelectSettingsConsole( &data );
    mainAdaptix->storage->SelectSettingsSessions( &data );
    mainAdaptix->storage->SelectSettingsGraph( &data );
    mainAdaptix->storage->SelectSettingsTasks( &data );
    mainAdaptix->storage->SelectSettingsTargets( &data );
    mainAdaptix->storage->SelectSettingsCredentials( &data );
    mainAdaptix->storage->SelectSettingsFiles( &data );
    mainAdaptix->storage->SelectSettingsTabBlink( &data );
}

void Settings::SaveToDB() const
{
    mainAdaptix->storage->UpdateSettingsMain( data );
    mainAdaptix->storage->UpdateSettingsConsole( data );
    mainAdaptix->storage->UpdateSettingsSessions( data );
    mainAdaptix->storage->UpdateSettingsGraph( data );
    mainAdaptix->storage->UpdateSettingsTasks( data );
    mainAdaptix->storage->UpdateSettingsTargets( data );
    mainAdaptix->storage->UpdateSettingsCredentials( data );
    mainAdaptix->storage->UpdateSettingsFiles( data );
    mainAdaptix->storage->UpdateSettingsTabBlink( data );
}
