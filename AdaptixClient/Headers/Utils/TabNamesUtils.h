#ifndef ADAPTIXCLIENT_TABNAMESUTILS_H
#define ADAPTIXCLIENT_TABNAMESUTILS_H

#include <QString>

class TabNamesUtils
{
public:
    static void removeAgentTabNames(const QString &agentId, const QString &projectName);
    static QString getCustomTabName(const QString &tabName, const QString &projectName);
    static void setCustomTabName(const QString &uniqueName, const QString &customTitle);
    static void removeCustomTabName(const QString &uniqueName);
    static bool hasCustomTabName(const QString &uniqueName);
    static QString buildUniqueName(const QString &tabName, const QString &projectName);
};

#endif // ADAPTIXCLIENT_TABNAMESUTILS_H
