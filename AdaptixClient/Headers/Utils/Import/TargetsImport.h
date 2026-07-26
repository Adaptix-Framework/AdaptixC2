#ifndef ADAPTIXCLIENT_TARGETSIMPORT_H
#define ADAPTIXCLIENT_TARGETSIMPORT_H

#include <main.h>

struct TargetsImportResult {
    QList<TargetData> items;
    int skipped = 0;
    int totalLines = 0;
    QString detectedFormat;
    QStringList notes;
};

TargetsImportResult parseTargetsImport(const QString& text, const QString& defaultTag = QString(), bool defaultAlive = true, int maxItems = 10000);

#endif
