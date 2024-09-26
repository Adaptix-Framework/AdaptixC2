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

    void                 AddProject(AuthProfile profile);
    void                 RemoveProject(QString project);
    bool                 ExistsProject(QString project);
    QVector<AuthProfile> ListProjects();
};

#endif //ADAPTIXCLIENT_STORAGE_H