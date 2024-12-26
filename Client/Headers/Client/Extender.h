#ifndef ADAPTIXCLIENT_EXTENDER_H
#define ADAPTIXCLIENT_EXTENDER_H

#include <main.h>
#include <UI/Dialogs/DialogExtender.h>
#include <MainAdaptix.h>

class DialogExtender;
class MainAdaptix;

class Extender
{
    MainAdaptix* mainAdaptix = nullptr;

public:
    Extender(MainAdaptix* m);
    ~Extender();

    DialogExtender* dialogExtender = nullptr;
    QMap<QString, ExtensionFile> extenderFiles;

    void LoadFromDB();
    void LoadFromFile(QString path, bool enabled);
    void SetExtension(ExtensionFile extFile );
    void EnableExtension(QString path);
    void DisableExtension(QString path);
    void RemoveExtension(QString path);
};

#endif //ADAPTIXCLIENT_EXTENDER_H
