#include <Client/Storage.h>
#include <Client/AuthProfile.h>
#include <QJsonDocument>
#include <QJsonObject>

Storage::Storage()
{
    QString homeDirPath = QDir::homePath();
    bool appDirExists = false;
    appDirPath = QDir(homeDirPath).filePath(".adaptix");

    QDir appDir(appDirPath);
    if ( appDir.exists() ) {
        appDirExists = true;
    } else {
        if (appDir.mkpath(appDirPath))
            appDirExists = true;
        else
            LogError("Adaptix directory %s not created!\n", appDirPath.toStdString().c_str());
    }

    if( appDirExists ) {
        dbFilePath = QDir(appDirPath).filePath("storage-v1.db");
        db = QSqlDatabase::addDatabase( "QSQLITE" );
        db.setDatabaseName(dbFilePath);
        if (db.open())
            this->checkDatabase();
        else
            LogError("Adaptix Database did not opened: %s\n", db.lastError().text().toStdString().c_str());
    }
}

Storage::~Storage()
{
    if (db.isOpen())
        db.close();
}

void Storage::checkDatabase()
{
    auto queryProjects = QSqlQuery();
    queryProjects.prepare("CREATE TABLE IF NOT EXISTS Projects ( "
            "project TEXT UNIQUE PRIMARY KEY, "
            "data TEXT );"
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



    auto querySettings = QSqlQuery();
    querySettings.prepare("CREATE TABLE IF NOT EXISTS Settings ( "
                            "key TEXT UNIQUE PRIMARY KEY, "
                            "data TEXT );"
    );
    if ( !querySettings.exec() )
        LogError("Table Settings not created: %s\n", querySettings.lastError().text().toStdString().c_str());
}

/// PROJECTS

QVector<AuthProfile> Storage::ListProjects()
{
    auto list = QVector<AuthProfile>();
    QSqlQuery query;
    query.prepare( "SELECT project, data FROM Projects;" );
    if (query.exec()) {
        while (query.next()) {
            QString project = query.value("project").toString();
            QString data    = query.value("data").toString();

            QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
            QJsonObject   json = doc.object();

            AuthProfile profile(project,
                                json["username"].toString(),
                                json["password"].toString(),
                                json["host"].toString(),
                                json["port"].toString(),
                                json["endpoint"].toString(),
                                json["projectDir"].toString());
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
    QJsonObject json;
    json["host"]     = profile.GetHost();
    json["port"]     = profile.GetPort();
    json["endpoint"] = profile.GetEndpoint();
    json["username"] = profile.GetUsername();
    json["password"] = profile.GetPassword();
    json["projectDir"] = profile.GetProjectDir();
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query;
    query.prepare( "INSERT INTO Projects (project, data) VALUES (:Project, :Data);");
    query.bindValue(":Project", profile.GetProject());
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("The project has not been added to the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::UpdateProject(AuthProfile profile)
{
    QJsonObject json;
    json["host"]     = profile.GetHost();
    json["port"]     = profile.GetPort();
    json["endpoint"] = profile.GetEndpoint();
    json["username"] = profile.GetUsername();
    json["password"] = profile.GetPassword();
    json["projectDir"] = profile.GetProjectDir();
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query;
    query.prepare("UPDATE Projects SET data = :Data WHERE project = :Project;");
    query.bindValue(":Project", profile.GetProject());
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("The project has not been updated in the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveProject(const QString &project)
{
    QSqlQuery query;
    query.prepare("DELETE FROM Projects WHERE project = :Project");
    query.bindValue(":Project", project);
    if (!query.exec())
        LogError("Failed to delete project from database: %s\n", query.lastError().text().toStdString().c_str());
}

/// EXTENSIONS

QVector<ExtensionFile> Storage::ListExtensions()
{
    auto list = QVector<ExtensionFile>();
    QSqlQuery query;
    query.prepare( "SELECT filepath, enabled FROM Extensions;" );
    if ( query.exec() ) {
        while ( query.next() ) {
            ExtensionFile ext = {};
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
    if (!query.exec())
        LogError("The extension has not been added to the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::UpdateExtension(const ExtensionFile &extFile)
{
    QSqlQuery query;
    query.prepare( "UPDATE Extensions SET enabled = :Enabled WHERE filepath = :Filepath;");
    query.bindValue(":Filepath", extFile.FilePath.toStdString().c_str());
    query.bindValue(":Enabled", extFile.Enabled);
    if (!query.exec())
        LogError("Extension not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveExtension(const QString &filepath)
{
    QSqlQuery query;
    query.prepare("DELETE FROM Extensions WHERE filepath = :Filepath");
    query.bindValue(":Filepath", filepath);
    if (!query.exec())
        LogError("Failed to delete extension from database: %s\n", query.lastError().text().toStdString().c_str());
}

/// SETTINGS

void Storage::SelectSettingsMain(SettingsData* settingsData)
{
    QSqlQuery query;
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsMain' LIMIT 1;" );
    if ( query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        settingsData->MainTheme   = json["theme"].toString();
        settingsData->FontFamily  = json["fontFamily"].toString();
        settingsData->FontSize    = json["fontSize"].toInt();
        settingsData->ConsoleTime = json["consoleTime"].toBool();
    }
}

void Storage::UpdateSettingsMain(const SettingsData &settingsData)
{
    QJsonObject json;
    json["theme"]       = settingsData.MainTheme;
    json["fontFamily"]  = settingsData.FontFamily;
    json["fontSize"]    = settingsData.FontSize;
    json["consoleTime"] = settingsData.ConsoleTime;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsMain', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsMain not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsConsole(SettingsData* settingsData)
{
    QSqlQuery query;
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsConsole' LIMIT 1;" );
    if ( query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        settingsData->RemoteTerminalBufferSize = json["terminalBuffer"].toInt();
        settingsData->ConsoleBufferSize        = json["consoleBuffer"].toInt();
        settingsData->ConsoleNoWrap            = json["noWrap"].toBool();
        settingsData->ConsoleAutoScroll        = json["autoScroll"].toBool();
    }
}

void Storage::UpdateSettingsConsole(const SettingsData &settingsData)
{
    QJsonObject json;
    json["terminalBuffer"] = settingsData.RemoteTerminalBufferSize;
    json["consoleBuffer"]  = settingsData.ConsoleBufferSize;
    json["noWrap"]         = settingsData.ConsoleNoWrap;
    json["autoScroll"]     = settingsData.ConsoleAutoScroll;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsConsole', :Data);");
    query.bindValue(":Data", data);
    if ( !query.exec() )
        LogError("SettingsConsole not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsSessions(SettingsData* settingsData)
{
    QSqlQuery query;
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsSessions' LIMIT 1;" );
    if ( query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        settingsData->CheckHealth  = json["healthCheck"].toBool();
        settingsData->HealthCoaf   = json["healthCoaf"].toDouble();
        settingsData->HealthOffset = json["healthOffset"].toInt();

        QJsonArray columns = json["columns"].toArray();
        for (int i = 0; i < 15 && i < columns.size(); i++)
            settingsData->SessionsTableColumns[i] = columns[i].toBool();
    }
}

void Storage::UpdateSettingsSessions(const SettingsData &settingsData)
{
    QJsonArray columns;
    for (int i = 0 ; i < 15; i++)
        columns.append(settingsData.SessionsTableColumns[i]);

    QJsonObject json;
    json["healthCheck"]  = settingsData.CheckHealth;
    json["healthCoaf"]   = settingsData.HealthCoaf;
    json["healthOffset"] = settingsData.HealthOffset;
    json["columns"]      = columns;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsSessions', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsSessions not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsGraph(SettingsData* settingsData)
{
    QSqlQuery query;
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsGraph' LIMIT 1;" );
    if ( query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject  json  = doc.object();

        settingsData->GraphVersion = json["version"].toString();
    }
}

void Storage::UpdateSettingsGraph(const SettingsData &settingsData)
{
    QJsonObject json;
    json["version"] = settingsData.GraphVersion;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsGraph', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsGraph not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsTasks(SettingsData* settingsData)
{
    QSqlQuery query;
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsTasks' LIMIT 1;" );
    if ( query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        QJsonArray columns = json["columns"].toArray();
        for (int i = 0; i < 11 && i < columns.size(); i++)
            settingsData->TasksTableColumns[i] = columns[i].toBool();
    }
}

void Storage::UpdateSettingsTasks(const SettingsData &settingsData)
{
    QJsonArray columns;
    for (int i = 0 ; i < 11; i++)
        columns.append(settingsData.TasksTableColumns[i]);

    QJsonObject json;
    json["columns"] = columns;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsTasks', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsTasks not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}



void Storage::SelectSettingsTabBlink(SettingsData* settingsData)
{
    QSqlQuery query;
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsTablBlink' LIMIT 1;");
    if (query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        settingsData->TabBlinkEnabled = json["TabBlinkEnabled"].toBool();

        QJsonObject widgets = json["BlinkWidgets"].toObject();
        for (auto it = widgets.begin(); it != widgets.end(); ++it)
            settingsData->BlinkWidgets[it.key()] = it.value().toBool();
    }
}

void Storage::UpdateSettingsTabBlink(const SettingsData &settingsData)
{
    QJsonObject widgets;
    for (auto it = settingsData.BlinkWidgets.begin(); it != settingsData.BlinkWidgets.end(); ++it)
        widgets[it.key()] = it.value();

    QJsonObject json;
    json["TabBlinkEnabled"] = settingsData.TabBlinkEnabled;
    json["BlinkWidgets"] = widgets;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsTablBlink', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsTablBlink not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}
