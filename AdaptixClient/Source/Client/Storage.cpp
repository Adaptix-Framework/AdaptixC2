#include <Client/Storage.h>
#include <Client/AuthProfile.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static const QString DB_CONNECTION_NAME = "adaptix_connection";

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
        db = QSqlDatabase::addDatabase( "QSQLITE", DB_CONNECTION_NAME );
        db.setDatabaseName(dbFilePath);
        if (db.open()) {
            QSqlQuery pragmaQuery(db);
            pragmaQuery.exec("PRAGMA journal_mode=WAL;");
            pragmaQuery.exec("PRAGMA synchronous=FULL;");
            this->checkDatabase();
        } else {
            LogError("Adaptix Database did not opened: %s\n", db.lastError().text().toStdString().c_str());
        }
    }
}

Storage::~Storage()
{
    if (db.isOpen())
        db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(DB_CONNECTION_NAME);
}

void Storage::checkDatabase()
{
    auto queryProjects = QSqlQuery(QSqlDatabase::database(DB_CONNECTION_NAME));
    queryProjects.prepare("CREATE TABLE IF NOT EXISTS Projects ( "
            "project TEXT UNIQUE PRIMARY KEY, "
            "data TEXT );"
    );
    if ( !queryProjects.exec() )
        LogError("Table PROJECTS not created: %s\n", queryProjects.lastError().text().toStdString().c_str());



    auto queryExtensions = QSqlQuery(QSqlDatabase::database(DB_CONNECTION_NAME));
    queryExtensions.prepare("CREATE TABLE IF NOT EXISTS Extensions ( "
                          "filepath TEXT UNIQUE PRIMARY KEY, "
                          "enabled BOOLEAN );"
    );
    if ( !queryExtensions.exec() )
        LogError("Table EXTENSIONS not created: %s\n", queryExtensions.lastError().text().toStdString().c_str());



    auto querySettings = QSqlQuery(QSqlDatabase::database(DB_CONNECTION_NAME));
    querySettings.prepare("CREATE TABLE IF NOT EXISTS Settings ( "
                            "key TEXT UNIQUE PRIMARY KEY, "
                            "data TEXT );"
    );
    if ( !querySettings.exec() )
        LogError("Table Settings not created: %s\n", querySettings.lastError().text().toStdString().c_str());



    auto queryListenerProfiles = QSqlQuery(QSqlDatabase::database(DB_CONNECTION_NAME));
    queryListenerProfiles.prepare("CREATE TABLE IF NOT EXISTS ListenerProfiles ( "
                            "project TEXT, "
                            "name TEXT, "
                            "data TEXT, "
                            "PRIMARY KEY (project, name) );"
    );
    if ( !queryListenerProfiles.exec() )
        LogError("Table ListenerProfiles not created: %s\n", queryListenerProfiles.lastError().text().toStdString().c_str());

    auto queryAgentProfiles = QSqlQuery(QSqlDatabase::database(DB_CONNECTION_NAME));
    queryAgentProfiles.prepare("CREATE TABLE IF NOT EXISTS AgentProfiles ( "
                            "project TEXT, "
                            "name TEXT, "
                            "data TEXT, "
                            "PRIMARY KEY (project, name) );"
    );
    if ( !queryAgentProfiles.exec() )
        LogError("Table AgentProfiles not created: %s\n", queryAgentProfiles.lastError().text().toStdString().c_str());

    auto queryBuildProfiles = QSqlQuery(QSqlDatabase::database(DB_CONNECTION_NAME));
    queryBuildProfiles.prepare("CREATE TABLE IF NOT EXISTS BuildProfiles ( "
                            "name TEXT UNIQUE PRIMARY KEY, "
                            "data TEXT );"
    );
    if ( !queryBuildProfiles.exec() )
        LogError("Table BuildProfiles not created: %s\n", queryBuildProfiles.lastError().text().toStdString().c_str());
}

/// PROJECTS

QVector<AuthProfile> Storage::ListProjects()
{
    auto list = QVector<AuthProfile>();
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
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

            if (json.contains("subscriptions")) {
                QStringList subs;
                for (const auto &v : json["subscriptions"].toArray())
                    subs.append(v.toString());

                if (subs.contains("chat")) {
                    subs.removeAll("chat");
                    subs.append("chat_history");
                    subs.append("chat_realtime");
                    subs.append("chat_todo");
                }
                if (subs.contains("downloads")) {
                    subs.removeAll("downloads");
                    subs.append("downloads_history");
                    subs.append("downloads_realtime");
                }
                if (subs.contains("screenshots")) {
                    subs.removeAll("screenshots");
                    subs.append("screenshot_history");
                    subs.append("screenshot_realtime");
                }
                if (subs.contains("credentials")) {
                    subs.removeAll("credentials");
                    subs.append("credentials_history");
                    subs.append("credentials_realtime");
                }
                if (subs.contains("targets")) {
                    subs.removeAll("targets");
                    subs.append("targets_history");
                    subs.append("targets_realtime");
                }
                if (subs.contains("tasks_only_active")) {
                    subs.removeAll("tasks_only_active");
                    subs.append("tasks_only_jobs");
                }

                profile.SetSubscriptions(subs);
            } else {
                profile.SetSubscriptions({
                    "chat_history", "chat_realtime", "chat_todo",
                    "downloads_history", "downloads_realtime",
                    "screenshot_history", "screenshot_realtime",
                    "credentials_history", "credentials_realtime",
                    "targets_history", "targets_realtime",
                    "notifications", "tunnels",
                    "agents_only_active",
                    "console_history",
                    "tasks_history",
                    "tasks_manager"
                });
            }
            profile.SetConsoleMultiuser(json.value("consoleMultiuser").toBool(true));

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
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
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
    json["host"]       = profile.GetHost();
    json["port"]       = profile.GetPort();
    json["endpoint"]   = profile.GetEndpoint();
    json["username"]   = profile.GetUsername();
    json["password"]   = profile.GetPassword();
    json["projectDir"] = profile.GetProjectDir();
    json["subscriptions"] = QJsonArray::fromStringList(profile.GetSubscriptions());
    json["consoleMultiuser"] = profile.GetConsoleMultiuser();
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare( "INSERT INTO Projects (project, data) VALUES (:Project, :Data);");
    query.bindValue(":Project", profile.GetProject());
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("The project has not been added to the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::UpdateProject(AuthProfile profile)
{
    QJsonObject json;
    json["host"]       = profile.GetHost();
    json["port"]       = profile.GetPort();
    json["endpoint"]   = profile.GetEndpoint();
    json["username"]   = profile.GetUsername();
    json["password"]   = profile.GetPassword();
    json["projectDir"] = profile.GetProjectDir();
    json["subscriptions"] = QJsonArray::fromStringList(profile.GetSubscriptions());
    json["consoleMultiuser"] = profile.GetConsoleMultiuser();
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("UPDATE Projects SET data = :Data WHERE project = :Project;");
    query.bindValue(":Project", profile.GetProject());
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("The project has not been updated in the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveProject(const QString &project)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("DELETE FROM Projects WHERE project = :Project");
    query.bindValue(":Project", project);
    if (!query.exec())
        LogError("Failed to delete project from database: %s\n", query.lastError().text().toStdString().c_str());
}

/// EXTENSIONS

QVector<ExtensionFile> Storage::ListExtensions()
{
    auto list = QVector<ExtensionFile>();
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
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
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
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
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare( "INSERT INTO Extensions (filepath, enabled) VALUES (:Filepath, :Enabled);");
    query.bindValue(":Filepath", extFile.FilePath.toStdString().c_str());
    query.bindValue(":Enabled", extFile.Enabled);
    if (!query.exec())
        LogError("The extension has not been added to the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::UpdateExtension(const ExtensionFile &extFile)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare( "UPDATE Extensions SET enabled = :Enabled WHERE filepath = :Filepath;");
    query.bindValue(":Filepath", extFile.FilePath.toStdString().c_str());
    query.bindValue(":Enabled", extFile.Enabled);
    if (!query.exec())
        LogError("Extension not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveExtension(const QString &filepath)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("DELETE FROM Extensions WHERE filepath = :Filepath");
    query.bindValue(":Filepath", filepath);
    if (!query.exec())
        LogError("Failed to delete extension from database: %s\n", query.lastError().text().toStdString().c_str());
}

/// SETTINGS

void Storage::SelectSettingsMain(SettingsData* settingsData)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsMain' LIMIT 1;" );
    if ( query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        if (json.contains("theme") && !json["theme"].toString().isEmpty())
            settingsData->MainTheme   = json["theme"].toString();
        if (json.contains("fontFamily") && !json["fontFamily"].toString().isEmpty())
            settingsData->FontFamily  = json["fontFamily"].toString();
        if (json.contains("fontSize") && json["fontSize"].toInt() > 0)
            settingsData->FontSize    = json["fontSize"].toInt();
        if (json.contains("consoleTime"))
            settingsData->ConsoleTime = json["consoleTime"].toBool();
        if (json.contains("pageSize"))
            settingsData->PageSize    = qBound(5, json["pageSize"].toInt(100), 10000);
        if (json.contains("toolbarPosition"))
            settingsData->ToolbarPosition = qBound(0, json["toolbarPosition"].toInt(0), 3);
    }
}

void Storage::UpdateSettingsMain(const SettingsData &settingsData)
{
    QJsonObject json;
    json["theme"]       = settingsData.MainTheme;
    json["fontFamily"]  = settingsData.FontFamily;
    json["fontSize"]    = settingsData.FontSize;
    json["consoleTime"] = settingsData.ConsoleTime;
    json["pageSize"]    = settingsData.PageSize;
    json["toolbarPosition"] = settingsData.ToolbarPosition;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsMain', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsMain not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsConsole(SettingsData* settingsData)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsConsole' LIMIT 1;" );
    if ( query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        settingsData->RemoteTerminalBufferSize = json["terminalBuffer"].toInt();
        settingsData->ConsoleBufferSize        = json["consoleBuffer"].toInt();
        settingsData->ConsoleNoWrap            = json["noWrap"].toBool();
        settingsData->ConsoleAutoScroll        = json["autoScroll"].toBool();
        settingsData->ConsoleShowBackground    = json.contains("showBackground") ? json["showBackground"].toBool() : true;
        settingsData->ConsoleUseAppTheme       = json.contains("useAppTheme") ? json["useAppTheme"].toBool() : true;
        settingsData->ConsoleBgImagePath       = json.contains("bgImagePath") && !json["bgImagePath"].toString().isEmpty() ? json["bgImagePath"].toString() : ":/Back";
        settingsData->ConsoleBgDimming         = json.contains("bgDimming") ? qBound(0, json["bgDimming"].toInt(80), 100) : 80;
        if (json.contains("consoleTheme"))
            settingsData->ConsoleTheme = json["consoleTheme"].toString();
        settingsData->ConsoleAutoLoadEarlier = json.contains("autoLoadEarlier") ? json["autoLoadEarlier"].toBool() : true;
        settingsData->ConsolePageSize = json.contains("consolePageSize") ? qBound(10, json["consolePageSize"].toInt(50), 2000) : 50;
    }
}

void Storage::UpdateSettingsConsole(const SettingsData &settingsData)
{
    QJsonObject json;
    json["terminalBuffer"]  = settingsData.RemoteTerminalBufferSize;
    json["consoleBuffer"]   = settingsData.ConsoleBufferSize;
    json["noWrap"]          = settingsData.ConsoleNoWrap;
    json["autoScroll"]      = settingsData.ConsoleAutoScroll;
    json["showBackground"]  = settingsData.ConsoleShowBackground;
    json["useAppTheme"]     = settingsData.ConsoleUseAppTheme;
    json["bgImagePath"]     = settingsData.ConsoleBgImagePath;
    json["bgDimming"]       = settingsData.ConsoleBgDimming;
    json["consoleTheme"]    = settingsData.ConsoleTheme;
    json["autoLoadEarlier"] = settingsData.ConsoleAutoLoadEarlier;
    json["consolePageSize"] = settingsData.ConsolePageSize;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsConsole', :Data);");
    query.bindValue(":Data", data);
    if ( !query.exec() )
        LogError("SettingsConsole not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsSessions(SettingsData* settingsData)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsSessions' LIMIT 1;" );
    if ( query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        settingsData->CheckHealth  = json["healthCheck"].toBool();
        settingsData->HealthCoaf   = json["healthCoaf"].toDouble();
        settingsData->HealthOffset = json["healthOffset"].toInt();
        settingsData->DeadLightnessShift = json["deadLightnessShift"].toDouble();
        if (settingsData->DeadLightnessShift <= 0.0)
            settingsData->DeadLightnessShift = 0.15;

        QJsonArray columns = json["columns"].toArray();
        if (columns.size() == 16) {
            settingsData->SessionsTableColumns[0] = columns[11].toBool();
            for (int i = 0; i < 11; i++)
                settingsData->SessionsTableColumns[i + 1] = columns[i].toBool();
            settingsData->SessionsTableColumns[12] = true;
            for (int i = 12; i < 16; i++)
                settingsData->SessionsTableColumns[i + 1] = columns[i].toBool();
        } else if (columns.size() == 17) {
            const QString layout = json["columnsLayout"].toString();
            if (layout == "iconFirst") {
                for (int i = 0; i < 17; i++)
                    settingsData->SessionsTableColumns[i] = columns[i].toBool();
            } else {
                settingsData->SessionsTableColumns[0] = columns[12].toBool();
                for (int i = 0; i < 12; i++)
                    settingsData->SessionsTableColumns[i + 1] = columns[i].toBool();
                for (int i = 13; i < 17; i++)
                    settingsData->SessionsTableColumns[i] = columns[i].toBool();
            }
        }

        QJsonArray columnOrder = json["columnOrder"].toArray();
        if (columnOrder.size() == 17 && json["columnsLayout"].toString() == "iconFirst") {
            for (int i = 0; i < 17; i++)
                settingsData->SessionsColumnOrder[i] = columnOrder[i].toInt();
        } else {
            for (int i = 0; i < 17; i++)
                settingsData->SessionsColumnOrder[i] = i;
        }

        if (json.contains("viewMode"))
            settingsData->SessionsViewMode = json["viewMode"].toInt();
        settingsData->SessionsAutoHideInactive = json.contains("autoHideInactive") ? json["autoHideInactive"].toBool() : false;
        settingsData->SessionsCompactMode = json.contains("compactMode") ? json["compactMode"].toBool() : false;
    }
}

void Storage::UpdateSettingsSessions(const SettingsData &settingsData)
{
    QJsonArray columns;
    for (int i = 0 ; i < 17; i++)
        columns.append(settingsData.SessionsTableColumns[i]);

    QJsonArray columnOrder;
    for (int i = 0 ; i < 17; i++)
        columnOrder.append(settingsData.SessionsColumnOrder[i]);

    QJsonObject json;
    json["healthCheck"]   = settingsData.CheckHealth;
    json["healthCoaf"]    = settingsData.HealthCoaf;
    json["healthOffset"]  = settingsData.HealthOffset;
    json["deadLightnessShift"] = settingsData.DeadLightnessShift;
    json["columns"]       = columns;
    json["columnOrder"]   = columnOrder;
    json["columnsLayout"] = QStringLiteral("iconFirst");
    json["viewMode"]      = settingsData.SessionsViewMode;
    json["autoHideInactive"] = settingsData.SessionsAutoHideInactive;
    json["compactMode"]   = settingsData.SessionsCompactMode;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsSessions', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsSessions not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsGraph(SettingsData* settingsData)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsGraph' LIMIT 1;" );
    if ( query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject  json  = doc.object();

        settingsData->GraphVersion = json["version"].toString();
        settingsData->GraphAutoHideInactive = json.contains("autoHideInactive") ? json["autoHideInactive"].toBool() : true;
        settingsData->GraphAutoHideNoChilds = json.contains("autoHideNoChilds") ? json["autoHideNoChilds"].toBool() : false;
    }
}

void Storage::UpdateSettingsGraph(const SettingsData &settingsData)
{
    QJsonObject json;
    json["version"]          = settingsData.GraphVersion;
    json["autoHideInactive"] = settingsData.GraphAutoHideInactive;
    json["autoHideNoChilds"] = settingsData.GraphAutoHideNoChilds;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsGraph', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsGraph not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsTasks(SettingsData* settingsData)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsTasks' LIMIT 1;" );
    if ( query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        QJsonArray columns = json["columns"].toArray();
        for (int i = 0; i < 11 && i < columns.size(); i++)
            settingsData->TasksTableColumns[i] = columns[i].toBool();

        if (json.contains("viewMode"))
            settingsData->TasksViewMode = json["viewMode"].toInt();
        settingsData->TasksInProcessOnly = json.contains("inProcessOnly") ? json["inProcessOnly"].toBool() : false;
        settingsData->TasksCompactMode = json.contains("compactMode") ? json["compactMode"].toBool() : false;
    }
}

void Storage::UpdateSettingsTasks(const SettingsData &settingsData)
{
    QJsonArray columns;
    for (int i = 0 ; i < 11; i++)
        columns.append(settingsData.TasksTableColumns[i]);

    QJsonObject json;
    json["columns"] = columns;
    json["viewMode"] = settingsData.TasksViewMode;
    json["inProcessOnly"] = settingsData.TasksInProcessOnly;
    json["compactMode"] = settingsData.TasksCompactMode;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsTasks', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsTasks not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsTargets(SettingsData* settingsData)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsTargets' LIMIT 1;");
    if (query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        if (json.contains("viewMode"))
            settingsData->TargetsViewMode = json["viewMode"].toInt();

        QJsonArray columns = json["columns"].toArray();
        for (int i = 0; i < 10 && i < columns.size(); i++)
            settingsData->TargetsTableColumns[i] = columns[i].toBool();
        settingsData->TargetsCompactMode = json.contains("compactMode") ? json["compactMode"].toBool() : false;
    }
}

void Storage::UpdateSettingsTargets(const SettingsData &settingsData)
{
    QJsonArray columns;
    for (int i = 0; i < 10; i++)
        columns.append(settingsData.TargetsTableColumns[i]);

    QJsonObject json;
    json["viewMode"] = settingsData.TargetsViewMode;
    json["columns"]  = columns;
    json["compactMode"] = settingsData.TargetsCompactMode;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsTargets', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsTargets not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsCredentials(SettingsData* settingsData)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsCredentials' LIMIT 1;");
    if (query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        if (json.contains("viewMode"))
            settingsData->CredentialsViewMode = json["viewMode"].toInt();

        QJsonArray columns = json["columns"].toArray();
        for (int i = 0; i < 10 && i < columns.size(); i++)
            settingsData->CredentialsTableColumns[i] = columns[i].toBool();
        settingsData->CredentialsCompactMode = json.contains("compactMode") ? json["compactMode"].toBool() : false;
    }
}

void Storage::UpdateSettingsCredentials(const SettingsData &settingsData)
{
    QJsonArray columns;
    for (int i = 0; i < 10; i++)
        columns.append(settingsData.CredentialsTableColumns[i]);

    QJsonObject json;
    json["viewMode"] = settingsData.CredentialsViewMode;
    json["columns"]  = columns;
    json["compactMode"] = settingsData.CredentialsCompactMode;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsCredentials', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsCredentials not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsFiles(SettingsData* settingsData)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsFiles' LIMIT 1;");
    if (query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        QJsonArray columns = json["columns"].toArray();
        for (int i = 0; i < 11 && i < columns.size(); i++)
            settingsData->FilesTableColumns[i] = columns[i].toBool();
        settingsData->FilesCompactMode = json.contains("compactMode") ? json["compactMode"].toBool() : false;
    }
}

void Storage::UpdateSettingsFiles(const SettingsData &settingsData)
{
    QJsonArray columns;
    for (int i = 0; i < 11; i++)
        columns.append(settingsData.FilesTableColumns[i]);

    QJsonObject json;
    json["columns"] = columns;
    json["compactMode"] = settingsData.FilesCompactMode;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsFiles', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsFiles not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsPayloads(SettingsData* settingsData)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsPayloads' LIMIT 1;");
    if (query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        QJsonArray columns = json["columns"].toArray();
        for (int i = 0; i < 14 && i < columns.size(); i++)
            settingsData->PayloadsTableColumns[i] = columns[i].toBool();
    }
}

void Storage::UpdateSettingsPayloads(const SettingsData &settingsData)
{
    QJsonArray columns;
    for (int i = 0; i < 14; i++)
        columns.append(settingsData.PayloadsTableColumns[i]);

    QJsonObject json;
    json["columns"] = columns;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsPayloads', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsPayloads not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsTabBlink(SettingsData* settingsData)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
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

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsTablBlink', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsTablBlink not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

static QJsonObject policyToJson(const AxScriptPolicy& p)
{
    QJsonObject o;
    o["fileRead"]  = p.fileRead;
    o["fileWrite"] = p.fileWrite;
    o["process"]   = p.process;
    o["sandboxFs"] = p.sandboxFs;
    return o;
}

static void policyFromJson(const QJsonObject& o, AxScriptPolicy* p)
{
    if (!p) return;
    if (o.contains("fileRead"))  p->fileRead  = o.value("fileRead").toBool(p->fileRead);
    if (o.contains("fileWrite")) p->fileWrite = o.value("fileWrite").toBool(p->fileWrite);
    if (o.contains("process"))   p->process   = o.value("process").toBool(p->process);
    if (o.contains("sandboxFs")) p->sandboxFs = o.value("sandboxFs").toBool(p->sandboxFs);
}

void Storage::SelectSettingsScript(SettingsData* settingsData)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsScript' LIMIT 1;");
    if (query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        policyFromJson(json.value("server").toObject(), &settingsData->ScriptServer);
        policyFromJson(json.value("local").toObject(), &settingsData->ScriptLocal);
        policyFromJson(json.value("editor").toObject(), &settingsData->ScriptEditor);
        policyFromJson(json.value("editorAction").toObject(), &settingsData->ScriptEditorAction);
        if (json.contains("sandboxDir"))
            settingsData->ScriptSandboxDir = json.value("sandboxDir").toString();
    }
}

void Storage::UpdateSettingsScript(const SettingsData &settingsData)
{
    QJsonObject json;
    json["server"]       = policyToJson(settingsData.ScriptServer);
    json["local"]        = policyToJson(settingsData.ScriptLocal);
    json["editor"]       = policyToJson(settingsData.ScriptEditor);
    json["editorAction"] = policyToJson(settingsData.ScriptEditorAction);
    json["sandboxDir"]   = settingsData.ScriptSandboxDir;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsScript', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsScript not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::SelectSettingsDockLayout(SettingsData* settingsData)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM Settings WHERE key = 'SettingsDockLayout' LIMIT 1;");
    if (query.exec() && query.next()) {
        QString       data = query.value("data").toString();
        QJsonDocument doc  = QJsonDocument::fromJson(data.toUtf8());
        QJsonObject   json = doc.object();

        if (json.contains("layout") && !json["layout"].toString().isEmpty())
            settingsData->DockLayout.layout = json["layout"].toString();

        settingsData->DockLayout.openIn.clear();
        if (json.contains("openIn") && json["openIn"].isObject()) {
            QJsonObject openIn = json["openIn"].toObject();
            for (auto it = openIn.begin(); it != openIn.end(); ++it)
                settingsData->DockLayout.openIn.insert(it.key(), it.value().toString());
        }

        settingsData->DockLayout.startup.clear();
        if (json.contains("startup") && json["startup"].isArray()) {
            for (const QJsonValue& v : json["startup"].toArray())
                settingsData->DockLayout.startup.append(v.toString());
        }
    }
}

void Storage::UpdateSettingsDockLayout(const SettingsData &settingsData)
{
    QJsonObject json;
    json["layout"] = settingsData.DockLayout.layout;
    QJsonObject openIn;
    for (auto it = settingsData.DockLayout.openIn.constBegin(); it != settingsData.DockLayout.openIn.constEnd(); ++it)
        openIn[it.key()] = it.value();
    json["openIn"] = openIn;
    QJsonArray startup;
    for (const QString& id : settingsData.DockLayout.startup)
        startup.append(id);
    json["startup"] = startup;
    QString data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO Settings (key, data) VALUES ('SettingsDockLayout', :Data);");
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("SettingsDockLayout not updated in database: %s\n", query.lastError().text().toStdString().c_str());
}

/// LISTENER PROFILES

QVector<QPair<QString, QString>> Storage::ListListenerProfiles(const QString &project)
{
    auto list = QVector<QPair<QString, QString>>();
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT name, data FROM ListenerProfiles WHERE project = :Project;");
    query.bindValue(":Project", project);
    if (query.exec()) {
        while (query.next()) {
            QString name = query.value("name").toString();
            QString data = query.value("data").toString();
            list.push_back(QPair<QString, QString>(name, data));
        }
    }
    else {
        LogError("Failed to query listener profiles from database: %s\n", query.lastError().text().toStdString().c_str());
    }
    return list;
}

void Storage::AddListenerProfile(const QString &project, const QString &name, const QString &data)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO ListenerProfiles (project, name, data) VALUES (:Project, :Name, :Data);");
    query.bindValue(":Project", project);
    query.bindValue(":Name", name);
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("The listener profile has not been added to the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveListenerProfile(const QString &project, const QString &name)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("DELETE FROM ListenerProfiles WHERE project = :Project AND name = :Name;");
    query.bindValue(":Project", project);
    query.bindValue(":Name", name);
    if (!query.exec())
        LogError("The listener profile has not been removed from the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveAllListenerProfiles(const QString &project)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("DELETE FROM ListenerProfiles WHERE project = :Project;");
    query.bindValue(":Project", project);
    if (!query.exec())
        LogError("Listener profiles have not been removed from the database: %s\n", query.lastError().text().toStdString().c_str());
}

QString Storage::GetListenerProfile(const QString &project, const QString &name)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM ListenerProfiles WHERE project = :Project AND name = :Name LIMIT 1;");
    query.bindValue(":Project", project);
    query.bindValue(":Name", name);
    if (query.exec() && query.next()) {
        return query.value("data").toString();
    }
    return QString();
}

/// AGENT PROFILES

QVector<QPair<QString, QString>> Storage::ListAgentProfiles(const QString &project)
{
    auto list = QVector<QPair<QString, QString>>();
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT name, data FROM AgentProfiles WHERE project = :Project;");
    query.bindValue(":Project", project);
    if (query.exec()) {
        while (query.next()) {
            QString name = query.value("name").toString();
            QString data = query.value("data").toString();
            list.push_back(QPair<QString, QString>(name, data));
        }
    }
    else {
        LogError("Failed to query agent profiles from database: %s\n", query.lastError().text().toStdString().c_str());
    }
    return list;
}

void Storage::AddAgentProfile(const QString &project, const QString &name, const QString &data)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO AgentProfiles (project, name, data) VALUES (:Project, :Name, :Data);");
    query.bindValue(":Project", project);
    query.bindValue(":Name", name);
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("The agent profile has not been added to the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveAgentProfile(const QString &project, const QString &name)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("DELETE FROM AgentProfiles WHERE project = :Project AND name = :Name;");
    query.bindValue(":Project", project);
    query.bindValue(":Name", name);
    if (!query.exec())
        LogError("The agent profile has not been removed from the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveAllAgentProfiles(const QString &project)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("DELETE FROM AgentProfiles WHERE project = :Project;");
    query.bindValue(":Project", project);
    if (!query.exec())
        LogError("Agent profiles have not been removed from the database: %s\n", query.lastError().text().toStdString().c_str());
}

QString Storage::GetAgentProfile(const QString &project, const QString &name)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT data FROM AgentProfiles WHERE project = :Project AND name = :Name LIMIT 1;");
    query.bindValue(":Project", project);
    query.bindValue(":Name", name);
    if (query.exec() && query.next()) {
        return query.value("data").toString();
    }
    return QString();
}

QVector<QPair<QString, QString>> Storage::ListBuildProfiles()
{
    QVector<QPair<QString, QString>> list;
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT name, data FROM BuildProfiles ORDER BY rowid;");
    if (query.exec()) {
        while (query.next()) {
            list.append({query.value("name").toString(),
                         query.value("data").toString()});
        }
    }
    return list;
}

void Storage::AddBuildProfile(const QString &name, const QString &data)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("INSERT OR REPLACE INTO BuildProfiles (name, data) VALUES (:Name, :Data);");
    query.bindValue(":Name", name);
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("AddBuildProfile failed: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveBuildProfile(const QString &name)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("DELETE FROM BuildProfiles WHERE name = :Name;");
    query.bindValue(":Name", name);
    if (!query.exec())
        LogError("RemoveBuildProfile failed: %s\n", query.lastError().text().toStdString().c_str());
}

bool Storage::ExistsBuildProfile(const QString &name)
{
    QSqlQuery query(QSqlDatabase::database(DB_CONNECTION_NAME));
    query.prepare("SELECT 1 FROM BuildProfiles WHERE name = :Name LIMIT 1;");
    query.bindValue(":Name", name);
    return query.exec() && query.next();
}