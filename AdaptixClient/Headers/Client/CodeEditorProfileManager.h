#ifndef ADAPTIXCLIENT_CODEEDITORPROFILEMANAGER_H
#define ADAPTIXCLIENT_CODEEDITORPROFILEMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QVector>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>

enum BuildProfileType {
    ProfileAxScript = 0,
    ProfileBOF      = 1,
    ProfileCustom   = 2
};

struct BuildProfileParam
{
    QString name;
    QString type;
    QString value;
};

struct BuildProfile
{
    QString name;
    QString buildCommand;
    QString runCommand;
    QString defines;
    bool    mainEngine  = false;
    QVector<BuildProfileParam> params;

    static BuildProfileType typeForName(const QString& name)
    {
        if (name == QLatin1String("AxScript"))
            return ProfileAxScript;
        if (name == QLatin1String("BOF-C") || name == QLatin1String("BOF-CPP"))
            return ProfileBOF;
        return ProfileCustom;
    }

    BuildProfileType profileType() const { return typeForName(name); }
    bool isSystem() const { return profileType() != ProfileCustom; }

    QJsonObject toJson() const;
    static BuildProfile fromJson(const QJsonObject& o);

    static BuildProfile customProfile(const QString& name);
    static BuildProfile axScriptProfile();
    static BuildProfile bofCProfile();
    static BuildProfile bofCppProfile();
    static QString profileNameForFile(const QString& filePath);
};



class CodeEditorProfileManager : public QObject
{
Q_OBJECT
    QList<BuildProfile> m_profiles;
    QString m_currentName;

    void ensureDefaultExists();
    void saveCurrentName() const;

public:
    explicit CodeEditorProfileManager(QObject* parent = nullptr);

    void load();
    void save() const;

    QList<BuildProfile> profiles() const { return m_profiles; }
    QStringList profileNames() const;
    const BuildProfile* current() const;
    QString currentName() const { return m_currentName; }

    bool setCurrent(const QString& name);
    void addProfile(const QString& name);
    bool removeProfile(const QString& name);
    bool renameCurrent(const QString& newName);
    void updateCurrent(const BuildProfile& p);

Q_SIGNALS:
    void profilesChanged();
    void currentChanged(const BuildProfile& p);
};

#endif
