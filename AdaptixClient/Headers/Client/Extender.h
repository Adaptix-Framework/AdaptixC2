#ifndef ADAPTIXCLIENT_EXTENDER_H
#define ADAPTIXCLIENT_EXTENDER_H

#include <main.h>

class MainAdaptix;

class Extender : public QObject
{
Q_OBJECT
    MainAdaptix* mainAdaptix = nullptr;

public:
    explicit Extender(MainAdaptix* m);
    ~Extender() override;

    QMap<QString, ExtensionFile> extenderFiles;

    void LoadFromDB();
    void LoadFromFile(const QString &path, bool enabled);
    bool IsLoaded(const QString &path) const;
    void SetExtension(ExtensionFile extFile );
    void EnableExtension(const QString &path);
    void DisableExtension(const QString &path);
    void RemoveExtension(const QString &path);

Q_SIGNALS:
    void extensionChanged();

public Q_SLOTS:
    void syncedOnReload(const QString &project);
    void loadGlobalScript(const QString &path);
    void unloadGlobalScript(const QString &path);
};

#endif
