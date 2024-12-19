#include <Client/Extender.h>

Extender::Extender(MainAdaptix* m)
{
    mainAdaptix = m;
    dialogExtender = new DialogExtender(this);
}

Extender::~Extender() = default;

void Extender::LoadFromFile(QString path)
{
    QString       fileContent;
    QJsonObject   rootObj;
    QJsonDocument jsonDocument;
    QJsonArray    extensionsArray;
    QJsonArray    agentsArray;
    QStringList   agentsList;
    QMap<QString, QVector<QJsonObject> > exCommands;

    ExtensionFile extensionFile = {0};
    extensionFile.FilePath = path;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        extensionFile.Comment = "File not readed";
        extensionFile.Enabled = false;
        goto END;
    }
    fileContent = QString(file.readAll());
    file.close();

    jsonDocument = QJsonDocument::fromJson(fileContent.toUtf8());
    if ( jsonDocument.isNull() || !jsonDocument.isObject()) {
        extensionFile.Comment = "Invalid JSON document!";
        extensionFile.Enabled = false;
        goto END;
    }

    rootObj = jsonDocument.object();
    if( !rootObj.contains("name") && rootObj["name"].isString() ) {
        extensionFile.Comment = "JSON document must include a required 'name' parameter";
        extensionFile.Enabled = false;
        goto END;
    }

    if( !rootObj.contains("extensions") && rootObj["extensions"].isArray() ) {
        extensionFile.Comment = "JSON document must include a required 'extensions' parameter";
        extensionFile.Enabled = false;
        goto END;
    }

    extensionFile.Name = rootObj.value("name").toString();
    extensionFile.Description = rootObj.value("description").toString();

    extensionsArray = rootObj.value("extensions").toArray();
    for (QJsonValue extensionValue : extensionsArray) {
        QJsonObject extJsonObject = extensionValue.toObject();

        if( !extJsonObject.contains("type") && extJsonObject["type"].isString() ) {
            extensionFile.Comment = "Extension must include a required 'type' parameter";
            extensionFile.Enabled = false;
            goto END;
        }

        QString type = extJsonObject.value("type").toString();
        if(type == "command") {

            if( !extJsonObject.contains("agents") && extJsonObject["agents"].isArray() ) {
                extensionFile.Comment = "Extension must include a required 'agents' parameter";
                extensionFile.Enabled = false;
                goto END;
            }

            agentsArray = extJsonObject.value("agents").toArray();
            for (QJsonValue agentStr : agentsArray) {
                agentsList.push_back(agentStr.toString());
            }

            // Commander validate

            for(QString key : agentsList) {
                exCommands[key].push_back(extJsonObject);
            }

        } else {
            extensionFile.Comment = "Unknown extension type";
            extensionFile.Enabled = false;
            goto END;
        }
    }

    extensionFile.Comment    = fileContent;
    extensionFile.ExCommands = exCommands;
    extensionFile.Enabled    = true;

END:
    this->SetExtension(extensionFile);
}

void Extender::SetExtension(ExtensionFile extFile)
{
    if(extenderFiles.contains(extFile.FilePath)) {
        dialogExtender->UpdateExtenderItem(extFile);
    }
    else {
        extenderFiles[extFile.FilePath] = extFile;
        dialogExtender->AddExtenderItem(extFile);
//        mainAdaptix->mainUI->

    }
}

void Extender::EnableExtension(QString path)
{

}

void Extender::DisableExtension(QString path)
{

}

void Extender::DeleteExtension(QString path)
{

}