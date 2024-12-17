#ifndef ADAPTIXCLIENT_EXTENDER_H
#define ADAPTIXCLIENT_EXTENDER_H

#include <main.h>
#include <UI/Dialogs/DialogExtender.h>

class DialogExtender;

class Extender
{

public:
    Extender();
    ~Extender();

    DialogExtender* dialogExtender = nullptr;

    void LoadFromFile(QString path);
    void NewExtension(ExtensionFile extFile );
    void EnableExtension(QString path);
    void DisableExtension(QString path);
    void DeleteExtension(QString path);
};

#endif //ADAPTIXCLIENT_EXTENDER_H
