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
    auto queryProjects = QSqlQuery();
    queryProjects.prepare("CREATE TABLE IF NOT EXISTS Projects ( "
            "project TEXT UNIQUE PRIMARY KEY, "
            "host TEXT, "
            "port INTEGER, "
            "endpoint TEXT, "
            "username TEXT, "
            "password TEXT );"
    );
    if ( !queryProjects.exec() )
        LogError("Table PROJECTS not created: %s\n", queryProjects.lastError().text().toStdString().c_str());



    auto queryExtensions = QSqlQuery();
    queryExtensions.prepare("CREATE TABLE IF NOT EXISTS Extensions ( "
                          "filepath TEXT UNIQUE PRIMARY KEY, "
                          "enabled BOOLEAN );"
    );
    if ( !queryExtensions.exec() )
        LogError("Table EXTENSIONS not created: %s\n", queryExtensions.lastError().text().toStdString().c_str());



    auto querySettingsMain = QSqlQuery();
    querySettingsMain.prepare("CREATE TABLE IF NOT EXISTS SettingsMain ( "
                            "id INTEGER, "
                            "theme TEXT, "
                            "fontFamily TEXT, "
                            "fontSize INTEGER, "
                            "consoleTime BOOLEAN );"
    );
    if ( !querySettingsMain.exec() )
        LogError("Table SettingsMain not created: %s\n", querySettingsMain.lastError().text().toStdString().c_str());

    auto querySettingsSessions = QSqlQuery();
    querySettingsSessions.prepare("CREATE TABLE IF NOT EXISTS SettingsSessions ( "
                            "id INTEGER, "
                            "healthCheck BOOLEAN, "
                            "healthCoaf REAL, "
                            "healthOffset INTEGER, "
                            "column0 BOOLEAN, "
                            "column1 BOOLEAN, "
                            "column2 BOOLEAN, "
                            "column3 BOOLEAN, "
                            "column4 BOOLEAN, "
                            "column5 BOOLEAN, "
                            "column6 BOOLEAN, "
                            "column7 BOOLEAN, "
                            "column8 BOOLEAN, "
                            "column9 BOOLEAN, "
                            "column10 BOOLEAN, "
                            "column11 BOOLEAN, "
                            "column12 BOOLEAN, "
                            "column13 BOOLEAN, "
                            "column14 BOOLEAN );"
    );

    if ( !querySettingsSessions.exec() )
        LogError("Table SettingsSessions not created: %s\n", querySettingsSessions.lastError().text().toStdString().c_str());

    auto querySettingsTasks = QSqlQuery();
    querySettingsTasks.prepare("CREATE TABLE IF NOT EXISTS SettingsTasks ( "
                            "id INTEGER, "
                            "column0 BOOLEAN, "
                            "column1 BOOLEAN, "
                            "column2 BOOLEAN, "
                            "column3 BOOLEAN, "
                            "column4 BOOLEAN, "
                            "column5 BOOLEAN, "
                            "column6 BOOLEAN, "
                            "column7 BOOLEAN, "
                            "column8 BOOLEAN, "
                            "column9 BOOLEAN, "
                            "column10 BOOLEAN );"
    );

    if ( !querySettingsTasks.exec() )
        LogError("Table SettingsTasks not created: %s\n", querySettingsTasks.lastError().text().toStdString().c_str());
}

/// PROJECTS

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

bool Storage::ExistsProject(const QString &project)
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
        LogError("The project has not been added to the database: %s\n", query.lastError().text().toStdString().c_str());
    }
}

void Storage::RemoveProject(const QString &project)
{
    QSqlQuery query;
    query.prepare("DELETE FROM Projects WHERE project = :Project");
    query.bindValue(":Project", project);
    if (!query.exec()) {
        LogError("Failed to delete project from database: %s\n", query.lastError().text().toStdString().c_str());
    }
}

/// EXTENSIONS

QVector<ExtensionFile> Storage::ListExtensions()
{
    auto list = QVector<ExtensionFile>();
    QSqlQuery query;

    query.prepare( "SELECT * FROM Extensions;" );
    if ( query.exec() ) {
        while ( query.next() ) {
            ExtensionFile ext;
            ext.FilePath = query.value("filepath").toString();
            ext.Enabled  = query.value("enabled").toBool();
            list.push_back(ext);
        }
    }
    else {
        LogError("Failed to query extensions from database: %s\n", query.lastError().text().toStdString().c_str());
    }

    return list;
}

bool Storage::ExistsExtension(const QString &path)
{
    QSqlQuery query;
    query.prepare("SELECT 1 FROM Extensions WHERE filepath = :Filepath LIMIT 1;");
    query.bindValue(":Filepath", path);
    if (!query.exec()) {
        LogError("Failed to query extension from database: %s\n", query.lastError().text().toStdString().c_str());
        return false;
    }

    return query.next();
}

void Storage::AddExtension(const ExtensionFile &extFile)
{
    QSqlQuery query;
    query.prepare( "INSERT INTO Extensions (filepath, enabled) VALUES (:Filepath, :Enabled);");

    query.bindValue(":Filepath", extFile.FilePath.toStdString().c_str());
    query.bindValue(":Enabled", extFile.Enabled);

    if ( !query.exec() ) {
        LogError("The extension has not been added to the database: %s\n", query.lastError().text().toStdString().c_str());
    }
}

void Storage::UpdateExtension(const ExtensionFile &extFile)
{
    QSqlQuery query;
    query.prepare( "UPDATE Extensions SET enabled = :Enabled WHERE filepath = :Filepath;");

    query.bindValue(":Filepath", extFile.FilePath.toStdString().c_str());
    query.bindValue(":Enabled", extFile.Enabled);

    if ( !query.exec() ) {
        LogError("Extension not updated in database: %s\n", query.lastError().text().toStdString().c_str());
    }
}

void Storage::RemoveExtension(const QString &filepath)
{
    QSqlQuery query;
    query.prepare("DELETE FROM Extensions WHERE filepath = :Filepath");
    query.bindValue(":Filepath", filepath);
    if (!query.exec()) {
        LogError("Failed to delete extension from database: %s\n", query.lastError().text().toStdString().c_str());
    }
}

/// SETTINGS

void Storage::SelectSettingsMain(SettingsData* settingsData)
{
    QSqlQuery existsQuery;
    existsQuery.prepare("SELECT 1 FROM SettingsMain WHERE Id = 1 LIMIT 1;");
    if (!existsQuery.exec()) {
        LogError("Failed to existsQuery main setting from database: %s\n", existsQuery.lastError().text().toStdString().c_str());
        return;
    }
    bool exists = existsQuery.next();

    if(exists) {
        QSqlQuery selectQuery;
        selectQuery.prepare("SELECT * FROM SettingsMain WHERE Id = 1;" );
        if ( selectQuery.exec() && selectQuery.next()) {
            settingsData->MainTheme   = selectQuery.value("theme").toString();
            settingsData->FontFamily  = selectQuery.value("fontFamily").toString();
            settingsData->FontSize    = selectQuery.value("fontSize").toInt();
            settingsData->ConsoleTime = selectQuery.value("consoleTime").toBool();
        }
        else {
            LogError("Failed to selectQuery main settings from database: %s\n", selectQuery.lastError().text().toStdString().c_str());
        }
    }
}

void Storage::UpdateSettingsMain(const SettingsData &settingsData)
{
    QSqlQuery existsQuery;
    existsQuery.prepare("SELECT 1 FROM SettingsMain WHERE Id = 1 LIMIT 1;");
    if (!existsQuery.exec()) {
        LogError("Failed to existsQuery main setting from database: %s\n", existsQuery.lastError().text().toStdString().c_str());
        return;
    }
    bool exists = existsQuery.next();

    if(exists) {
        QSqlQuery updateQuery;
        updateQuery.prepare("UPDATE SettingsMain SET "
                            "theme = :Theme, "
                            "fontFamily = :FontFamily, "
                            "fontSize = :FontSize, "
                            "consoleTime = :ConsoleTime "
                            "WHERE Id = 1;");

        updateQuery.bindValue(":Theme", settingsData.MainTheme.toStdString().c_str());
        updateQuery.bindValue(":FontFamily", settingsData.FontFamily.toStdString().c_str());
        updateQuery.bindValue(":FontSize", settingsData.FontSize);
        updateQuery.bindValue(":ConsoleTime", settingsData.ConsoleTime);

        if ( !updateQuery.exec() ) {
            LogError("SettingsMain not updated in database: %s\n", updateQuery.lastError().text().toStdString().c_str());
        }
    }
    else {
        QSqlQuery insertQuery;
        insertQuery.prepare("INSERT INTO SettingsMain (id, theme, fontFamily, fontSize, consoleTime) VALUES (:Id, :Theme, :FontFamily, :FontSize, :ConsoleTime);");

        insertQuery.bindValue(":Id", 1);
        insertQuery.bindValue(":Theme", settingsData.MainTheme.toStdString().c_str());
        insertQuery.bindValue(":FontFamily", settingsData.FontFamily.toStdString().c_str());
        insertQuery.bindValue(":FontSize", settingsData.FontSize);
        insertQuery.bindValue(":ConsoleTime", settingsData.ConsoleTime);

        if ( !insertQuery.exec() ) {
            LogError("The main settings has not been added to the database: %s\n", insertQuery.lastError().text().toStdString().c_str());
        }
    }
}

void Storage::SelectSettingsSessions(SettingsData* settingsData)
{
    QSqlQuery existsQuery;
    existsQuery.prepare("SELECT 1 FROM SettingsSessions WHERE Id = 1 LIMIT 1;");
    if (!existsQuery.exec()) {
        LogError("Failed to existsQuery sessions setting from database: %s\n", existsQuery.lastError().text().toStdString().c_str());
        return;
    }
    bool exists = existsQuery.next();

    if(exists) {
        QSqlQuery selectQuery;
        selectQuery.prepare("SELECT * FROM SettingsSessions WHERE Id = 1;" );
        if ( selectQuery.exec() && selectQuery.next()) {

            settingsData->CheckHealth = selectQuery.value("healthCheck").toBool();
            settingsData->HealthCoaf = selectQuery.value("healthCoaf").toDouble();
            settingsData->HealthOffset = selectQuery.value("healthOffset").toInt();

            for (int i = 0; i < 15; i++) {
                QString columnName = "column" + QString::number(i);
                settingsData->SessionsTableColumns[i] = selectQuery.value(columnName).toBool();
            }
        }
        else {
            LogError("Failed to selectQuery sessions settings from database: %s\n", selectQuery.lastError().text().toStdString().c_str());
        }
    }
}

void Storage::UpdateSettingsSessions(const SettingsData &settingsData)
{
    QSqlQuery existsQuery;
    existsQuery.prepare("SELECT 1 FROM SettingsSessions WHERE Id = 1 LIMIT 1;");
    if (!existsQuery.exec()) {
        LogError("Failed to existsQuery sessions setting from database: %s\n", existsQuery.lastError().text().toStdString().c_str());
        return;
    }
    bool exists = existsQuery.next();

    if(exists) {
        QString strQuery = "UPDATE SettingsSessions SET healthCheck = :HealthCheck, healthCoaf = :HealthCoaf, healthOffset = :HealthOffset, column0 = :Column0";
        for (int i = 1 ; i < 15; i++)
            strQuery += QString(", column%1 = :Column%2").arg(i).arg(i);
        strQuery += " WHERE Id = 1;";

        QSqlQuery updateQuery;
        updateQuery.prepare(strQuery);
        updateQuery.bindValue(":HealthCheck", settingsData.CheckHealth);
        updateQuery.bindValue(":HealthCoaf", settingsData.HealthCoaf);
        updateQuery.bindValue(":HealthOffset", settingsData.HealthOffset);
        for (int i = 0 ; i < 15; i++) {
            QString column = ":Column" + QString::number(i);
            updateQuery.bindValue(column, settingsData.SessionsTableColumns[i]);
        }
        if ( !updateQuery.exec() ) {
            LogError("SettingsSessions not updated in database: %s\n", updateQuery.lastError().text().toStdString().c_str());
        }
    }
    else {
        QString strQuery = "INSERT INTO SettingsSessions (id, healthCheck, healthCoaf, healthOffset, column0";
        for (int i = 1 ; i < 15; i++)
            strQuery += QString(", column%1").arg(i);
        strQuery += ") VALUES (:Id, :HealthCheck, :HealthCoaf, :HealthOffset, :Column0";
        for (int i = 1 ; i < 15; i++)
            strQuery += QString(", :Column%1").arg(i);
        strQuery += ");";

        QSqlQuery insertQuery;
        insertQuery.prepare(strQuery);
        insertQuery.bindValue(":Id", 1);
        insertQuery.bindValue(":HealthCheck", settingsData.CheckHealth);
        insertQuery.bindValue(":HealthCoaf", settingsData.HealthCoaf);
        insertQuery.bindValue(":HealthOffset", settingsData.HealthOffset);
        for (int i = 0 ; i < 15; i++) {
            QString column = ":Column" + QString::number(i);
            insertQuery.bindValue(column, settingsData.SessionsTableColumns[i]);
        }

        if ( !insertQuery.exec() ) {
            LogError("The sessions settings has not been added to the database: %s\n", insertQuery.lastError().text().toStdString().c_str());
        }
    }
}

void Storage::SelectSettingsTasks(SettingsData* settingsData)
{
    QSqlQuery existsQuery;
    existsQuery.prepare("SELECT 1 FROM SettingsTasks WHERE Id = 1 LIMIT 1;");
    if (!existsQuery.exec()) {
        LogError("Failed to existsQuery sessions setting from database: %s\n", existsQuery.lastError().text().toStdString().c_str());
        return;
    }
    bool exists = existsQuery.next();

    if(exists) {
        QSqlQuery selectQuery;
        selectQuery.prepare("SELECT * FROM SettingsTasks WHERE Id = 1;" );
        if ( selectQuery.exec() && selectQuery.next()) {

            for (int i = 0; i < 11; i++) {
                QString columnName = "column" + QString::number(i);
                settingsData->TasksTableColumns[i] = selectQuery.value(columnName).toBool();
            }
        }
        else {
            LogError("Failed to selectQuery sessions settings from database: %s\n", selectQuery.lastError().text().toStdString().c_str());
        }
    }
}

void Storage::UpdateSettingsTasks(const SettingsData &settingsData)
{
    QSqlQuery existsQuery;
    existsQuery.prepare("SELECT 1 FROM SettingsTasks WHERE Id = 1 LIMIT 1;");
    if (!existsQuery.exec()) {
        LogError("Failed to existsQuery sessions setting from database: %s\n", existsQuery.lastError().text().toStdString().c_str());
        return;
    }
    bool exists = existsQuery.next();

    if(exists) {
        QString strQuery = "UPDATE SettingsTasks SET column0 = :Column0";
        for (int i = 1 ; i < 11; i++)
            strQuery += QString(", column%1 = :Column%2").arg(i).arg(i);
        strQuery += " WHERE Id = 1;";

        QSqlQuery updateQuery;
        updateQuery.prepare(strQuery);
        for (int i = 0 ; i < 11; i++) {
            QString column = ":Column" + QString::number(i);
            updateQuery.bindValue(column, settingsData.TasksTableColumns[i]);
        }
        if ( !updateQuery.exec() ) {
            LogError("SettingsTasks not updated in database: %s\n", updateQuery.lastError().text().toStdString().c_str());
        }
    }
    else {
        QString strQuery = "INSERT INTO SettingsTasks (id, column0";
        for (int i = 1 ; i < 11; i++)
            strQuery += QString(", column%1").arg(i);
        strQuery += ") VALUES (:Id, :Column0";
        for (int i = 1 ; i < 11; i++)
            strQuery += QString(", :Column%1").arg(i);
        strQuery += ");";

        QSqlQuery insertQuery;
        insertQuery.prepare(strQuery);
        insertQuery.bindValue(":Id", 1);
        for (int i = 0 ; i < 11; i++) {
            QString column = ":Column" + QString::number(i);
            insertQuery.bindValue(column, settingsData.TasksTableColumns[i]);
        }

        if ( !insertQuery.exec() ) {
            LogError("The sessions settings has not been added to the database: %s\n", insertQuery.lastError().text().toStdString().c_str());
        }
    }
}
