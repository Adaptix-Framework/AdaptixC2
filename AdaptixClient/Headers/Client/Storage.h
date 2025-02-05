#ifndef ADAPTIXCLIENT_STORAGE_H
#define ADAPTIXCLIENT_STORAGE_H

#include <main.h>
#include <Client/AuthProfile.h>

class Storage
{
    QSqlDatabase db;
    QString      appDirPath;

    void checkDatabase();

public:
    QString dbFilePath;

    Storage();
    ~Storage();

    QVector<AuthProfile> ListProjects();
    void AddProject(AuthProfile profile);
    void RemoveProject(QString project);
    bool ExistsProject(QString project);

    QVector<ExtensionFile> ListExtensions();
    bool ExistsExtension(QString path);
    void AddExtension(ExtensionFile extFile);
    void UpdateExtension(ExtensionFile extFile);
    void RemoveExtension(QString filepath);

    void SelectSettingsMain(SettingsData* settingsData);
    void UpdateSettingsMain(SettingsData settingsData);
};

#endif //ADAPTIXCLIENT_STORAGE_H