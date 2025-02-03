#include <Client/Settings.h>

Settings::Settings(MainAdaptix* m)
{
    mainAdaptix = m;
    dialogSettings = new DialogSettings(this);
}

Settings::~Settings() = default;