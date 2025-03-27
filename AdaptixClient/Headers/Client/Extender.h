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
    explicit Extender(MainAdaptix* m);
    ~Extender();

    DialogExtender* dialogExtender = nullptr;
    QMap<QString, ExtensionFile> extenderFiles;

    void LoadFromDB();
    void LoadFromFile(QString path, bool enabled);
    void SetExtension(const ExtensionFile &extFile );
    void EnableExtension(const QString &path);
    void DisableExtension(const QString &path);
    void RemoveExtension(const QString &path);
};

#endif //ADAPTIXCLIENT_EXTENDER_H
