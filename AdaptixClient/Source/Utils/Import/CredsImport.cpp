#include <Utils/Import/CredsImport.h>

#include <QRegularExpression>
#include <QSet>

namespace {

bool isHexString(const QString& s, int expectedLen = -1)
{
    if (s.isEmpty())
        return false;

    if (expectedLen > 0 && s.size() != expectedLen)
        return false;

    for (const QChar c : s) {
        if (!c.isDigit() && (c.toLower() < QLatin1Char('a') || c.toLower() > QLatin1Char('f')))
            return false;
    }
    return true;
}

QString guessSecretType(const QString& secret)
{
    if (isHexString(secret, 32))
        return QStringLiteral("ntlm");
    if (isHexString(secret, 64))
        return QStringLiteral("aes256");
    if (secret.startsWith(QLatin1String("aes256"), Qt::CaseInsensitive) || secret.startsWith(QLatin1String("aes128"), Qt::CaseInsensitive) || secret.startsWith(QLatin1String("des-cbc"), Qt::CaseInsensitive) || secret.startsWith(QLatin1String("rc4"), Qt::CaseInsensitive))
        return QStringLiteral("hash");
    return QStringLiteral("password");
}

QString dedupeKey(const CredentialData& c)
{
    return c.Username.toLower() + QLatin1Char('\x1f') + c.Realm.toLower() + QLatin1Char('\x1f') + c.Password + QLatin1Char('\x1f') + c.Host.toLower();
}

bool looksLikeSecretsdump(const QString& line)
{
    static const QRegularExpression re(QStringLiteral(R"(^(?:(?:[^:\\/\s]+)[\\/])?[^:]+:\d+:(?:[0-9A-Fa-f]{32}|\*):(?:[0-9A-Fa-f]{32}|\*)(?::.*)?$)"));
    return re.match(line).hasMatch();
}

bool parseSecretsdump(const QString& line, CredentialData& out)
{
    const int c1 = line.indexOf(QLatin1Char(':'));
    if (c1 <= 0)
        return false;

    const int c2 = line.indexOf(QLatin1Char(':'), c1 + 1);
    if (c2 < 0)
        return false;

    const int c3 = line.indexOf(QLatin1Char(':'), c2 + 1);
    if (c3 < 0)
        return false;

    const int c4 = line.indexOf(QLatin1Char(':'), c3 + 1);

    const QString account = line.left(c1).trimmed();
    const QString rid = line.mid(c1 + 1, c2 - c1 - 1).trimmed();
    const QString lm = line.mid(c2 + 1, c3 - c2 - 1).trimmed();
    const QString nt = (c4 < 0) ? line.mid(c3 + 1).trimmed() : line.mid(c3 + 1, c4 - c3 - 1).trimmed();

    bool ridOk = false;
    rid.toLongLong(&ridOk);
    if (!ridOk)
        return false;

    if (!isHexString(lm, 32) && lm != QLatin1String("*"))
        return false;

    if (!isHexString(nt, 32) && nt != QLatin1String("*"))
        return false;

    QString realm;
    QString user = account;
    const int bs = account.indexOf(QLatin1Char('\\'));
    const int sl = account.indexOf(QLatin1Char('/'));
    if (bs > 0) {
        realm = account.left(bs);
        user = account.mid(bs + 1);
    } else if (sl > 0) {
        realm = account.left(sl);
        user = account.mid(sl + 1);
    }

    user = user.trimmed();
    if (user.isEmpty())
        return false;

    out = {};
    out.Username = user;
    out.Password = nt;
    out.Realm = realm;
    out.Type = nt.isEmpty() ? QStringLiteral("password") : QStringLiteral("hash");
    out.Storage = QStringLiteral("ntds");
    return !out.Username.isEmpty();
}

bool parseDomainBackslash(const QString& line, CredentialData& out)
{
    static const QRegularExpression re(QStringLiteral(R"(^([^\\:\s]+)\\([^:]+):(.*)$)"));
    const auto m = re.match(line);
    if (!m.hasMatch())
        return false;

    out = {};
    out.Realm = m.captured(1).trimmed();
    out.Username = m.captured(2).trimmed();
    out.Password = m.captured(3).trimmed();
    if (out.Username.isEmpty())
        return false;
    out.Type = guessSecretType(out.Password);
    return true;
}

bool parseImpacketSlash(const QString& line, CredentialData& out)
{
    if (looksLikeSecretsdump(line))
        return false;

    static const QRegularExpression re(QStringLiteral(R"(^([^/:\s]+)/([^:]+):(.*)$)"));
    const auto m = re.match(line);
    if (!m.hasMatch())
        return false;

    out = {};
    out.Realm = m.captured(1).trimmed();
    out.Username = m.captured(2).trimmed();
    out.Password = m.captured(3).trimmed();
    if (out.Username.isEmpty())
        return false;

    out.Type = guessSecretType(out.Password);
    return true;
}

bool parseUserPass(const QString& line, CredentialData& out)
{
    if (looksLikeSecretsdump(line))
        return false;

    const int c = line.indexOf(QLatin1Char(':'));
    if (c <= 0)
        return false;

    if (line.contains(QLatin1String("://")))
        return false;

    out = {};
    out.Username = line.left(c).trimmed();
    out.Password = line.mid(c + 1).trimmed();
    if (out.Username.isEmpty())
        return false;

    if (out.Username.contains(QLatin1Char(' ')))
        return false;

    out.Type = guessSecretType(out.Password);
    return true;
}

}

CredsImportResult parseCredentialsImport(const QString& text, const QString& defaultTag, const QString& defaultStorage, int maxItems)
{
    CredsImportResult result;
    QSet<QString> seen;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral(R"(\r\n|\n|\r)")));
    result.totalLines = lines.size();

    for (QString raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')) || line.startsWith(QLatin1String("//")))
            continue;

        if (line.startsWith(QLatin1String("[*]")) || line.startsWith(QLatin1String("[-]")) || line.startsWith(QLatin1String("[!]")) || line.startsWith(QLatin1String("[+]")))
            continue;
        if (line.contains(QLatin1String("Impacket"), Qt::CaseInsensitive) && line.contains(QLatin1String("version"), Qt::CaseInsensitive))
            continue;

        CredentialData cred;
        bool ok = false;
        if (looksLikeSecretsdump(line))
            ok = parseSecretsdump(line, cred);
        if (!ok)
            ok = parseDomainBackslash(line, cred);
        if (!ok)
            ok = parseImpacketSlash(line, cred);
        if (!ok)
            ok = parseUserPass(line, cred);

        if (!ok) {
            result.skipped++;
            continue;
        }

        if (cred.Username.isEmpty() && cred.Password.isEmpty()) {
            result.skipped++;
            continue;
        }

        if (cred.Storage.isEmpty())
            cred.Storage = defaultStorage;
        if (cred.Tag.isEmpty())
            cred.Tag = defaultTag;

        const QString key = dedupeKey(cred);
        if (seen.contains(key)) {
            result.skipped++;
            continue;
        }
        seen.insert(key);

        if (result.items.size() >= maxItems) {
            result.notes.append(QStringLiteral("Reached import limit (%1 items)").arg(maxItems));
            break;
        }
        result.items.append(cred);
    }

    if (result.items.isEmpty() && result.skipped == 0)
        result.notes.append(QStringLiteral("No credential lines found"));

    return result;
}
