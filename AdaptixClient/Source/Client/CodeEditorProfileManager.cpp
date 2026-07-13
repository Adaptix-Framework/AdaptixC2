#include <Client/CodeEditorProfileManager.h>
#include <Client/Storage.h>

#include <QJsonDocument>

QJsonObject BuildProfile::toJson() const
{
    QJsonObject o;
    o["name"]         = name;
    o["buildCommand"] = buildCommand;
    o["runCommand"]   = runCommand;
    o["defines"]      = defines;
    o["mainEngine"]   = mainEngine;
    QJsonArray arr;
    for (const auto& p : params) {
        QJsonObject po;
        po["name"]  = p.name;
        po["type"]  = p.type;
        po["value"] = p.value;
        arr.append(po);
    }
    o["params"] = arr;
    return o;
}

BuildProfile BuildProfile::fromJson(const QJsonObject& o)
{
    BuildProfile p;
    p.name         = o.value("name").toString();
    p.buildCommand = o.value("buildCommand").toString();
    p.runCommand   = o.value("runCommand").toString();
    p.defines      = o.value("defines").toString();
    p.mainEngine   = o.value("mainEngine").toBool(false);
    const QJsonArray arr = o.value("params").toArray();
    for (const auto& v : arr) {
        const QJsonObject po = v.toObject();
        p.params.append({po.value("name").toString(),
                         po.value("type").toString(),
                         po.value("value").toString()});
    }
    return p;
}

BuildProfile BuildProfile::customProfile(const QString& name)
{
    return BuildProfile{name, QString(), QString(), QString(), false, {}};
}

BuildProfile BuildProfile::axScriptProfile()
{
    return BuildProfile{QStringLiteral("AxScript"), QString(), QStringLiteral("ax.load('%f')"), QString(), false, {}};
}

BuildProfile BuildProfile::bofCProfile()
{
    return BuildProfile{QStringLiteral("BOF-C"), QStringLiteral("x86_64-w64-mingw32-gcc -c %f -o %o -D__BOF__"), QString(), QString(), false, {}};
}

BuildProfile BuildProfile::bofCppProfile()
{
    return BuildProfile{QStringLiteral("BOF-CPP"), QStringLiteral("x86_64-w64-mingw32-g++ -c %f -o %o -D__BOF__"), QString(), QString(), false, {}};
}

QString BuildProfile::profileNameForFile(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == "axs")
        return QStringLiteral("AxScript");
    if (suffix == "c")
        return QStringLiteral("BOF-C");
    if (suffix == "cpp" || suffix == "cc" || suffix == "cxx")
        return QStringLiteral("BOF-CPP");
    return QString();
}

static const QString CURRENT_PROFILE_KEY = "codeeditor_current_profile";

CodeEditorProfileManager::CodeEditorProfileManager(QObject* parent) : QObject(parent) {}

void CodeEditorProfileManager::load()
{
    m_profiles.clear();
    m_currentName.clear();

    const auto rows = Storage::ListBuildProfiles();
    for (const auto& [name, data] : rows) {
        const QJsonObject json = QJsonDocument::fromJson(data.toUtf8()).object();
        BuildProfile p = BuildProfile::fromJson(json);
        if (!p.name.isEmpty())
            m_profiles.append(p);
    }

    SettingsData settings;
    Storage::SelectSettingsMain(&settings);
    const auto currentRows = Storage::ListBuildProfiles();
    Q_UNUSED(currentRows);

    ensureDefaultExists();

    if (m_currentName.isEmpty())
        m_currentName = m_profiles.first().name;
}

void CodeEditorProfileManager::ensureDefaultExists()
{
    if (m_profiles.isEmpty()) {
        m_profiles.append(BuildProfile::axScriptProfile());
        m_profiles.append(BuildProfile::bofCProfile());
        m_profiles.append(BuildProfile::bofCppProfile());
        m_currentName = m_profiles.first().name;
        save();
        return;
    }

    const QStringList names = profileNames();
    if (!names.contains("AxScript"))
        m_profiles.prepend(BuildProfile::axScriptProfile());
    if (!names.contains("BOF-C"))
        m_profiles.insert(qMin(1, m_profiles.size()), BuildProfile::bofCProfile());
    if (!names.contains("BOF-CPP"))
        m_profiles.insert(qMin(2, m_profiles.size()), BuildProfile::bofCppProfile());

    if (m_currentName.isEmpty())
        m_currentName = m_profiles.first().name;
}

void CodeEditorProfileManager::save() const
{
    const auto existing = Storage::ListBuildProfiles();
    for (const auto& [name, _] : existing)
        Storage::RemoveBuildProfile(name);

    for (const auto& p : m_profiles) {
        const QJsonObject json = p.toJson();
        const QString data = QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact));
        Storage::AddBuildProfile(p.name, data);
    }

    saveCurrentName();
}

void CodeEditorProfileManager::saveCurrentName() const
{
    SettingsData settings;
    Storage::SelectSettingsMain(&settings);
    Storage::RemoveBuildProfile("__current__");
    QJsonObject meta;
    meta["current"] = m_currentName;
    const QString data = QString::fromUtf8(QJsonDocument(meta).toJson(QJsonDocument::Compact));
    Storage::AddBuildProfile("__current__", data);
}

QStringList CodeEditorProfileManager::profileNames() const
{
    QStringList names;
    names.reserve(m_profiles.size());
    for (const auto& p : m_profiles)
        names.append(p.name);
    return names;
}

const BuildProfile* CodeEditorProfileManager::current() const
{
    for (const auto& p : m_profiles)
        if (p.name == m_currentName)
            return &p;
    return nullptr;
}

bool CodeEditorProfileManager::setCurrent(const QString& name)
{
    for (const auto& p : m_profiles) {
        if (p.name == name) {
            if (m_currentName == name)
                return true;
            m_currentName = name;
            saveCurrentName();
            Q_EMIT currentChanged(p);
            return true;
        }
    }
    return false;
}

void CodeEditorProfileManager::addProfile(const QString& name)
{
    QString unique = name;
    int i = 1;
    const auto names = profileNames();
    while (names.contains(unique))
        unique = QString("%1_%2").arg(name).arg(i++);

    m_profiles.append(BuildProfile::customProfile(unique));
    m_currentName = unique;
    save();
    Q_EMIT profilesChanged();
    Q_EMIT currentChanged(m_profiles.last());
}

bool CodeEditorProfileManager::removeProfile(const QString& name)
{
    if (BuildProfile::typeForName(name) != ProfileCustom)
        return false;

    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].name == name) {
            m_profiles.removeAt(i);
            if (m_currentName == name) {
                m_currentName = m_profiles.first().name;
                saveCurrentName();
                Q_EMIT currentChanged(m_profiles.first());
            }
            save();
            Q_EMIT profilesChanged();
            return true;
        }
    }
    return false;
}

bool CodeEditorProfileManager::renameCurrent(const QString& newName)
{
    if (newName.isEmpty())
        return false;
    const auto names = profileNames();
    QString unique = newName;
    int i = 1;
    while (names.contains(unique) && unique != m_currentName)
        unique = QString("%1_%2").arg(newName).arg(i++);

    for (auto& p : m_profiles) {
        if (p.name == m_currentName) {
            Storage::RemoveBuildProfile(m_currentName);
            p.name = unique;
            m_currentName = unique;
            save();
            Q_EMIT profilesChanged();
            return true;
        }
    }
    return false;
}

void CodeEditorProfileManager::updateCurrent(const BuildProfile& p)
{
    for (auto& prof : m_profiles) {
        if (prof.name == m_currentName) {
            prof.buildCommand = p.buildCommand;
            prof.runCommand   = p.runCommand;
            prof.defines      = p.defines;
            prof.mainEngine   = p.mainEngine;
            prof.params       = p.params;
            save();
            return;
        }
    }
}
