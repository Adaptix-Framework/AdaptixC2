#include <Utils/Import/TargetsImport.h>

#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>

namespace {

QString dedupeKey(const TargetData& t)
{
    if (!t.Address.isEmpty())
        return QLatin1String("a:") + t.Address.toLower();
    return QLatin1String("c:") + t.Computer.toLower() + QLatin1Char('@') + t.Domain.toLower();
}

bool isIPv4(const QString& s)
{
    static const QRegularExpression re(QStringLiteral(R"(^(?:\d{1,3}\.){3}\d{1,3}$)"));
    return re.match(s).hasMatch();
}

bool isIPv6ish(const QString& s)
{
    return s.contains(QLatin1Char(':')) && !s.contains(QLatin1Char(' '));
}

int detectOs(const QString& text, QString* osDesc = nullptr)
{
    const QString lower = text.toLower();
    if (lower.contains(QLatin1String("windows")) || lower.contains(QLatin1String("microsoft")) || lower.contains(QLatin1String("win10")) || lower.contains(QLatin1String("win11")) || lower.contains(QLatin1String("server 20"))) {
        if (osDesc) {
            *osDesc = text.trimmed();
            if (osDesc->size() > 80)
                *osDesc = osDesc->left(80);
        }
        return OS_WINDOWS;
    }
    if (lower.contains(QLatin1String("linux")) || lower.contains(QLatin1String("ubuntu")) || lower.contains(QLatin1String("debian")) || lower.contains(QLatin1String("centos")) || lower.contains(QLatin1String("redhat")) || lower.contains(QLatin1String("unix"))) {
        if (osDesc) {
            *osDesc = text.trimmed();
            if (osDesc->size() > 80)
                *osDesc = osDesc->left(80);
        }
        return OS_LINUX;
    }
    if (lower.contains(QLatin1String("macos")) || lower.contains(QLatin1String("mac os")) || lower.contains(QLatin1String("darwin")) || lower.contains(QLatin1String("apple"))) {
        if (osDesc) {
            *osDesc = text.trimmed();
            if (osDesc->size() > 80)
                *osDesc = osDesc->left(80);
        }
        return OS_MAC;
    }
    return 0;
}

void addTarget(QList<TargetData>& items, QSet<QString>& seen, TargetData t, const QString& defaultTag, bool defaultAlive, int maxItems, int* skipped)
{
    if (t.Address.isEmpty() && t.Computer.isEmpty()) {
        if (skipped) (*skipped)++;
        return;
    }
    if (t.Tag.isEmpty())
        t.Tag = defaultTag;
    t.Alive = defaultAlive;

    if (t.Computer.compare(QLatin1String("None"), Qt::CaseInsensitive) == 0 || t.Computer == QLatin1String("*") || t.Computer == QLatin1String("-") || t.Computer.compare(QLatin1String("N/A"), Qt::CaseInsensitive) == 0)
        t.Computer.clear();

    const QString key = dedupeKey(t);
    if (seen.contains(key)) {
        if (skipped) (*skipped)++;
        return;
    }
    if (items.size() >= maxItems) {
        if (skipped) (*skipped)++;
        return;
    }
    seen.insert(key);
    items.append(t);
}



bool parseNmapXml(const QString& text, QList<TargetData>& items, QSet<QString>& seen, const QString& defaultTag, bool defaultAlive, int maxItems, int* skipped)
{
    if (!text.contains(QLatin1String("<nmaprun"), Qt::CaseInsensitive) && !text.contains(QLatin1String("<host"), Qt::CaseInsensitive))
        return false;

    QXmlStreamReader xml(text);
    TargetData current;
    bool inHost = false;
    bool hostUp = true;
    QString bestHostname;
    QString osMatch;

    auto flush = [&]() {
        if (!inHost)
            return;
        current.Alive = hostUp ? defaultAlive : false;
        if (!bestHostname.isEmpty() && current.Computer.isEmpty())
            current.Computer = bestHostname;
        if (!osMatch.isEmpty()) {
            current.OsDesc = osMatch;
            current.Os = detectOs(osMatch);
        }
        addTarget(items, seen, current, defaultTag, current.Alive, maxItems, skipped);
        current = {};
        bestHostname.clear();
        osMatch.clear();
        hostUp = true;
        inHost = false;
    };

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const QStringView name = xml.name();
            if (name == QLatin1String("host")) {
                flush();
                inHost = true;
                current = {};
                hostUp = true;
            } else if (inHost && name == QLatin1String("status")) {
                const QString state = xml.attributes().value(QLatin1String("state")).toString();
                hostUp = (state.compare(QLatin1String("up"), Qt::CaseInsensitive) == 0);
            } else if (inHost && name == QLatin1String("address")) {
                const QString addrType = xml.attributes().value(QLatin1String("addrtype")).toString();
                const QString addr = xml.attributes().value(QLatin1String("addr")).toString();
                if (addrType == QLatin1String("ipv4") || addrType == QLatin1String("ipv6") || current.Address.isEmpty())
                    current.Address = addr;
            } else if (inHost && name == QLatin1String("hostname")) {
                const QString hn = xml.attributes().value(QLatin1String("name")).toString();
                const QString type = xml.attributes().value(QLatin1String("type")).toString();
                if (!hn.isEmpty()) {
                    if (type == QLatin1String("user") || bestHostname.isEmpty())
                        bestHostname = hn;
                }
            } else if (inHost && name == QLatin1String("osmatch")) {
                const QString n = xml.attributes().value(QLatin1String("name")).toString();
                if (!n.isEmpty() && osMatch.isEmpty())
                    osMatch = n;
            }
        } else if (xml.isEndElement() && xml.name() == QLatin1String("host")) {
            flush();
        }
    }

    flush();
    return !items.isEmpty() || text.contains(QLatin1String("<host"));
}



int parseNmapGrepable(const QString& text, QList<TargetData>& items, QSet<QString>& seen, const QString& defaultTag, bool defaultAlive, int maxItems, int* skipped)
{
    static const QRegularExpression re(QStringLiteral(R"(^Host:\s+(\S+)(?:\s+\(([^)]*)\))?(?:\s+Status:\s*(\S+))?)"), QRegularExpression::CaseInsensitiveOption);

    int count = 0;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral(R"(\r\n|\n|\r)")));
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (!line.startsWith(QLatin1String("Host:"), Qt::CaseInsensitive))
            continue;

        const auto m = re.match(line);
        if (!m.hasMatch()) {
            if (skipped) (*skipped)++;
            continue;
        }

        TargetData t;
        t.Address = m.captured(1).trimmed();
        t.Computer = m.captured(2).trimmed();
        const QString status = m.captured(3).trimmed();
        if (!status.isEmpty())
            t.Alive = status.compare(QLatin1String("Up"), Qt::CaseInsensitive) == 0;
        else
            t.Alive = defaultAlive;

        if (line.contains(QLatin1String("Ports:"), Qt::CaseInsensitive)) {
            static const QRegularExpression portRe(QStringLiteral(R"((\d+)/(open)/([^/]*)/([^/]*)/([^/]*)/([^/]*))"), QRegularExpression::CaseInsensitiveOption);
            QStringList services;
            auto it = portRe.globalMatch(line);
            while (it.hasNext() && services.size() < 12) {
                const auto pm = it.next();
                QString svc = pm.captured(5).trimmed();
                if (svc.isEmpty())
                    svc = pm.captured(1);
                if (!svc.isEmpty() && !services.contains(svc))
                    services.append(svc);
            }
            if (!services.isEmpty())
                t.Info = services.join(QLatin1String(", "));
        }

        const int before = items.size();
        addTarget(items, seen, t, defaultTag, t.Alive, maxItems, skipped);
        if (items.size() > before)
            count++;
    }
    return count;
}



int parseNmapNormal(const QString& text, QList<TargetData>& items, QSet<QString>& seen, const QString& defaultTag, bool defaultAlive, int maxItems, int* skipped)
{
    static const QRegularExpression reReport(QStringLiteral(R"(^Nmap scan report for (.+)$)"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reHostIp(QStringLiteral(R"(^(.+?)\s+\(([^)]+)\)\s*$)"));

    int count = 0;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral(R"(\r\n|\n|\r)")));
    TargetData pending;
    bool havePending = false;

    auto flushPending = [&]() {
        if (!havePending)
            return;
        const int before = items.size();
        addTarget(items, seen, pending, defaultTag, pending.Alive, maxItems, skipped);
        if (items.size() > before)
            count++;
        pending = {};
        havePending = false;
    };

    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1String("Nmap scan report for"), Qt::CaseInsensitive)) {
            flushPending();
            const auto m = reReport.match(line);
            if (!m.hasMatch())
                continue;
            const QString rest = m.captured(1).trimmed();
            pending = {};
            pending.Alive = defaultAlive;
            const auto hm = reHostIp.match(rest);
            if (hm.hasMatch()) {
                const QString a = hm.captured(1).trimmed();
                const QString b = hm.captured(2).trimmed();
                if (isIPv4(b) || isIPv6ish(b)) {
                    pending.Computer = a;
                    pending.Address = b;
                } else if (isIPv4(a) || isIPv6ish(a)) {
                    pending.Address = a;
                    pending.Computer = b;
                } else {
                    pending.Computer = a;
                    pending.Address = b;
                }
            } else if (isIPv4(rest) || isIPv6ish(rest)) {
                pending.Address = rest;
            } else {
                pending.Computer = rest;
            }
            if (pending.Computer.contains(QLatin1Char('.')) && !isIPv4(pending.Computer)) {
                const int dot = pending.Computer.indexOf(QLatin1Char('.'));
                if (dot > 0 && pending.Domain.isEmpty()) {
                    pending.Domain = pending.Computer.mid(dot + 1);
                }
            }
            havePending = true;
            continue;
        }
        if (havePending && line.startsWith(QLatin1String("Host is up"), Qt::CaseInsensitive)) {
            pending.Alive = true;
            continue;
        }
        if (havePending && line.startsWith(QLatin1String("Host is down"), Qt::CaseInsensitive)) {
            pending.Alive = false;
            continue;
        }
        if (havePending && (line.startsWith(QLatin1String("OS details:"), Qt::CaseInsensitive) || line.startsWith(QLatin1String("Running:"), Qt::CaseInsensitive) || line.startsWith(QLatin1String("OS CPE:"), Qt::CaseInsensitive))) {
            QString desc = line;
            const int colon = desc.indexOf(QLatin1Char(':'));
            if (colon >= 0)
                desc = desc.mid(colon + 1).trimmed();
            pending.OsDesc = desc;
            pending.Os = detectOs(desc);
            continue;
        }
    }
    flushPending();
    return count;
}



int parseNetExec(const QString& text, QList<TargetData>& items, QSet<QString>& seen, const QString& defaultTag, bool defaultAlive, int maxItems, int* skipped)
{
    static const QRegularExpression re( QStringLiteral(
            R"(^(SMB|SSH|WINRM|RDP|LDAP|MSSQL|FTP|WMI|VNC|HTTP|HTTPS|NFS|DNS)\s+)"
            R"((\d{1,3}(?:\.\d{1,3}){3}|[0-9A-Fa-f:]+)\s+)"
            R"((\d+)\s+)"
            R"((\S+)\s+)"
            R"(\[([*+x!X-])\]\s*(.*)$)"),
        QRegularExpression::CaseInsensitiveOption);

    static const QRegularExpression reLoose( QStringLiteral(
            R"(^(SMB|SSH|WINRM|RDP|LDAP|MSSQL|FTP|WMI|VNC|HTTP|HTTPS)\s+)"
            R"((\d{1,3}(?:\.\d{1,3}){3})\s+)"
            R"((\d+)\s+)"
            R"((\S+)\s+)"
            R"((.*)$)"),
        QRegularExpression::CaseInsensitiveOption);

    static const QRegularExpression reDomain( QStringLiteral(R"(\(domain:([^)]+)\))"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression reName( QStringLiteral(R"(\(name:([^)]+)\))"), QRegularExpression::CaseInsensitiveOption);

    int count = 0;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral(R"(\r\n|\n|\r)")));
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1String("[*]")) || line.startsWith(QLatin1String("[-]")) || line.startsWith(QLatin1String("[!]")) || line.startsWith(QLatin1String("[+]")))
            continue;

        QString proto, ip, port, host, rest;
        auto m = re.match(line);
        if (m.hasMatch()) {
            proto = m.captured(1);
            ip = m.captured(2);
            port = m.captured(3);
            host = m.captured(4);
            rest = m.captured(6);
        } else {
            m = reLoose.match(line);
            if (!m.hasMatch())
                continue;
            proto = m.captured(1);
            ip = m.captured(2);
            port = m.captured(3);
            host = m.captured(4);
            rest = m.captured(5);
            if (rest.isEmpty() && host.isEmpty())
                continue;
        }

        TargetData t;
        t.Address = ip.trimmed();
        t.Computer = host.trimmed();
        t.Alive = defaultAlive;

        const auto dm = reDomain.match(rest);
        if (dm.hasMatch())
            t.Domain = dm.captured(1).trimmed();
        const auto nm = reName.match(rest);
        if (nm.hasMatch()) {
            const QString n = nm.captured(1).trimmed();
            if (!n.isEmpty())
                t.Computer = n;
        }

        QString osDesc;
        t.Os = detectOs(rest, &osDesc);
        if (!osDesc.isEmpty())
            t.OsDesc = osDesc;

        QString info = proto.toUpper() + QLatin1Char('/') + port;
        QString shortRest = rest;
        const int paren = shortRest.indexOf(QLatin1Char('('));
        if (paren > 0)
            shortRest = shortRest.left(paren).trimmed();
        if (!shortRest.isEmpty() && shortRest.size() < 60)
            info += QLatin1String(" — ") + shortRest;
        t.Info = info;

        const int before = items.size();
        addTarget(items, seen, t, defaultTag, t.Alive, maxItems, skipped);
        if (items.size() > before)
            count++;
    }
    return count;
}

}

TargetsImportResult parseTargetsImport(const QString& text, const QString& defaultTag, bool defaultAlive, int maxItems)
{
    TargetsImportResult result;
    QSet<QString> seen;
    const QStringList lines = text.split(QRegularExpression(QStringLiteral(R"(\r\n|\n|\r)")));
    result.totalLines = lines.size();

    if (text.contains(QLatin1String("<nmaprun"), Qt::CaseInsensitive) || (text.contains(QLatin1String("<?xml"), Qt::CaseInsensitive) && text.contains(QLatin1String("<host"), Qt::CaseInsensitive))) {
        QList<TargetData> items;
        int skipped = 0;
        if (parseNmapXml(text, items, seen, defaultTag, defaultAlive, maxItems, &skipped)) {
            result.items = items;
            result.skipped = skipped;
            result.detectedFormat = QStringLiteral("nmap XML");
            if (result.items.size() >= maxItems)
                result.notes.append(QStringLiteral("Reached import limit (%1 items)").arg(maxItems));
            return result;
        }
    }
    {
        int skipped = 0;
        const int g = parseNmapGrepable(text, result.items, seen, defaultTag, defaultAlive, maxItems, &skipped);
        result.skipped += skipped;
        if (g > 0)
            result.detectedFormat = QStringLiteral("nmap greppable");
    }
    {
        int skipped = 0;
        const int n = parseNmapNormal(text, result.items, seen, defaultTag, defaultAlive, maxItems, &skipped);
        result.skipped += skipped;
        if (n > 0) {
            if (result.detectedFormat.isEmpty())
                result.detectedFormat = QStringLiteral("nmap normal");
            else if (!result.detectedFormat.contains(QLatin1String("nmap")))
                result.detectedFormat += QStringLiteral(" + nmap normal");
            else if (!result.detectedFormat.contains(QLatin1String("normal")))
                result.detectedFormat += QStringLiteral(" + normal");
        }
    }
    {
        int skipped = 0;
        const int nx = parseNetExec(text, result.items, seen, defaultTag, defaultAlive, maxItems, &skipped);
        result.skipped += skipped;
        if (nx > 0) {
            if (result.detectedFormat.isEmpty())
                result.detectedFormat = QStringLiteral("NetExec");
            else
                result.detectedFormat += QStringLiteral(" + NetExec");
        }
    }

    if (result.items.isEmpty()) {
        result.notes.append(QStringLiteral("No nmap/NetExec hosts found"));
    } else if (result.items.size() >= maxItems) {
        result.notes.append(QStringLiteral("Reached import limit (%1 items)").arg(maxItems));
    }

    return result;
}
