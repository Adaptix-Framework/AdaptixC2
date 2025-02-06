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
}

void Settings::LoadFromDB()
{
    mainAdaptix->storage->SelectSettingsMain( &data );
}

void Settings::SaveToDB()
{
    mainAdaptix->storage->UpdateSettingsMain( data );
}