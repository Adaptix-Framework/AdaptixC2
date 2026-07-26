#ifndef ADAPTIXCLIENT_CODEEDITORPROFILEMANAGER_H
#define ADAPTIXCLIENT_CODEEDITORPROFILEMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

enum BuildProfileType {
    ProfileAxScript      = 0,
    ProfileBOF           = 1,
    ProfileEventHandler  = 2,
    ProfileCustom        = 3
};

enum class BuildProfileOrigin {
    System  = 0,
    User    = 1,
    Managed = 2
};

struct BuildProfileToolbar
{
    bool newFile    = true;
    bool openFile   = true;
    bool openFolder = true;
    bool save       = true;

    bool explorer = true;
    bool buildLog = true;
    bool minimap  = true;
    bool wordWrap = true;

    QString panel = QStringLiteral("axscript");

    static BuildProfileToolbar defaults();

    QJsonObject toJson() const;
    static BuildProfileToolbar fromJson(const QJsonObject& o, const BuildProfileToolbar& fallback);
};

struct BuildProfileAction
{
    QString id;
    QString label;
    QString icon;
    QString script;

    static BuildProfileAction make(const QString& id, const QString& label, const QString& icon, const QString& script);
    static QStringList toolbarIconPaths();

    QJsonObject toJson() const;
    static BuildProfileAction fromJson(const QJsonObject& o);
};

inline constexpr int kBuildProfileSchemaVersion = 9;

struct BuildProfile
{
    QString id;
    QString name;
    BuildProfileOrigin origin = BuildProfileOrigin::User;
    bool persist = true;
    QString contentHash;

    QString language = QStringLiteral("plain");
    BuildProfileToolbar toolbar;
    QString panelScript;
    QJsonObject panelState;
    QVector<BuildProfileAction> customActions;
    int schemaVersion = kBuildProfileSchemaVersion;

    void applyCurrentDefaults(bool preservePanelState = true);

    QString computeContentHash() const;
    void refreshContentHash() { contentHash = computeContentHash(); }

    static QString originToString(BuildProfileOrigin o);
    static BuildProfileOrigin originFromString(const QString& s);

    static QString defaultAxScriptPanelScript();
    static QString defaultBofPanelScript();
    static QString defaultEventHandlerPanelScript();
    static QString defaultCustomPanelScript();
    static QVector<BuildProfileAction> defaultAxScriptActions();
    static QVector<BuildProfileAction> defaultBofActions();
    static QVector<BuildProfileAction> defaultEventHandlerActions();

    static QString sanitizeIdSegment(const QString& s);
    static QString makeUserId(const QString& nameHint);

    static BuildProfileType typeForName(const QString& name)
    {
        if (name == QLatin1String("AxScript"))
            return ProfileAxScript;
        if (name == QLatin1String("BOF"))
            return ProfileBOF;
        if (name == QLatin1String("Event Handler"))
            return ProfileEventHandler;
        return ProfileCustom;
    }

    BuildProfileType profileType() const { return typeForName(name); }
    bool isSystem() const {
        return origin == BuildProfileOrigin::System || profileType() == ProfileAxScript || profileType() == ProfileBOF || profileType() == ProfileEventHandler;
    }
    bool isManaged() const { return origin == BuildProfileOrigin::Managed; }
    bool isUser() const { return origin == BuildProfileOrigin::User && !isSystem(); }
    bool isDeletable() const { return !isSystem(); }
    bool isRenameable() const { return isUser(); }
    bool isEditable() const { return !isSystem(); }
    bool shouldPersist() const { return !isSystem() && persist; }

    QJsonObject toJson() const;
    static BuildProfile fromJson(const QJsonObject& o);

    static BuildProfile customProfile(const QString& name);
    static BuildProfile axScriptProfile();
    static BuildProfile bofProfile();
    static BuildProfile eventHandlerProfile();
    static QString profileNameForFile(const QString& filePath);
};

struct CodeEditorOpenOptions
{
    bool restrictProfiles = false;
    QStringList profiles;
    QString profile;

    QString filePath;
    QString contentName;
    QString content;
    QString documentKey;
    QJsonObject panelSeed;

    static CodeEditorOpenOptions fromVariantMap(const QVariantMap& m);
};

class CodeEditorProfileManager : public QObject
{
Q_OBJECT
    QList<BuildProfile> m_profiles;
    QString m_currentId;

    void ensureDefaultExists();
    void saveCurrentName() const;
    void persistProfileRow(const BuildProfile& p) const;
    void removePersistedRow(const QString& storageKey) const;
    int  indexOfId(const QString& id) const;
    int  indexOfName(const QString& name) const;
    const BuildProfile* profileById(const QString& id) const;

public:
    explicit CodeEditorProfileManager(QObject* parent = nullptr);

    static CodeEditorProfileManager* instance();

    void load();

    QList<BuildProfile> profiles() const { return m_profiles; }
    QStringList profileNames() const;
    QList<BuildProfile> visibleProfiles(const CodeEditorOpenOptions& opts) const;

    const BuildProfile* current() const;
    const BuildProfile* profile(const QString& idOrName) const;
    QString currentId() const { return m_currentId; }

    bool setCurrent(const QString& idOrName);
    QString addProfile(const QString& name);
    bool removeProfile(const QString& idOrName);
    bool renameProfile(const QString& idOrName, const QString& newName, QString* outFinalName = nullptr);
    bool updateProfile(const BuildProfile& p);

    QString upsertProfile(const BuildProfile& incoming, bool force = false);

    QString forkProfile(const QString& idOrName, const QString& newName = QString());

Q_SIGNALS:
    void profilesChanged();
    void currentChanged(const BuildProfile& p);
};

#endif
