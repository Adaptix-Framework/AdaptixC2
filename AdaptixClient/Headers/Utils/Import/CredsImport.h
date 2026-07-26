#ifndef ADAPTIXCLIENT_CREDSIMPORT_H
#define ADAPTIXCLIENT_CREDSIMPORT_H

#include <main.h>

struct CredsImportResult {
    QList<CredentialData> items;
    int skipped = 0;
    int totalLines = 0;
    QStringList notes;
};

CredsImportResult parseCredentialsImport(const QString& text, const QString& defaultTag = QString(), const QString& defaultStorage = QStringLiteral("manual"), int maxItems = 10000);

#endif
