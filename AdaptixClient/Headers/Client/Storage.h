#ifndef ADAPTIXCLIENT_STORAGE_H
#define ADAPTIXCLIENT_STORAGE_H

#include <main.h>

class AuthProfile;

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
    static void UpdateProject(AuthProfile profile);
    static void RemoveProject(const QString &project);
    static bool ExistsProject(const QString &project);

    static QVector<ExtensionFile> ListExtensions();

    static bool ExistsExtension(const QString &path);
    static void AddExtension(const ExtensionFile &extFile);
    static void UpdateExtension(const ExtensionFile &extFile);
    static void RemoveExtension(const QString &filepath);

    static void SelectSettingsMain(SettingsData* settingsData);
    static void UpdateSettingsMain(const SettingsData &settingsData);

    static void SelectSettingsConsole(SettingsData* settingsData);
    static void UpdateSettingsConsole(const SettingsData &settingsData);

    static void SelectSettingsSessions(SettingsData* settingsData);
    static void UpdateSettingsSessions(const SettingsData &settingsData);

    static void SelectSettingsGraph(SettingsData* settingsData);
    static void UpdateSettingsGraph(const SettingsData &settingsData);

    static void SelectSettingsTasks(SettingsData* settingsData);
    static void UpdateSettingsTasks(const SettingsData &settingsData);

    static void SelectSettingsTabBlink(SettingsData* settingsData);
    static void UpdateSettingsTabBlink(const SettingsData &settingsData);

    static QVector<QPair<QString, QString>> ListListenerProfiles(const QString &project);
    static void AddListenerProfile(const QString &project, const QString &name, const QString &data);
    static void RemoveListenerProfile(const QString &project, const QString &name);
    static QString GetListenerProfile(const QString &project, const QString &name);

    static QVector<QPair<QString, QString>> ListAgentProfiles(const QString &project);
    static void AddAgentProfile(const QString &project, const QString &name, const QString &data);
    static void RemoveAgentProfile(const QString &project, const QString &name);
    static QString GetAgentProfile(const QString &project, const QString &name);
};

#endif