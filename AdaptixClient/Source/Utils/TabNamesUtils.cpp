#include <Utils/TabNamesUtils.h>
#include <QSettings>
#include <QStringList>

namespace {
    constexpr const char* SETTINGS_ORG = "Adaptix";
    constexpr const char* SETTINGS_APP = "AdaptixClient";
    constexpr const char* TAB_NAMES_PREFIX = "TabNames/";
    const QStringList TAB_TYPES = {"Console [", "Terminal [", "Files [", "Processes ["};
    
    QSettings& getSettings() {
        static QSettings settings(SETTINGS_ORG, SETTINGS_APP);
        return settings;
    }
}

void TabNamesUtils::removeAgentTabNames(const QString &agentId, const QString &projectName)
{
    QSettings& settings = getSettings();
    
    for (const QString& tabType : TAB_TYPES) {
        QString uniqueName = tabType + agentId + "]:Dock-" + projectName;
        settings.remove(TAB_NAMES_PREFIX + uniqueName);
    }
}

QString TabNamesUtils::getCustomTabName(const QString &tabName, const QString &projectName)
{
    QString uniqueName = buildUniqueName(tabName, projectName);
    QSettings& settings = getSettings();
    return settings.value(TAB_NAMES_PREFIX + uniqueName).toString();
}

void TabNamesUtils::setCustomTabName(const QString &uniqueName, const QString &customTitle)
{
    QSettings& settings = getSettings();
    settings.setValue(TAB_NAMES_PREFIX + uniqueName, customTitle);
}

void TabNamesUtils::removeCustomTabName(const QString &uniqueName)
{
    QSettings& settings = getSettings();
    settings.remove(TAB_NAMES_PREFIX + uniqueName);
}

bool TabNamesUtils::hasCustomTabName(const QString &uniqueName)
{
    QSettings& settings = getSettings();
    return settings.contains(TAB_NAMES_PREFIX + uniqueName);
}

QString TabNamesUtils::buildUniqueName(const QString &tabName, const QString &projectName)
{
    return tabName + ":Dock-" + projectName;
}
