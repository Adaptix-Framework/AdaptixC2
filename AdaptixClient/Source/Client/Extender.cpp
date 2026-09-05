#include <UI/MainUI.h>
#include <Client/Extender.h>
#include <Client/Storage.h>
#include <MainAdaptix.h>

#include <QJSEngine>
#include <QJSValue>
#include <QFile>
#include <QTextStream>

static void parseMetadata(const QString& code, ExtensionFile* ext)
{
    if (code.isEmpty())
        return;

    QJSEngine js;
    js.evaluate(code);

    QJSValue metadata = js.globalObject().property("metadata");
    if (!metadata.isObject())
        return;

    QString name = metadata.property("name").toString();
    if (!name.isEmpty())
        ext->Name = name;

    QString desc = metadata.property("description").toString();
    if (!desc.isEmpty())
        ext->Description = desc;

    ext->NoSave = metadata.property("nosave").toBool();
}

Extender::Extender(MainAdaptix* m)
{
    mainAdaptix = m;
    this->LoadFromDB();
}

Extender::~Extender() = default;

void Extender::LoadFromDB()
{
    auto list = mainAdaptix->storage->ListExtensions();
    for(auto ext : list) {
        if (ext.FilePath.startsWith("__server__:")) {
            mainAdaptix->storage->RemoveExtension(ext.FilePath);
            continue;
        }

        QFile file(ext.FilePath);
        if (!file.open(QIODevice::ReadOnly)) {
            ext.Enabled = false;
            ext.Message = "Cannot open file.";
            extenderFiles[ext.FilePath] = ext;
            continue;
        }
        ext.Code = QTextStream(&file).readAll();
        file.close();

        parseMetadata(ext.Code, &ext);

        extenderFiles[ext.FilePath] = ext;
    }

    Q_EMIT extensionChanged();
}

void Extender::LoadFromFile(const QString &path, const bool enabled)
{
    ExtensionFile extensionFile = {};
    extensionFile.FilePath = path;
    extensionFile.Enabled  = enabled;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        extensionFile.Enabled = false;
        extensionFile.Message = "Cannot open file.";
    } else {
        extensionFile.Code = QTextStream(&file).readAll();
        file.close();
    }

    this->SetExtension(extensionFile);
}

bool Extender::IsLoaded(const QString &path) const
{
    return extenderFiles.contains(path) && extenderFiles[path].Enabled;
}

void Extender::SetExtension(ExtensionFile extFile)
{
    parseMetadata(extFile.Code, &extFile);

    const bool existed = extenderFiles.contains(extFile.FilePath);

    if (existed || extFile.Enabled)
        mainAdaptix->mainUI->RemoveExtension(extFile);

    if (extFile.Enabled) {
        bool success = mainAdaptix->mainUI->AddNewExtension(&extFile);
        if (!success)
            mainAdaptix->mainUI->RemoveExtension(extFile);
    }

    if (!extFile.NoSave) {
        extenderFiles[extFile.FilePath] = extFile;
        if (mainAdaptix->storage->ExistsExtension(extFile.FilePath))
            mainAdaptix->storage->UpdateExtension(extFile);
        else
            mainAdaptix->storage->AddExtension(extFile);
    } else if (existed) {
        extenderFiles[extFile.FilePath] = extFile;
    }

    Q_EMIT extensionChanged();
}

void Extender::EnableExtension(const QString &path)
{
    if (!extenderFiles.contains(path))
        return;

    LoadFromFile(path, true);
}

void Extender::DisableExtension(const QString &path)
{
    if( !extenderFiles.contains(path) )
        return;

    if( extenderFiles[path].Enabled ) {
        extenderFiles[path].Enabled = false;
        mainAdaptix->mainUI->RemoveExtension(extenderFiles[path]);
        mainAdaptix->storage->UpdateExtension(extenderFiles[path]);
        Q_EMIT extensionChanged();
    }
}

void Extender::RemoveExtension(const QString &path)
{
    if( !extenderFiles.contains(path) )
        return;

    mainAdaptix->mainUI->RemoveExtension(extenderFiles[path]);
    mainAdaptix->storage->RemoveExtension(path);

    extenderFiles.remove(path);
    Q_EMIT extensionChanged();
}

void Extender::syncedOnReload(const QString &project)
{
    for (auto path : extenderFiles.keys()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            extenderFiles[path].Code = QTextStream(&file).readAll();
            file.close();
            parseMetadata(extenderFiles[path].Code, &extenderFiles[path]);
        }

        if(extenderFiles[path].Enabled) {
            bool success = mainAdaptix->mainUI->SyncExtension(project, &(extenderFiles[path]));
            if (!success) {
                mainAdaptix->mainUI->RemoveExtension(extenderFiles[path]);
            }
        }
        mainAdaptix->storage->UpdateExtension(extenderFiles[path]);
    }

    Q_EMIT extensionChanged();
}

void Extender::loadGlobalScript(const QString &path) { this->LoadFromFile(path, true); }

void Extender::unloadGlobalScript(const QString &path) { this->RemoveExtension(path); }
