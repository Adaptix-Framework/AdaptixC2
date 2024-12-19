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

    QMap<QString, ExtensionFile> extenderFiles;

public:
    Extender(MainAdaptix* mainAdaptix);
    ~Extender();

    DialogExtender* dialogExtender = nullptr;

    void LoadFromFile(QString path);
    void SetExtension(ExtensionFile extFile );
    void EnableExtension(QString path);
    void DisableExtension(QString path);
    void DeleteExtension(QString path);
};

#endif //ADAPTIXCLIENT_EXTENDER_H
