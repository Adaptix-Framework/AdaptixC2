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



    auto queryListenerProfiles = QSqlQuery();
    queryListenerProfiles.prepare("CREATE TABLE IF NOT EXISTS ListenerProfiles ( "
                            "project TEXT, "
                            "name TEXT, "
                            "data TEXT, "
                            "PRIMARY KEY (project, name) );"
    );
    if ( !queryListenerProfiles.exec() )
        LogError("Table ListenerProfiles not created: %s\n", queryListenerProfiles.lastError().text().toStdString().c_str());

    auto queryAgentProfiles = QSqlQuery();
    queryAgentProfiles.prepare("CREATE TABLE IF NOT EXISTS AgentProfiles ( "
                            "project TEXT, "
                            "name TEXT, "
                            "data TEXT, "
                            "PRIMARY KEY (project, name) );"
    );
    if ( !queryAgentProfiles.exec() )
        LogError("Table AgentProfiles not created: %s\n", queryAgentProfiles.lastError().text().toStdString().c_str());
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

            if (json.contains("subscriptions")) {
                QStringList subs;
                for (const auto &v : json["subscriptions"].toArray())
                    subs.append(v.toString());

                if (subs.contains("chat")) {
                    subs.removeAll("chat");
                    subs.append("chat_history");
                    subs.append("chat_realtime");
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
                    "chat_history", "chat_realtime",
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
    json["host"]       = profile.GetHost();
    json["port"]       = profile.GetPort();
    json["endpoint"]   = profile.GetEndpoint();
    json["username"]   = profile.GetUsername();
    json["password"]   = profile.GetPassword();
    json["projectDir"] = profile.GetProjectDir();
    json["subscriptions"] = QJsonArray::fromStringList(profile.GetSubscriptions());
    json["consoleMultiuser"] = profile.GetConsoleMultiuser();
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
    json["host"]       = profile.GetHost();
    json["port"]       = profile.GetPort();
    json["endpoint"]   = profile.GetEndpoint();
    json["username"]   = profile.GetUsername();
    json["password"]   = profile.GetPassword();
    json["projectDir"] = profile.GetProjectDir();
    json["subscriptions"] = QJsonArray::fromStringList(profile.GetSubscriptions());
    json["consoleMultiuser"] = profile.GetConsoleMultiuser();
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
        for (int i = 0; i < 16 && i < columns.size(); i++)
            settingsData->SessionsTableColumns[i] = columns[i].toBool();

        QJsonArray columnOrder = json["columnOrder"].toArray();
        for (int i = 0; i < 16 && i < columnOrder.size(); i++)
            settingsData->SessionsColumnOrder[i] = columnOrder[i].toInt();
    }
}

void Storage::UpdateSettingsSessions(const SettingsData &settingsData)
{
    QJsonArray columns;
    for (int i = 0 ; i < 16; i++)
        columns.append(settingsData.SessionsTableColumns[i]);

    QJsonArray columnOrder;
    for (int i = 0 ; i < 16; i++)
        columnOrder.append(settingsData.SessionsColumnOrder[i]);

    QJsonObject json;
    json["healthCheck"]   = settingsData.CheckHealth;
    json["healthCoaf"]    = settingsData.HealthCoaf;
    json["healthOffset"]  = settingsData.HealthOffset;
    json["columns"]       = columns;
    json["columnOrder"]   = columnOrder;
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



/// LISTENER PROFILES

QVector<QPair<QString, QString>> Storage::ListListenerProfiles(const QString &project)
{
    auto list = QVector<QPair<QString, QString>>();
    QSqlQuery query;
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
    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO ListenerProfiles (project, name, data) VALUES (:Project, :Name, :Data);");
    query.bindValue(":Project", project);
    query.bindValue(":Name", name);
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("The listener profile has not been added to the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveListenerProfile(const QString &project, const QString &name)
{
    QSqlQuery query;
    query.prepare("DELETE FROM ListenerProfiles WHERE project = :Project AND name = :Name;");
    query.bindValue(":Project", project);
    query.bindValue(":Name", name);
    if (!query.exec())
        LogError("The listener profile has not been removed from the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveAllListenerProfiles(const QString &project)
{
    QSqlQuery query;
    query.prepare("DELETE FROM ListenerProfiles WHERE project = :Project;");
    query.bindValue(":Project", project);
    if (!query.exec())
        LogError("Listener profiles have not been removed from the database: %s\n", query.lastError().text().toStdString().c_str());
}

QString Storage::GetListenerProfile(const QString &project, const QString &name)
{
    QSqlQuery query;
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
    QSqlQuery query;
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
    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO AgentProfiles (project, name, data) VALUES (:Project, :Name, :Data);");
    query.bindValue(":Project", project);
    query.bindValue(":Name", name);
    query.bindValue(":Data", data);
    if (!query.exec())
        LogError("The agent profile has not been added to the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveAgentProfile(const QString &project, const QString &name)
{
    QSqlQuery query;
    query.prepare("DELETE FROM AgentProfiles WHERE project = :Project AND name = :Name;");
    query.bindValue(":Project", project);
    query.bindValue(":Name", name);
    if (!query.exec())
        LogError("The agent profile has not been removed from the database: %s\n", query.lastError().text().toStdString().c_str());
}

void Storage::RemoveAllAgentProfiles(const QString &project)
{
    QSqlQuery query;
    query.prepare("DELETE FROM AgentProfiles WHERE project = :Project;");
    query.bindValue(":Project", project);
    if (!query.exec())
        LogError("Agent profiles have not been removed from the database: %s\n", query.lastError().text().toStdString().c_str());
}

QString Storage::GetAgentProfile(const QString &project, const QString &name)
{
    QSqlQuery query;
    query.prepare("SELECT data FROM AgentProfiles WHERE project = :Project AND name = :Name LIMIT 1;");
    query.bindValue(":Project", project);
    query.bindValue(":Name", name);
    if (query.exec() && query.next()) {
        return query.value("data").toString();
    }
    return QString();
}