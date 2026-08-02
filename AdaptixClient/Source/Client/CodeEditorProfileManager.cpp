#include <Client/CodeEditorProfileManager.h>
#include <Client/Storage.h>

#include <QJsonDocument>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QVariantMap>
#include <QSet>
#include <algorithm>

namespace {

QString loadAxScriptResource(const QString& path)
{
    static QMutex mu;
    static QHash<QString, QString> cache;
    QMutexLocker lock(&mu);
    const auto it = cache.constFind(path);
    if (it != cache.constEnd())
        return it.value();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("CodeEditorProfileManager: missing resource %s", qPrintable(path));
        cache.insert(path, QString());
        return {};
    }
    const QString text = QString::fromUtf8(f.readAll());
    cache.insert(path, text);
    return text;
}

QString axRes(const char* relative)
{
    return loadAxScriptResource(QStringLiteral(":/axscript/") + QLatin1String(relative));
}

} // namespace

BuildProfileToolbar BuildProfileToolbar::defaults()
{
    BuildProfileToolbar t;
    t.panel = QStringLiteral("axscript");
    return t;
}

QJsonObject BuildProfileToolbar::toJson() const
{
    QJsonObject o;
    o["newFile"] = newFile;
    o["openFile"] = openFile;
    o["openFolder"] = openFolder;
    o["save"] = save;
    o["explorer"] = explorer;
    o["buildLog"] = buildLog;
    o["minimap"] = minimap;
    o["wordWrap"] = wordWrap;
    o["panel"] = panel;
    return o;
}

BuildProfileToolbar BuildProfileToolbar::fromJson(const QJsonObject& o, const BuildProfileToolbar& fallback)
{
    BuildProfileToolbar t = fallback;
    auto b = [&](const char* key, bool& field) {
        if (o.contains(QLatin1String(key)))
            field = o.value(QLatin1String(key)).toBool(field);
    };
    b("newFile", t.newFile);
    b("openFile", t.openFile);
    b("openFolder", t.openFolder);
    b("save", t.save);
    b("explorer", t.explorer);
    b("buildLog", t.buildLog);
    b("minimap", t.minimap);
    b("wordWrap", t.wordWrap);
    if (o.contains(QLatin1String("panel")))
        t.panel = o.value(QLatin1String("panel")).toString(t.panel);
    return t;
}

BuildProfileAction BuildProfileAction::make(const QString& id, const QString& label, const QString& icon, const QString& script)
{
    BuildProfileAction a;
    a.id = id;
    a.label = label;
    a.icon = icon;
    a.script = script;
    return a;
}

QStringList BuildProfileAction::toolbarIconPaths()
{
    static const QStringList k = {
        QStringLiteral(":/icons/logs"),
        QStringLiteral(":/icons/listeners"),
        QStringLiteral(":/icons/extension"),
        QStringLiteral(":/icons/folder_code"),
        QStringLiteral(":/icons/code"),
        QStringLiteral(":/icons/format_list"),
        QStringLiteral(":/icons/graph"),
        QStringLiteral(":/icons/job"),
        QStringLiteral(":/icons/chat"),
        QStringLiteral(":/icons/vpn"),
        QStringLiteral(":/icons/downloads"),
        QStringLiteral(":/icons/devices"),
        QStringLiteral(":/icons/key"),
        QStringLiteral(":/icons/picture"),
        QStringLiteral(":/icons/keyboard"),
        QStringLiteral(":/icons/link"),
        QStringLiteral(":/icons/unlink"),
        QStringLiteral(":/icons/plus"),
        QStringLiteral(":/icons/close"),
        QStringLiteral(":/icons/check"),
        QStringLiteral(":/icons/double_arrow_down"),
        QStringLiteral(":/icons/arrow_drop_down"),
        QStringLiteral(":/icons/arrow_drop_up"),
        QStringLiteral(":/icons/reload"),
        QStringLiteral(":/icons/folder"),
        QStringLiteral(":/icons/upload"),
        QStringLiteral(":/icons/arrow_right"),
        QStringLiteral(":/icons/storage"),
        QStringLiteral(":/icons/search"),
        QStringLiteral(":/icons/file_open"),
        QStringLiteral(":/icons/save_as"),
        QStringLiteral(":/icons/start"),
        QStringLiteral(":/icons/restart"),
        QStringLiteral(":/icons/stop"),
        QStringLiteral(":/icons/settings_account"),
        QStringLiteral(":/icons/build"),
        QStringLiteral(":/icons/delete"),
        QStringLiteral(":/icons/new_file"),
        QStringLiteral(":/icons/open_folder"),
        QStringLiteral(":/icons/fs_ssd"),
        QStringLiteral(":/icons/fs_document"),
        QStringLiteral(":/icons/fs_folder"),
        QStringLiteral(":/icons/fs_open_folder"),
        QStringLiteral(":/icons/fs_unknown"),
    };
    return k;
}

QJsonObject BuildProfileAction::toJson() const
{
    QJsonObject o;
    o["id"]     = id;
    o["label"]  = label;
    o["icon"]   = icon;
    o["script"] = script;
    return o;
}

BuildProfileAction BuildProfileAction::fromJson(const QJsonObject& o)
{
    BuildProfileAction a;
    a.id     = o.value(QStringLiteral("id")).toString();
    a.label  = o.value(QStringLiteral("label")).toString();
    a.icon   = o.value(QStringLiteral("icon")).toString();
    a.script = o.value(QStringLiteral("script")).toString();
    if (a.id.isEmpty() && !a.label.isEmpty())
        a.id = a.label.toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9_]+")), QStringLiteral("_"));
    return a;
}

QString BuildProfile::defaultAxScriptPanelScript()
{
    return axRes("panels/axscript_panel.axs");
}

QString BuildProfile::defaultBofPanelScript()
{
    return axRes("panels/bof_panel.axs");
}

QString BuildProfile::defaultCustomPanelScript()
{
    return axRes("panels/custom_panel.axs");
}

QString BuildProfile::defaultEventHandlerPanelScript()
{
    return axRes("panels/event_handler_panel.axs");
}

void BuildProfile::applyCurrentDefaults(const bool preservePanelState)
{
    const QJsonObject keepState = preservePanelState ? panelState : QJsonObject();
    panelState = QJsonObject();

    const BuildProfileType t = profileType();
    if (t == ProfileEventHandler) {
        language = QStringLiteral("axscript");
        toolbar = BuildProfileToolbar::defaults();
        panelScript = defaultEventHandlerPanelScript();
        customActions = defaultEventHandlerActions();
        if (!panelState.contains(QStringLiteral("event")))
            panelState.insert(QStringLiteral("event"), QStringLiteral("agent.new"));
    } else if (t == ProfileAxScript || language == QLatin1String("axscript")) {
        language = QStringLiteral("axscript");
        toolbar = BuildProfileToolbar::defaults();
        panelScript = defaultAxScriptPanelScript();
        customActions = defaultAxScriptActions();
    } else if (t == ProfileBOF || language == QLatin1String("c") || language == QLatin1String("cpp")) {
        if (language.isEmpty() || language == QLatin1String("plain"))
            language = (name.contains(QLatin1String("CPP"), Qt::CaseInsensitive) || name.contains(QLatin1String("C++"))) ? QStringLiteral("cpp") : QStringLiteral("c");
        toolbar = BuildProfileToolbar::defaults();
        panelScript = defaultBofPanelScript();
        customActions = defaultBofActions();
        const QString cmd = language == QLatin1String("cpp")
            ? QStringLiteral("x86_64-w64-mingw32-g++ -c %f -o %o -DBOF")
            : QStringLiteral("x86_64-w64-mingw32-gcc -c %f -o %o -DBOF");
        if (!panelState.contains(QStringLiteral("build")))
            panelState.insert(QStringLiteral("build"), cmd);
    } else {
        toolbar = BuildProfileToolbar::defaults();
        panelScript = defaultCustomPanelScript();
        customActions = defaultBofActions();
    }
    for (auto it = keepState.begin(); it != keepState.end(); ++it)
        panelState.insert(it.key(), it.value());
    if (toolbar.panel.isEmpty() || toolbar.panel == QLatin1String("build"))
        toolbar.panel = QStringLiteral("axscript");
    schemaVersion = kBuildProfileSchemaVersion;
    refreshContentHash();
}

QString BuildProfile::originToString(BuildProfileOrigin o)
{
    switch (o) {
    case BuildProfileOrigin::System:  return QStringLiteral("system");
    case BuildProfileOrigin::Managed: return QStringLiteral("managed");
    case BuildProfileOrigin::User:
    default:                          return QStringLiteral("user");
    }
}

BuildProfileOrigin BuildProfile::originFromString(const QString& s)
{
    const QString v = s.trimmed().toLower();
    if (v == QLatin1String("system"))  return BuildProfileOrigin::System;
    if (v == QLatin1String("managed")) return BuildProfileOrigin::Managed;
    return BuildProfileOrigin::User;
}

QString BuildProfile::sanitizeIdSegment(const QString& s)
{
    QString out = s.trimmed().toLower();
    out.replace(QRegularExpression(QStringLiteral("[^a-z0-9._-]+")), QStringLiteral("_"));
    out.replace(QRegularExpression(QStringLiteral("_+")), QStringLiteral("_"));
    while (out.startsWith(QLatin1Char('_')) || out.startsWith(QLatin1Char('.')))
        out.remove(0, 1);
    while (out.endsWith(QLatin1Char('_')) || out.endsWith(QLatin1Char('.')))
        out.chop(1);
    if (out.isEmpty())
        out = QStringLiteral("profile");
    return out;
}

QString BuildProfile::makeUserId(const QString& nameHint)
{
    return QStringLiteral("user.%1").arg(sanitizeIdSegment(nameHint));
}

QString BuildProfile::computeContentHash() const
{
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(id.toUtf8());
    h.addData(name.toUtf8());
    h.addData(originToString(origin).toUtf8());
    h.addData(language.toUtf8());
    h.addData(panelScript.toUtf8());
    h.addData(QJsonDocument(toolbar.toJson()).toJson(QJsonDocument::Compact));
    QJsonArray acts;
    for (const auto& a : customActions)
        acts.append(a.toJson());
    h.addData(QJsonDocument(acts).toJson(QJsonDocument::Compact));
    return QString::fromLatin1(h.result().toHex());
}

QJsonObject BuildProfile::toJson() const
{
    QJsonObject o;
    o["id"]            = id;
    o["name"]          = name;
    o["origin"]        = originToString(origin);
    o["persist"]       = persist;
    o["contentHash"]   = contentHash.isEmpty() ? computeContentHash() : contentHash;
    o["language"]      = language;
    o["toolbar"]       = toolbar.toJson();
    o["panelScript"]   = panelScript;
    o["schemaVersion"] = schemaVersion;
    if (!panelState.isEmpty())
        o["panelState"] = panelState;
    QJsonArray acts;
    for (const auto& a : customActions)
        acts.append(a.toJson());
    o["customActions"] = acts;
    return o;
}

BuildProfile BuildProfile::fromJson(const QJsonObject& o)
{
    BuildProfile p;
    p.name          = o.value(QStringLiteral("name")).toString();
    p.id            = o.value(QStringLiteral("id")).toString();
    p.origin        = originFromString(o.value(QStringLiteral("origin")).toString());
    p.persist       = o.value(QStringLiteral("persist")).toBool(true);
    p.contentHash   = o.value(QStringLiteral("contentHash")).toString();
    p.language      = o.value(QStringLiteral("language")).toString();
    p.panelScript   = o.value(QStringLiteral("panelScript")).toString();
    p.schemaVersion = o.value(QStringLiteral("schemaVersion")).toInt(0);
    if (o.value(QStringLiteral("panelState")).isObject())
        p.panelState = o.value(QStringLiteral("panelState")).toObject();

    BuildProfileToolbar fallback = BuildProfileToolbar::defaults();
    const BuildProfileType t = typeForName(p.name);

    if (p.language.isEmpty()) {
        if (t == ProfileAxScript || t == ProfileEventHandler)
            p.language = QStringLiteral("axscript");
        else if (t == ProfileBOF)
            p.language = QStringLiteral("c");
        else
            p.language = QStringLiteral("plain");
    }

    p.toolbar = BuildProfileToolbar::fromJson(o.value(QStringLiteral("toolbar")).toObject(), fallback);
    if (p.toolbar.panel == QLatin1String("build") || p.toolbar.panel.isEmpty())
        p.toolbar.panel = QStringLiteral("axscript");

    const QJsonArray acts = o.value(QStringLiteral("customActions")).toArray();
    for (const auto& v : acts)
        p.customActions.append(BuildProfileAction::fromJson(v.toObject()));

    if (t != ProfileCustom) {
        p.origin = BuildProfileOrigin::System;
        if (p.id.isEmpty()) {
            if (t == ProfileAxScript)
                p.id = QStringLiteral("system.axscript");
            else if (t == ProfileEventHandler)
                p.id = QStringLiteral("system.event_handler");
            else
                p.id = QStringLiteral("system.bof");
        }
    }

    if (p.id.isEmpty()) {
        if (p.origin == BuildProfileOrigin::Managed)
            p.id = QStringLiteral("managed.%1").arg(sanitizeIdSegment(p.name));
        else
            p.id = makeUserId(p.name.isEmpty() ? QStringLiteral("profile") : p.name);
    }

    if (p.customActions.isEmpty() || p.panelScript.trimmed().isEmpty()) {
        p.applyCurrentDefaults(true);
    } else {
        p.schemaVersion = kBuildProfileSchemaVersion;
        if (p.contentHash.isEmpty())
            p.refreshContentHash();
    }
    return p;
}

QVector<BuildProfileAction> BuildProfile::defaultAxScriptActions()
{
    return {
        BuildProfileAction::make(
            QStringLiteral("run"), QStringLiteral("Run"), QStringLiteral(":/icons/start"),
            axRes("actions/axscript_run.axs")),
        BuildProfileAction::make(
            QStringLiteral("stop"), QStringLiteral("Stop"), QStringLiteral(":/icons/stop"),
            axRes("actions/axscript_stop.axs")),
    };
}

QVector<BuildProfileAction> BuildProfile::defaultBofActions()
{
    return {
        BuildProfileAction::make(
            QStringLiteral("build"), QStringLiteral("Build"), QStringLiteral(":/icons/build"),
            axRes("actions/bof_build.axs")),
        BuildProfileAction::make(
            QStringLiteral("stop"), QStringLiteral("Stop"), QStringLiteral(":/icons/stop"),
            axRes("actions/bof_stop.axs")),
    };
}

QVector<BuildProfileAction> BuildProfile::defaultEventHandlerActions()
{
    const QString helpers = axRes("actions/event_handler_helpers.axs");
    return {
        BuildProfileAction::make(
            QStringLiteral("register"), QStringLiteral("Register"), QStringLiteral(":/icons/upload"),
            helpers + axRes("actions/event_handler_register.axs")),
        BuildProfileAction::make(
            QStringLiteral("test_local"), QStringLiteral("Test local"), QStringLiteral(":/icons/start"),
            helpers + axRes("actions/event_handler_test_local.axs")),
    };
}

BuildProfile BuildProfile::customProfile(const QString& name)
{
    BuildProfile p;
    p.name = name;
    p.id = makeUserId(name);
    p.origin = BuildProfileOrigin::User;
    p.persist = true;
    p.language = QStringLiteral("plain");
    p.toolbar = BuildProfileToolbar::defaults();
    p.panelScript = defaultCustomPanelScript();
    p.customActions = defaultBofActions();
    p.refreshContentHash();
    return p;
}

BuildProfile BuildProfile::axScriptProfile()
{
    BuildProfile p;
    p.id = QStringLiteral("system.axscript");
    p.name = QStringLiteral("AxScript");
    p.origin = BuildProfileOrigin::System;
    p.persist = false;
    p.language = QStringLiteral("axscript");
    p.toolbar = BuildProfileToolbar::defaults();
    p.panelScript = defaultAxScriptPanelScript();
    p.customActions = defaultAxScriptActions();
    p.refreshContentHash();
    return p;
}

BuildProfile BuildProfile::bofProfile()
{
    BuildProfile p;
    p.id = QStringLiteral("system.bof");
    p.name = QStringLiteral("BOF");
    p.origin = BuildProfileOrigin::System;
    p.persist = false;
    const QString cmd = QStringLiteral("x86_64-w64-mingw32-gcc -c %f -o %o -DBOF");
    p.panelState.insert(QStringLiteral("build"), cmd);
    p.language = QStringLiteral("c");
    p.toolbar = BuildProfileToolbar::defaults();
    p.panelScript = defaultBofPanelScript();
    p.customActions = defaultBofActions();
    p.refreshContentHash();
    return p;
}

BuildProfile BuildProfile::eventHandlerProfile()
{
    BuildProfile p;
    p.id = QStringLiteral("system.event_handler");
    p.name = QStringLiteral("Event Handler");
    p.origin = BuildProfileOrigin::System;
    p.persist = false;
    p.language = QStringLiteral("axscript");
    p.toolbar = BuildProfileToolbar::defaults();
    p.panelScript = defaultEventHandlerPanelScript();
    p.customActions = defaultEventHandlerActions();
    p.panelState.insert(QStringLiteral("event"), QStringLiteral("agent.new"));
    p.panelState.insert(QStringLiteral("name"), QStringLiteral("auto_handler"));
    p.panelState.insert(QStringLiteral("description"), QStringLiteral("Handler for agent.new"));
    p.panelState.insert(QStringLiteral("group"), QStringLiteral("auto_handler"));
    p.panelState.insert(QStringLiteral("skip_restore"), false);
    p.refreshContentHash();
    return p;
}

QString BuildProfile::profileNameForFile(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == QLatin1String("axs") || suffix == QLatin1String("js") || suffix == QLatin1String("mjs"))
        return QStringLiteral("AxScript");
    if (suffix == QLatin1String("c") || suffix == QLatin1String("cc") || suffix == QLatin1String("cpp") || suffix == QLatin1String("cxx") || suffix == QLatin1String("h") || suffix == QLatin1String("hh") || suffix == QLatin1String("hpp") || suffix == QLatin1String("hxx") || suffix == QLatin1String("h++") || suffix == QLatin1String("inl"))
        return QStringLiteral("BOF");
    return QString();
}

CodeEditorOpenOptions CodeEditorOpenOptions::fromVariantMap(const QVariantMap& m)
{
    CodeEditorOpenOptions opts;
    if (m.contains(QStringLiteral("profiles"))) {
        opts.restrictProfiles = true;
        const QVariant v = m.value(QStringLiteral("profiles"));
        if (v.canConvert<QStringList>()) {
            opts.profiles = v.toStringList();
        } else if (v.typeId() == QMetaType::QVariantList || v.canConvert<QVariantList>()) {
            for (const QVariant& item : v.toList())
                opts.profiles.append(item.toString());
        } else if (v.canConvert<QString>()) {
            const QString s = v.toString().trimmed();
            if (!s.isEmpty())
                opts.profiles.append(s);
        }
    }
    if (m.contains(QStringLiteral("profile")))
        opts.profile = m.value(QStringLiteral("profile")).toString();
    return opts;
}

static const QString kCurrentMetaName = QStringLiteral("__current__");

CodeEditorProfileManager::CodeEditorProfileManager(QObject* parent) : QObject(parent) {}

CodeEditorProfileManager* CodeEditorProfileManager::instance()
{
    static CodeEditorProfileManager* s = nullptr;
    if (!s) {
        s = new CodeEditorProfileManager(qApp);
        s->load();
    }
    return s;
}

void CodeEditorProfileManager::load()
{
    m_profiles.clear();
    m_currentId.clear();

    QStringList staleStorageKeys;
    const auto rows = Storage::ListBuildProfiles();
    for (const auto& [rowKey, data] : rows) {
        if (rowKey == kCurrentMetaName) {
            const QJsonObject meta = QJsonDocument::fromJson(data.toUtf8()).object();
            m_currentId = meta.value(QStringLiteral("current")).toString();
            continue;
        }
        if (BuildProfile::typeForName(rowKey) != ProfileCustom)
            continue;

        const QJsonObject json = QJsonDocument::fromJson(data.toUtf8()).object();
        BuildProfile p = BuildProfile::fromJson(json);
        if (p.name.isEmpty())
            p.name = rowKey;
        if (p.isSystem())
            continue;
        if (p.id.isEmpty())
            p.id = BuildProfile::makeUserId(p.name);

        if (rowKey != p.id) {
            staleStorageKeys.append(rowKey);
            if (p.shouldPersist())
                persistProfileRow(p);
        }

        if (indexOfId(p.id) >= 0)
            continue;
        m_profiles.append(p);
    }

    for (const QString& k : staleStorageKeys)
        removePersistedRow(k);

    for (const auto& [rowKey, _] : Storage::ListBuildProfiles()) {
        if (rowKey == kCurrentMetaName)
            continue;
        if (BuildProfile::typeForName(rowKey) != ProfileCustom)
            removePersistedRow(rowKey);
    }

    ensureDefaultExists();

    if (m_currentId.isEmpty() || !profile(m_currentId))
        m_currentId = QStringLiteral("system.axscript");
    else if (const BuildProfile* cur = profile(m_currentId))
        m_currentId = cur->id;

    saveCurrentName();
}

void CodeEditorProfileManager::ensureDefaultExists()
{
    m_profiles.erase(std::remove_if(m_profiles.begin(), m_profiles.end(), [](const BuildProfile& p) { return p.isSystem(); }), m_profiles.end());

    m_profiles.prepend(BuildProfile::eventHandlerProfile());
    m_profiles.prepend(BuildProfile::bofProfile());
    m_profiles.prepend(BuildProfile::axScriptProfile());

    if (m_currentId.isEmpty() || !profile(m_currentId))
        m_currentId = QStringLiteral("system.axscript");
}

void CodeEditorProfileManager::persistProfileRow(const BuildProfile& p) const
{
    if (!p.shouldPersist() || p.id.isEmpty())
        return;
    BuildProfile copy = p;
    if (copy.contentHash.isEmpty())
        copy.refreshContentHash();
    const QString data = QString::fromUtf8(QJsonDocument(copy.toJson()).toJson(QJsonDocument::Compact));
    Storage::AddBuildProfile(copy.id, data);
}

void CodeEditorProfileManager::removePersistedRow(const QString& storageKey) const
{
    if (storageKey.isEmpty())
        return;
    Storage::RemoveBuildProfile(storageKey);
}

void CodeEditorProfileManager::saveCurrentName() const
{
    Storage::RemoveBuildProfile(kCurrentMetaName);
    QJsonObject meta;
    meta["current"] = m_currentId;
    const QString data = QString::fromUtf8(QJsonDocument(meta).toJson(QJsonDocument::Compact));
    Storage::AddBuildProfile(kCurrentMetaName, data);
}

int CodeEditorProfileManager::indexOfId(const QString& id) const
{
    if (id.isEmpty())
        return -1;
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].id == id)
            return i;
    }
    return -1;
}

int CodeEditorProfileManager::indexOfName(const QString& name) const
{
    if (name.isEmpty())
        return -1;
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].name == name)
            return i;
    }
    return -1;
}

QStringList CodeEditorProfileManager::profileNames() const
{
    QStringList names;
    names.reserve(m_profiles.size());
    for (const auto& p : m_profiles)
        names.append(p.name);
    return names;
}

QList<BuildProfile> CodeEditorProfileManager::visibleProfiles(const CodeEditorOpenOptions& opts) const
{
    if (!opts.restrictProfiles)
        return m_profiles;

    QList<BuildProfile> out;
    QSet<QString> seen;
    for (const QString& key : opts.profiles) {
        if (const BuildProfile* p = profile(key)) {
            if (seen.contains(p->id))
                continue;
            seen.insert(p->id);
            out.append(*p);
        }
    }
    return out;
}

const BuildProfile* CodeEditorProfileManager::current() const
{
    return profile(m_currentId);
}

const BuildProfile* CodeEditorProfileManager::profileById(const QString& id) const
{
    const int i = indexOfId(id);
    return i >= 0 ? &m_profiles[i] : nullptr;
}

const BuildProfile* CodeEditorProfileManager::profile(const QString& idOrName) const
{
    if (idOrName.isEmpty())
        return nullptr;
    if (const BuildProfile* p = profileById(idOrName))
        return p;
    const int n = indexOfName(idOrName);
    return n >= 0 ? &m_profiles[n] : nullptr;
}

bool CodeEditorProfileManager::setCurrent(const QString& idOrName)
{
    const BuildProfile* p = profile(idOrName);
    if (!p)
        return false;
    if (m_currentId == p->id)
        return true;
    m_currentId = p->id;
    saveCurrentName();
    Q_EMIT currentChanged(*p);
    return true;
}

QString CodeEditorProfileManager::addProfile(const QString& name)
{
    QString unique = name.trimmed();
    if (unique.isEmpty())
        unique = QStringLiteral("Profile");
    int i = 1;
    const auto names = profileNames();
    QString candidate = unique;
    while (names.contains(candidate) || candidate == kCurrentMetaName || BuildProfile::typeForName(candidate) != ProfileCustom)
        candidate = QStringLiteral("%1_%2").arg(unique).arg(i++);

    BuildProfile p = BuildProfile::customProfile(candidate);
    int j = 1;
    QString id = p.id;
    while (indexOfId(id) >= 0)
        id = QStringLiteral("%1_%2").arg(p.id).arg(j++);
    p.id = id;
    p.refreshContentHash();

    m_profiles.append(p);
    m_currentId = p.id;
    persistProfileRow(p);
    saveCurrentName();
    Q_EMIT profilesChanged();
    Q_EMIT currentChanged(m_profiles.last());
    return p.name;
}

bool CodeEditorProfileManager::removeProfile(const QString& idOrName)
{
    const BuildProfile* found = profile(idOrName);
    if (!found)
        return false;
    if (!found->isDeletable())
        return false;

    const QString id = found->id;
    const QString name = found->name;
    const int i = indexOfId(id);
    if (i < 0)
        return false;

    m_profiles.removeAt(i);
    removePersistedRow(id);
    if (name != id)
        removePersistedRow(name);

    if (m_currentId == id) {
        m_currentId = m_profiles.isEmpty() ? QString() : m_profiles.first().id;
        saveCurrentName();
        if (!m_profiles.isEmpty())
            Q_EMIT currentChanged(m_profiles.first());
    }
    Q_EMIT profilesChanged();
    return true;
}

bool CodeEditorProfileManager::renameProfile(const QString& idOrName, const QString& newName, QString* outFinalName)
{
    const int i = indexOfId(idOrName) >= 0 ? indexOfId(idOrName) : indexOfName(idOrName);
    if (i < 0 || newName.trimmed().isEmpty())
        return false;
    if (!m_profiles[i].isRenameable())
        return false;

    QString unique = newName.trimmed();
    if (unique == kCurrentMetaName)
        return false;
    if (BuildProfile::typeForName(unique) != ProfileCustom)
        unique = unique + QStringLiteral("_custom");

    int n = 1;
    const auto names = profileNames();
    QString candidate = unique;
    while ((names.contains(candidate) && candidate != m_profiles[i].name) || BuildProfile::typeForName(candidate) != ProfileCustom || candidate == kCurrentMetaName)
        candidate = QStringLiteral("%1_%2").arg(unique).arg(n++);

    m_profiles[i].name = candidate;
    m_profiles[i].refreshContentHash();
    if (outFinalName)
        *outFinalName = candidate;
    persistProfileRow(m_profiles[i]);
    Q_EMIT profilesChanged();
    if (m_currentId == m_profiles[i].id)
        Q_EMIT currentChanged(m_profiles[i]);
    return true;
}

bool CodeEditorProfileManager::updateProfile(const BuildProfile& p)
{
    const int i = indexOfId(p.id) >= 0 ? indexOfId(p.id) : (p.name.isEmpty() ? -1 : indexOfName(p.name));
    if (i < 0)
        return false;

    if (m_profiles[i].isSystem()) {
        m_profiles[i].panelState = p.panelState;
        if (m_currentId == m_profiles[i].id)
            Q_EMIT currentChanged(m_profiles[i]);
        return true;
    }

    const QString keepId = m_profiles[i].id;
    const BuildProfileOrigin keepOrigin = m_profiles[i].origin;
    const bool keepPersist = m_profiles[i].persist;

    m_profiles[i].language       = p.language.isEmpty() ? m_profiles[i].language : p.language;
    m_profiles[i].toolbar        = p.toolbar;
    m_profiles[i].panelScript    = p.panelScript;
    m_profiles[i].panelState     = p.panelState;
    m_profiles[i].customActions  = p.customActions;
    m_profiles[i].schemaVersion  = p.schemaVersion > 0 ? p.schemaVersion : kBuildProfileSchemaVersion;
    if (!p.name.isEmpty() && m_profiles[i].isRenameable())
        m_profiles[i].name = p.name;
    m_profiles[i].id = keepId;
    m_profiles[i].origin = keepOrigin;
    m_profiles[i].persist = keepPersist;
    m_profiles[i].refreshContentHash();

    persistProfileRow(m_profiles[i]);
    Q_EMIT profilesChanged();
    if (m_currentId == m_profiles[i].id)
        Q_EMIT currentChanged(m_profiles[i]);
    return true;
}

QString CodeEditorProfileManager::upsertProfile(const BuildProfile& incoming, bool force)
{
    BuildProfile p = incoming;
    if (p.id.trimmed().isEmpty())
        return QStringLiteral("error: id is required");
    p.id = p.id.trimmed();

    if (p.id.startsWith(QLatin1String("system.")) || BuildProfile::typeForName(p.name) != ProfileCustom) {
        return QStringLiteral("error: cannot upsert system profile");
    }

    if (p.name.trimmed().isEmpty())
        p.name = p.id;
    if (p.origin == BuildProfileOrigin::System)
        p.origin = BuildProfileOrigin::Managed;
    if (!force && p.origin != BuildProfileOrigin::User)
        p.origin = BuildProfileOrigin::Managed;

    if (p.language.isEmpty())
        p.language = QStringLiteral("plain");
    if (p.toolbar.panel.isEmpty())
        p.toolbar.panel = QStringLiteral("axscript");
    p.schemaVersion = kBuildProfileSchemaVersion;
    p.refreshContentHash();

    const int i = indexOfId(p.id);
    if (i < 0) {
        QString name = p.name;
        int n = 1;
        const auto names = profileNames();
        while (names.contains(name) || BuildProfile::typeForName(name) != ProfileCustom)
            name = QStringLiteral("%1_%2").arg(p.name).arg(n++);
        p.name = name;
        p.refreshContentHash();
        m_profiles.append(p);
        persistProfileRow(p);
        Q_EMIT profilesChanged();
        return QStringLiteral("inserted");
    }

    BuildProfile& existing = m_profiles[i];
    if (existing.isSystem())
        return QStringLiteral("error: cannot upsert system profile");

    if (existing.isUser() && !force && p.origin == BuildProfileOrigin::Managed)
        return QStringLiteral("unchanged");

    if (existing.contentHash == p.contentHash
        && existing.name == p.name
        && existing.language == p.language
        && existing.panelScript == p.panelScript) {
        return QStringLiteral("unchanged");
    }

    const QJsonObject keepState = existing.panelState;
    existing.language = p.language;
    existing.toolbar = p.toolbar;
    existing.panelScript = p.panelScript;
    existing.customActions = p.customActions;
    existing.schemaVersion = kBuildProfileSchemaVersion;
    existing.origin = p.origin;
    existing.persist = p.persist;
    if (!p.name.isEmpty())
        existing.name = p.name;
    if (!p.panelState.isEmpty())
        existing.panelState = p.panelState;
    else
        existing.panelState = keepState;
    existing.refreshContentHash();

    if (existing.shouldPersist())
        persistProfileRow(existing);
    else
        removePersistedRow(existing.id);

    Q_EMIT profilesChanged();
    if (m_currentId == existing.id)
        Q_EMIT currentChanged(existing);
    return QStringLiteral("updated");
}

QString CodeEditorProfileManager::forkProfile(const QString& idOrName, const QString& newName)
{
    const BuildProfile* src = profile(idOrName);
    if (!src)
        return {};

    QString baseName = newName.trimmed();
    if (baseName.isEmpty())
        baseName = src->name + QStringLiteral(" (copy)");
    if (BuildProfile::typeForName(baseName) != ProfileCustom)
        baseName = baseName + QStringLiteral("_custom");

    int n = 1;
    QString candidate = baseName;
    const auto names = profileNames();
    while (names.contains(candidate) || BuildProfile::typeForName(candidate) != ProfileCustom)
        candidate = QStringLiteral("%1_%2").arg(baseName).arg(n++);

    BuildProfile fork = *src;
    fork.name = candidate;
    fork.id = BuildProfile::makeUserId(candidate);
    int j = 1;
    QString id = fork.id;
    while (indexOfId(id) >= 0)
        id = QStringLiteral("%1_%2").arg(fork.id).arg(j++);
    fork.id = id;
    fork.origin = BuildProfileOrigin::User;
    fork.persist = true;
    fork.schemaVersion = kBuildProfileSchemaVersion;
    fork.refreshContentHash();

    m_profiles.append(fork);
    persistProfileRow(fork);
    Q_EMIT profilesChanged();
    return fork.id;
}
