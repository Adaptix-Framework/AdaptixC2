#include <UI/MainUI.h>
#include <UI/Dialogs/DialogExtender.h>
#include <Client/Extender.h>
#include <Client/Storage.h>
#include <MainAdaptix.h>

Extender::Extender(MainAdaptix* m)
{
    mainAdaptix = m;
    dialogExtender = new DialogExtender(this);
}

Extender::~Extender() = default;

void Extender::LoadFromDB()
{
    auto list = mainAdaptix->storage->ListExtensions();
    for(int i=0; i < list.size(); i++)
        this->LoadFromFile( list[i].FilePath, list[i].Enabled );
}

void Extender::LoadFromFile(const QString &path, const bool enabled)
{
    if (extenderFiles.contains(path))
        return;

    ExtensionFile extensionFile = {};
    extensionFile.FilePath = path;
    extensionFile.Enabled  = enabled;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        extensionFile.Enabled = false;
        extensionFile.Message = "Cannot open file.";
        goto END;
    }
    extensionFile.Code = QTextStream(&file).readAll();
    file.close();

END:
    this->SetExtension(extensionFile);
}

void Extender::SetExtension(ExtensionFile extFile)
{
    bool result = false;

    if(extenderFiles.contains(extFile.FilePath)) {
        if(extFile.Enabled) {
            mainAdaptix->mainUI->RemoveExtension(extFile);
            bool success = mainAdaptix->mainUI->AddNewExtension(&extFile);
            if (!success) {
                mainAdaptix->mainUI->RemoveExtension(extFile);
            }
        }
        dialogExtender->UpdateExtenderItem(extFile);
        mainAdaptix->storage->UpdateExtension(extFile);
    }
    else {
        if( extFile.Enabled ) {
            bool success = mainAdaptix->mainUI->AddNewExtension(&extFile);
            if (!success) {
                mainAdaptix->mainUI->RemoveExtension(extFile);
            }
        }

        if (!extFile.NoSave) {
            extenderFiles[extFile.FilePath] = extFile;
            dialogExtender->AddExtenderItem(extFile);
            if( !mainAdaptix->storage->ExistsExtension(extFile.FilePath))
                mainAdaptix->storage->AddExtension(extFile);
        }
    }
}

void Extender::EnableExtension(const QString &path)
{
    if( !extenderFiles.contains(path) )
        return;

    if( !extenderFiles[path].Enabled && extenderFiles[path].Message.isEmpty()) {
        extenderFiles[path].Enabled = true;
        bool success = mainAdaptix->mainUI->AddNewExtension(&(extenderFiles[path]));
        if (!success) {
            mainAdaptix->mainUI->RemoveExtension(extenderFiles[path]);
        }

        dialogExtender->UpdateExtenderItem(extenderFiles[path]);
        mainAdaptix->storage->UpdateExtension(extenderFiles[path]);
    }
}

void Extender::DisableExtension(const QString &path)
{
    if( !extenderFiles.contains(path) )
        return;

    if( extenderFiles[path].Enabled ) {
        extenderFiles[path].Enabled = false;
        mainAdaptix->mainUI->RemoveExtension(extenderFiles[path]);
        dialogExtender->UpdateExtenderItem(extenderFiles[path]);
        mainAdaptix->storage->UpdateExtension(extenderFiles[path]);
    }
}

void Extender::RemoveExtension(const QString &path)
{
    if( !extenderFiles.contains(path) )
        return;

    mainAdaptix->mainUI->RemoveExtension(extenderFiles[path]);
    dialogExtender->RemoveExtenderItem(extenderFiles[path]);
    mainAdaptix->storage->RemoveExtension(path);

    extenderFiles.remove(path);
}

void Extender::syncedOnReload(QString project) { this->LoadFromDB(); }

void Extender::loadGlobalScript(const QString &path) { this->LoadFromFile(path, true); }

void Extender::unloadGlobalScript(const QString &path) { this->RemoveExtension(path); }
