#ifndef ADAPTIXCLIENT_STORAGE_H
#define ADAPTIXCLIENT_STORAGE_H

#include <main.h>
#include <Client/AuthProfile.h>

class Storage
{
    QSqlDatabase db;
    QString      appDirPath;

    static void checkDatabase();

public:
    QString dbFilePath;

    Storage();
    ~Storage();

    static QVector<AuthProfile> ListProjects();

    static void AddProject(AuthProfile profile);
    static void RemoveProject(const QString &project);
    static bool ExistsProject(const QString &project);

    static QVector<ExtensionFile> ListExtensions();

    static bool ExistsExtension(const QString &path);
    static void AddExtension(const ExtensionFile &extFile);
    static void UpdateExtension(const ExtensionFile &extFile);
    static void RemoveExtension(const QString &filepath);

    static void SelectSettingsMain(SettingsData* settingsData);
    static void UpdateSettingsMain(const SettingsData &settingsData);

    static void SelectSettingsSessions(SettingsData* settingsData);
    static void UpdateSettingsSessions(const SettingsData &settingsData);

    static void SelectSettingsTasks(SettingsData* settingsData);
    static void UpdateSettingsTasks(const SettingsData &settingsData);
};

#endif //ADAPTIXCLIENT_STORAGE_H