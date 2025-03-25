#include <Client/Settings.h>

Settings::Settings(MainAdaptix* m)
{
    mainAdaptix = m;

    this->SetDefault();
    this->LoadFromDB();

    dialogSettings = new DialogSettings(this);
}

Settings::~Settings() = default;

void Settings::SetDefault()
{
    this->data.MainTheme   = "Dark";
    this->data.FontFamily  = "Adaptix - DejaVu Sans Mono";
    this->data.FontSize    = 10;
    this->data.ConsoleTime = true;

    for ( int i = 0; i < 15; i++)
        data.SessionsTableColumns[i] = true;

    this->data.CheckHealth = true;
    this->data.HealthCoaf = 2.0;
    this->data.HealthOffset = 40;

    for ( int i = 0; i < 11; i++)
        data.TasksTableColumns[i] = true;
}

void Settings::LoadFromDB()
{
    mainAdaptix->storage->SelectSettingsMain( &data );
    mainAdaptix->storage->SelectSettingsSessions( &data );
    mainAdaptix->storage->SelectSettingsTasks( &data );
}

void Settings::SaveToDB() const
{
    mainAdaptix->storage->UpdateSettingsMain( data );
    mainAdaptix->storage->UpdateSettingsSessions( data );
    mainAdaptix->storage->UpdateSettingsTasks( data );
}
