#include <Client/Storage.h>

Storage::Storage()
{
    QString homeDirPath = QDir::homePath();
    bool appDirExists = false;
    appDirPath = QDir(homeDirPath).filePath(".adaptix");

    QDir appDir(appDirPath);
    if ( appDir.exists() ) {
        appDirExists = true;
    } else {
        if (appDir.mkpath(appDirPath)) {
            appDirExists = true;
        }
        else {
            LogError("Adaptix directory %s not created!\n", appDirPath.toStdString().c_str());
        }
    }

    if( appDirExists ) {
        dbFilePath = QDir(appDirPath).filePath("storage.db");
        db = QSqlDatabase::addDatabase( "QSQLITE" );
        db.setDatabaseName(dbFilePath);

        if ( db.open() ) {
            this->checkDatabase();
        }
        else {
            LogError("Adaptix Database did not opened: %s\n", db.lastError().text().toStdString().c_str());
        }
    }
}

Storage::~Storage()
{
    if (db.isOpen()) {
        db.close();
    }
}

void Storage::checkDatabase()
{
    auto query = QSqlQuery();
    query.prepare("CREATE TABLE IF NOT EXISTS Projects ( "
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "project TEXT UNIQUE, "
            "host TEXT, "
            "port INTEGER, "
            "endpoint TEXT, "
            "username TEXT, "
            "password TEXT );"
    );

    if ( !query.exec() ) {
        LogError("Table PROJECTS not created: %s\n", query.lastError().text().toStdString().c_str());
    }
}

void Storage::AddProject(AuthProfile profile)
{
    QSqlQuery query;
    query.prepare( "INSERT INTO Projects (project, host, port, endpoint, username, password) VALUES (:Project, :Host, :Port, :Endpoint, :Username, :Password);");

    query.bindValue(":Project", profile.GetProject().toStdString().c_str());
    query.bindValue(":Host", profile.GetHost().toStdString().c_str());
    query.bindValue(":Port", profile.GetPort().toStdString().c_str());
    query.bindValue(":Endpoint", profile.GetEndpoint().toStdString().c_str());
    query.bindValue(":Username", profile.GetUsername().toStdString().c_str());
    query.bindValue(":Password", profile.GetPassword().toStdString().c_str());

    if ( !query.exec() ) {
        LogError("The project has not been added to the database.: %s\n", query.lastError().text().toStdString().c_str());
    }
}

QVector<AuthProfile> Storage::ListProjects()
{
    auto list = QVector<AuthProfile>();
    QSqlQuery query;

    query.prepare( "SELECT * FROM Projects;" );
    if ( query.exec() ) {
        while ( query.next() ) {
            AuthProfile profile(query.value( "project" ).toString(),
                                query.value( "username" ).toString(),
                                query.value( "password" ).toString(),
                                query.value( "host" ).toString(),
                                query.value( "port" ).toString(),
                                query.value( "endpoint" ).toString() );
            list.push_back(profile);
        }
    }
    else {
        LogError("Failed to query projects from database: %s\n", query.lastError().text().toStdString().c_str());
    }

    return list;
}

bool Storage::ExistsProject(QString project)
{
    QSqlQuery query;

    query.prepare("SELECT 1 FROM Projects WHERE project = :Project LIMIT 1;");
    query.bindValue(":Project", project);
    if (!query.exec()) {
        LogError("Failed to query projects from database: %s\n", query.lastError().text().toStdString().c_str());
        return false;
    }

    return query.next();
}

void Storage::RemoveProject(QString project)
{
    QSqlQuery query;
    query.prepare("DELETE FROM Projects WHERE project = :Project");
    query.bindValue(":Project", project);
    if (!query.exec()) {
        LogError("Failed to delete project from database: %s\n", query.lastError().text().toStdString().c_str());
    }
}
