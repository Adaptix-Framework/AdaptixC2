#include <Client/DockLayoutEngine.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <MainAdaptix.h>
#include <Client/Settings.h>

#include <algorithm>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/Group.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/qtcommon/View.h>

namespace {

QString secondaryFor(const QString& layout)
{
    if (layout == QLatin1String("main_left"))
        return QStringLiteral("bottom");
    if (layout == QLatin1String("main_right"))
        return QStringLiteral("left_bottom");
    if (layout == QLatin1String("quad"))
        return QStringLiteral("bottom_left");
    return QStringLiteral("bottom");
}

QString primaryFor(const QString& layout)
{
    if (layout == QLatin1String("main_left"))
        return QStringLiteral("top_left");
    if (layout == QLatin1String("main_right"))
        return QStringLiteral("left_top");
    if (layout == QLatin1String("quad"))
        return QStringLiteral("top_left");
    return QStringLiteral("top");
}

QString sideFor(const QString& layout)
{
    if (layout == QLatin1String("main_left"))
        return QStringLiteral("top_right");
    if (layout == QLatin1String("main_right"))
        return QStringLiteral("right");
    if (layout == QLatin1String("quad"))
        return QStringLiteral("top_right");
    return QStringLiteral("bottom");
}

} // namespace

QStringList DockLayoutEngine::layoutIds()
{
    return {
        QStringLiteral("main_right"),
        QStringLiteral("main_left"),
        QStringLiteral("split_v2"),
        QStringLiteral("quad"),
    };
}

QString DockLayoutEngine::layoutLabel(const QString& layoutId)
{
    if (layoutId == QLatin1String("main_left"))
        return QStringLiteral("Top split (left/right) + full bottom");
    if (layoutId == QLatin1String("main_right"))
        return QStringLiteral("Big right + left split (top/bottom)");
    if (layoutId == QLatin1String("quad"))
        return QStringLiteral("Four zones");
    return QStringLiteral("Two zones (top / bottom)");
}

QStringList DockLayoutEngine::zoneIdsForLayout(const QString& layoutId)
{
    if (layoutId == QLatin1String("main_left"))
        return { QStringLiteral("top_left"), QStringLiteral("top_right"), QStringLiteral("bottom") };
    if (layoutId == QLatin1String("main_right"))
        return { QStringLiteral("left_top"), QStringLiteral("left_bottom"), QStringLiteral("right") };
    if (layoutId == QLatin1String("quad"))
        return {
            QStringLiteral("top_left"), QStringLiteral("top_right"),
            QStringLiteral("bottom_left"), QStringLiteral("bottom_right")
        };
    return { QStringLiteral("top"), QStringLiteral("bottom") };
}

QString DockLayoutEngine::zoneLabel(const QString& zoneId)
{
    static const QMap<QString, QString> labels = {
        { QStringLiteral("top"), QStringLiteral("Top") },
        { QStringLiteral("bottom"), QStringLiteral("Bottom") },
        { QStringLiteral("left"), QStringLiteral("Left") },
        { QStringLiteral("right"), QStringLiteral("Right") },
        { QStringLiteral("left_top"), QStringLiteral("Left · Top") },
        { QStringLiteral("left_bottom"), QStringLiteral("Left · Bottom") },
        { QStringLiteral("top_left"), QStringLiteral("Top · Left") },
        { QStringLiteral("top_right"), QStringLiteral("Top · Right") },
        { QStringLiteral("bottom_left"), QStringLiteral("Bottom · Left") },
        { QStringLiteral("bottom_right"), QStringLiteral("Bottom · Right") },
    };
    return labels.value(zoneId, zoneId);
}

QStringList DockLayoutEngine::widgetIds()
{
    return {
        QStringLiteral("sessions"),
        QStringLiteral("graph"),
        QStringLiteral("logs"),
        QStringLiteral("chat"),
        QStringLiteral("listeners"),
        QStringLiteral("payloads"),
        QStringLiteral("tunnels"),
        QStringLiteral("downloads"),
        QStringLiteral("screenshots"),
        QStringLiteral("credentials"),
        QStringLiteral("targets"),
        QStringLiteral("scripts"),
        QStringLiteral("code_editor"),
        QStringLiteral("tasks"),
        QStringLiteral("tasks_output"),
        QStringLiteral("console"),
        QStringLiteral("files"),
        QStringLiteral("processes"),
        QStringLiteral("terminal"),
        QStringLiteral("shell"),
    };
}

QString DockLayoutEngine::widgetLabel(const QString& widgetId)
{
    static const QMap<QString, QString> labels = {
        { QStringLiteral("sessions"), QStringLiteral("Sessions") },
        { QStringLiteral("graph"), QStringLiteral("Session graph") },
        { QStringLiteral("logs"), QStringLiteral("Notifications / Logs") },
        { QStringLiteral("chat"), QStringLiteral("Chat") },
        { QStringLiteral("listeners"), QStringLiteral("Listeners") },
        { QStringLiteral("payloads"), QStringLiteral("Payload Store") },
        { QStringLiteral("tunnels"), QStringLiteral("Tunnels") },
        { QStringLiteral("downloads"), QStringLiteral("Files") },
        { QStringLiteral("screenshots"), QStringLiteral("Screenshots") },
        { QStringLiteral("credentials"), QStringLiteral("Credentials") },
        { QStringLiteral("targets"), QStringLiteral("Targets") },
        { QStringLiteral("scripts"), QStringLiteral("Scripts") },
        { QStringLiteral("code_editor"), QStringLiteral("Code editor") },
        { QStringLiteral("tasks"), QStringLiteral("Tasks") },
        { QStringLiteral("tasks_output"), QStringLiteral("Task output") },
        { QStringLiteral("console"), QStringLiteral("Agent console") },
        { QStringLiteral("files"), QStringLiteral("File browser") },
        { QStringLiteral("processes"), QStringLiteral("Process browser") },
        { QStringLiteral("terminal"), QStringLiteral("Remote terminal") },
        { QStringLiteral("shell"), QStringLiteral("Remote shell") },
    };
    return labels.value(widgetId, widgetId);
}

QString DockLayoutEngine::widgetIconPath(const QString& widgetId)
{
    // Match AdaptixWidget toolbar icons
    static const QMap<QString, QString> icons = {
        { QStringLiteral("sessions"), QStringLiteral(":/icons/format_list") },
        { QStringLiteral("graph"), QStringLiteral(":/icons/graph") },
        { QStringLiteral("tasks"), QStringLiteral(":/icons/job") },
        { QStringLiteral("tasks_output"), QStringLiteral(":/icons/job") },
        { QStringLiteral("listeners"), QStringLiteral(":/icons/listeners") },
        { QStringLiteral("payloads"), QStringLiteral(":/icons/kill") },
        { QStringLiteral("tunnels"), QStringLiteral(":/icons/vpn") },
        { QStringLiteral("logs"), QStringLiteral(":/icons/logs") },
        { QStringLiteral("downloads"), QStringLiteral(":/icons/downloads") },
        { QStringLiteral("targets"), QStringLiteral(":/icons/devices") },
        { QStringLiteral("credentials"), QStringLiteral(":/icons/key") },
        { QStringLiteral("screenshots"), QStringLiteral(":/icons/picture") },
        { QStringLiteral("chat"), QStringLiteral(":/icons/chat") },
        { QStringLiteral("scripts"), QStringLiteral(":/icons/folder_code") },
        { QStringLiteral("code_editor"), QStringLiteral(":/icons/code") },
        { QStringLiteral("console"), QStringLiteral(":/icons/terminal") },
        { QStringLiteral("files"), QStringLiteral(":/icons/folder") },
        { QStringLiteral("processes"), QStringLiteral(":/icons/computer") },
        { QStringLiteral("terminal"), QStringLiteral(":/icons/terminal") },
        { QStringLiteral("shell"), QStringLiteral(":/icons/terminal") },
    };
    return icons.value(widgetId);
}

QStringList DockLayoutEngine::startupCandidateIds()
{
    return {
        QStringLiteral("sessions"),
        QStringLiteral("graph"),
        QStringLiteral("logs"),
        QStringLiteral("chat"),
        QStringLiteral("listeners"),
        QStringLiteral("payloads"),
        QStringLiteral("tunnels"),
        QStringLiteral("downloads"),
        QStringLiteral("screenshots"),
        QStringLiteral("credentials"),
        QStringLiteral("targets"),
        QStringLiteral("scripts"),
        QStringLiteral("tasks"),
        QStringLiteral("code_editor"),
    };
}

DockLayoutSettings DockLayoutEngine::defaultsForLayout(const QString& layoutId)
{
    DockLayoutSettings s;
    s.layout = layoutIds().contains(layoutId) ? layoutId : QStringLiteral("main_right");
    s.startup = { QStringLiteral("sessions"), QStringLiteral("logs") };

    if (s.layout == QLatin1String("main_right")) {
        const QString leftTop = QStringLiteral("left_top");
        const QString leftBottom = QStringLiteral("left_bottom");
        const QString right = QStringLiteral("right");

        for (const QString& id : widgetIds()) {
            if (id == QLatin1String("sessions") || id == QLatin1String("graph"))
                s.openIn[id] = leftTop;
            else if (id == QLatin1String("code_editor") || id == QLatin1String("tasks") || id == QLatin1String("tasks_output") || id == QLatin1String("listeners") || id == QLatin1String("tunnels"))
                s.openIn[id] = right;
            else
                s.openIn[id] = leftBottom;
        }
        return s;
    }

    const QString primary = primaryFor(s.layout);
    const QString secondary = secondaryFor(s.layout);
    const QString side = sideFor(s.layout);

    for (const QString& id : widgetIds()) {
        if (id == QLatin1String("sessions") || id == QLatin1String("graph"))
            s.openIn[id] = primary;
        else if (id == QLatin1String("tasks") || id == QLatin1String("code_editor") || id == QLatin1String("tasks_output") || id == QLatin1String("listeners") || id == QLatin1String("tunnels"))
            s.openIn[id] = side;
        else
            s.openIn[id] = secondary;
    }
    return s;
}

void DockLayoutEngine::ensureValid(DockLayoutSettings& s)
{
    if (!layoutIds().contains(s.layout))
        s.layout = QStringLiteral("main_right");

    const QStringList zones = zoneIdsForLayout(s.layout);
    const DockLayoutSettings defs = defaultsForLayout(s.layout);

    if (s.openIn.isEmpty())
        s.openIn = defs.openIn;
    else {
        for (const QString& id : widgetIds()) {
            QString z = s.openIn.value(id);
            if (z.isEmpty() || !zones.contains(z))
                s.openIn[id] = defs.openIn.value(id, zones.isEmpty() ? QString() : zones.first());
        }
    }

    if (s.startup.isEmpty())
        s.startup = defs.startup;
    else {
        QStringList cleaned;
        for (const QString& id : s.startup) {
            if (startupCandidateIds().contains(id) && !cleaned.contains(id))
                cleaned.append(id);
        }
        if (cleaned.isEmpty())
            cleaned = defs.startup;
        s.startup = cleaned;
    }

    if (s.layout == QLatin1String("main_right")) {
        QStringList preferred;
        for (const QString& id : { QStringLiteral("sessions"), QStringLiteral("graph"), QStringLiteral("logs"), QStringLiteral("chat") }) {
            if (s.startup.contains(id))
                preferred.append(id);
        }
        for (const QString& id : s.startup) {
            if (!preferred.contains(id))
                preferred.append(id);
        }
        s.startup = preferred;

        const QString leftTop = QStringLiteral("left_top");
        const QString leftBottom = QStringLiteral("left_bottom");
        const QString right = QStringLiteral("right");
        for (const QString& id : widgetIds()) {
            const QString want = defs.openIn.value(id);
            if (want == leftTop || want == leftBottom || want == right)
                s.openIn[id] = want;
        }
    }
}

void DockLayoutEngine::attach(KDDockWidgets::QtWidgets::MainWindow* mainWindow, const QString& projectName)
{
    m_main = mainWindow;
    m_project = projectName;
}

void DockLayoutEngine::clear()
{
    m_zones.clear();
    m_zoneOrder.clear();
    m_zoneAnchors.clear();
    m_zoneDockNames.clear();
    m_layout.clear();
}

bool DockLayoutEngine::isZoneHost(const KDDockWidgets::QtWidgets::DockWidget* dock)
{
    if (!dock)
        return false;
    // Hosts created by makeHost(): "<project>-Zone-<zoneId>"
    return dock->uniqueName().contains(QLatin1String("-Zone-"));
}

bool DockLayoutEngine::isLiveDock(const KDDockWidgets::QtWidgets::DockWidget* dock)
{
    if (!dock)
        return false;
    auto* registry = KDDockWidgets::DockRegistry::self();
    if (!registry)
        return false;
    const QString name = dock->uniqueName();
    if (name.isEmpty())
        return false;
    KDDockWidgets::Core::DockWidget* core = registry->dockByName(name);
    if (!core)
        return false;
    auto* qt = qobject_cast<KDDockWidgets::QtWidgets::DockWidget*>(
        KDDockWidgets::QtCommon::View_qt::asQWidget(core));
    return qt == dock;
}

void DockLayoutEngine::rememberZoneDock(const QString& zoneId, KDDockWidgets::QtWidgets::DockWidget* dock) const
{
    if (zoneId.isEmpty() || !dock || isZoneHost(dock))
        return;
    if (!isLiveDock(dock))
        return;
    const QString name = dock->uniqueName();
    QStringList& names = m_zoneDockNames[zoneId];
    if (!names.contains(name))
        names.append(name);
    m_zoneAnchors[zoneId] = dock;
}

KDDockWidgets::QtWidgets::DockWidget* DockLayoutEngine::liveAnchor(const QString& zoneId) const
{
    auto asQt = [](KDDockWidgets::Core::DockWidget* cdw) -> KDDockWidgets::QtWidgets::DockWidget* {
        if (!cdw)
            return nullptr;
        return qobject_cast<KDDockWidgets::QtWidgets::DockWidget*>(
            KDDockWidgets::QtCommon::View_qt::asQWidget(cdw));
    };

    auto tryOpen = [&](KDDockWidgets::QtWidgets::DockWidget* a) -> KDDockWidgets::QtWidgets::DockWidget* {
        if (!a || !isLiveDock(a))
            return nullptr;
        if (a->isOpen())
            return a;
        if (auto* g = a->group()) {
            for (KDDockWidgets::Core::DockWidget* cdw : g->dockWidgets()) {
                if (!cdw || !cdw->isOpen())
                    continue;
                if (auto* qt = asQt(cdw)) {
                    if (isLiveDock(qt))
                        return qt;
                }
            }
        }
        return nullptr;
    };

    if (m_zoneAnchors.contains(zoneId) && m_zoneAnchors.value(zoneId).isNull())
        m_zoneAnchors.remove(zoneId);

    if (auto* found = tryOpen(m_zoneAnchors.value(zoneId).data())) {
        m_zoneAnchors[zoneId] = found;
        return found;
    }

    if (auto* registry = KDDockWidgets::DockRegistry::self()) {
        QStringList& names = m_zoneDockNames[zoneId];
        for (int i = names.size() - 1; i >= 0; --i) {
            const QString& name = names.at(i);
            KDDockWidgets::Core::DockWidget* cdw = registry->dockByName(name);
            if (!cdw) {
                names.removeAt(i);
                continue;
            }
            if (auto* qt = tryOpen(asQt(cdw))) {
                m_zoneAnchors[zoneId] = qt;
                return qt;
            }
        }
    }

    if (auto* host = m_zones.value(zoneId, nullptr)) {
        if (isLiveDock(host) && (host->isOpen() || host->group()))
            return host;
    }

    m_zoneAnchors.remove(zoneId);
    return nullptr;
}

KDDockWidgets::QtWidgets::DockWidget* DockLayoutEngine::relativeRef(const QString& zoneId) const
{
    if (auto* a = liveAnchor(zoneId))
        return a;
    auto* h = m_zones.value(zoneId, nullptr);
    if (h && isLiveDock(h) && (h->group() || h->isOpen()))
        return h;
    return nullptr;
}

KDDockWidgets::QtWidgets::DockWidget* DockLayoutEngine::makeHost(const QString& zoneId) const
{
    if (!m_main)
        return nullptr;
    const QString name = m_project + QStringLiteral("-Zone-") + zoneId;
    auto* host = new KDDockWidgets::QtWidgets::DockWidget(
        name, KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    host->setWidget(new QWidget());
    host->setTitle(zoneLabel(zoneId));
    return host;
}

void DockLayoutEngine::addHost(const QString& zoneId, KDDockWidgets::Location loc, KDDockWidgets::QtWidgets::DockWidget* relativeTo) const
{
    if (m_zones.contains(zoneId))
        return;
    auto* host = makeHost(zoneId);
    if (!host || !m_main)
        return;
    m_main->addDockWidget(host, loc, relativeTo);
    m_zones.insert(zoneId, host);
    m_zoneOrder.append(zoneId);
}

void DockLayoutEngine::build(const DockLayoutSettings& settings)
{
    clear();
    if (!m_main)
        return;

    DockLayoutSettings s = settings;
    ensureValid(s);
    m_layout = s.layout;
}

KDDockWidgets::QtWidgets::DockWidget* DockLayoutEngine::ensureZone(const QString& zoneId) const
{
    if (!m_main || zoneId.isEmpty())
        return nullptr;

    if (auto* existing = m_zones.value(zoneId, nullptr)) {
        if (isLiveDock(existing)) {
            if (existing->group() || existing->isOpen())
                return existing;
            if (!liveAnchor(zoneId)) {
                existing->open();
                return existing;
            }
            return existing;
        }
        m_zones.remove(zoneId);
        m_zoneOrder.removeAll(zoneId);
    }

    if (!zoneIdsForLayout(m_layout).contains(zoneId))
        return nullptr;

    if (m_layout == QLatin1String("main_right")) {
        if (zoneId == QLatin1String("left_top")) {
            if (auto* right = relativeRef(QStringLiteral("right")))
                addHost(zoneId, KDDockWidgets::Location_OnLeft, right);
            else
                addHost(zoneId, KDDockWidgets::Location_OnLeft);
        } else if (zoneId == QLatin1String("left_bottom")) {
            if (auto* leftTop = relativeRef(QStringLiteral("left_top")))
                addHost(zoneId, KDDockWidgets::Location_OnBottom, leftTop);
            else if (auto* right = relativeRef(QStringLiteral("right")))
                addHost(zoneId, KDDockWidgets::Location_OnLeft, right);
            else
                addHost(zoneId, KDDockWidgets::Location_OnBottom);
        } else if (zoneId == QLatin1String("right")) {
            addHost(zoneId, KDDockWidgets::Location_OnRight, nullptr);
        }
    } else if (m_layout == QLatin1String("main_left")) {
        if (zoneId == QLatin1String("top_left")) {
            addHost(zoneId, KDDockWidgets::Location_OnTop);
        } else if (zoneId == QLatin1String("top_right")) {
            if (auto* tl = relativeRef(QStringLiteral("top_left")))
                addHost(zoneId, KDDockWidgets::Location_OnRight, tl);
            else
                addHost(zoneId, KDDockWidgets::Location_OnTop);
        } else if (zoneId == QLatin1String("bottom")) {
            addHost(zoneId, KDDockWidgets::Location_OnBottom);
        }
    } else if (m_layout == QLatin1String("quad")) {
        if (zoneId == QLatin1String("top_left")) {
            addHost(zoneId, KDDockWidgets::Location_OnTop);
        } else if (zoneId == QLatin1String("top_right")) {
            if (auto* tl = relativeRef(QStringLiteral("top_left")))
                addHost(zoneId, KDDockWidgets::Location_OnRight, tl);
            else
                addHost(zoneId, KDDockWidgets::Location_OnTop);
        } else if (zoneId == QLatin1String("bottom_left")) {
            if (auto* tl = relativeRef(QStringLiteral("top_left")))
                addHost(zoneId, KDDockWidgets::Location_OnBottom, tl);
            else
                addHost(zoneId, KDDockWidgets::Location_OnBottom);
        } else if (zoneId == QLatin1String("bottom_right")) {
            if (auto* bl = relativeRef(QStringLiteral("bottom_left")))
                addHost(zoneId, KDDockWidgets::Location_OnRight, bl);
            else if (auto* tr = relativeRef(QStringLiteral("top_right")))
                addHost(zoneId, KDDockWidgets::Location_OnBottom, tr);
            else
                addHost(zoneId, KDDockWidgets::Location_OnBottom);
        }
    } else {
        if (zoneId == QLatin1String("top")) {
            addHost(zoneId, KDDockWidgets::Location_OnTop);
        } else if (zoneId == QLatin1String("bottom")) {
            if (auto* top = relativeRef(QStringLiteral("top")))
                addHost(zoneId, KDDockWidgets::Location_OnBottom, top);
            else
                addHost(zoneId, KDDockWidgets::Location_OnBottom);
        }
    }

    return m_zones.value(zoneId, nullptr);
}

KDDockWidgets::QtWidgets::DockWidget* DockLayoutEngine::zoneHost(const QString& zoneId) const
{
    return m_zones.value(zoneId, nullptr);
}

KDDockWidgets::QtWidgets::DockWidget* DockLayoutEngine::primaryHost() const
{
    return ensureZone(primaryFor(m_layout.isEmpty() ? QStringLiteral("main_right") : m_layout));
}

KDDockWidgets::QtWidgets::DockWidget* DockLayoutEngine::secondaryHost() const
{
    return ensureZone(secondaryFor(m_layout.isEmpty() ? QStringLiteral("main_right") : m_layout));
}

QString DockLayoutEngine::resolveZone(const QString& widgetId, const QString& zoneOverride, const DockLayoutSettings& settings) const
{
    DockLayoutSettings s = settings;
    ensureValid(s);
    const QStringList zones = zoneIdsForLayout(s.layout);

    auto pick = [&](const QString& z) -> QString {
        if (!z.isEmpty() && zones.contains(z))
            return z;
        if (z == QLatin1String("top") || z == QLatin1String("primary") || z == QLatin1String("main"))
            return primaryFor(s.layout);
        if (z == QLatin1String("bottom") || z == QLatin1String("secondary") || z == QLatin1String("work"))
            return secondaryFor(s.layout);
        if (z == QLatin1String("side"))
            return sideFor(s.layout);
        return QString();
    };

    QString z = pick(zoneOverride);
    if (z.isEmpty())
        z = pick(s.openIn.value(widgetId));
    if (z.isEmpty() && (widgetId == QLatin1String("logs") || widgetId == QLatin1String("chat") || widgetId == QLatin1String("console")))
        z = secondaryFor(s.layout);
    if (z.isEmpty())
        z = secondaryFor(s.layout);
    if (!zones.contains(z) && !zones.isEmpty())
        z = zones.first();
    return z;
}

void DockLayoutEngine::fixMainRightLeftColumn() const
{
    if (m_layout != QLatin1String("main_right") || !m_main)
        return;

    auto* top = liveAnchor(QStringLiteral("left_top"));
    auto* bot = liveAnchor(QStringLiteral("left_bottom"));
    if (!top || !bot)
        return;

    if (bot->isOpen())
        bot->close();

    m_main->addDockWidget(bot, KDDockWidgets::Location_OnBottom, top);
    m_zoneAnchors[QStringLiteral("left_bottom")] = bot;
}

bool DockLayoutEngine::placeFirstDirect(const QString& zoneId, KDDockWidgets::QtWidgets::DockWidget* dock) const
{
    if (!m_main || !dock || zoneId.isEmpty())
        return false;

    using Loc = KDDockWidgets::Location;

    auto add = [&](Loc loc, KDDockWidgets::QtWidgets::DockWidget* relativeTo) {
        m_main->addDockWidget(dock, loc, relativeTo);
        rememberZoneDock(zoneId, dock);
    };

    if (m_layout == QLatin1String("main_right")) {
        if (zoneId == QLatin1String("left_top")) {
            if (auto* lb = liveAnchor(QStringLiteral("left_bottom")))
                add(Loc::Location_OnTop, lb);
            else if (auto* r = liveAnchor(QStringLiteral("right")))
                add(Loc::Location_OnLeft, r);
            else
                add(Loc::Location_OnTop, nullptr);
            return true;
        }
        if (zoneId == QLatin1String("left_bottom")) {
            if (auto* lt = liveAnchor(QStringLiteral("left_top")))
                add(Loc::Location_OnBottom, lt);
            else if (auto* r = liveAnchor(QStringLiteral("right")))
                add(Loc::Location_OnLeft, r);
            else
                add(Loc::Location_OnBottom, nullptr);
            return true;
        }
        if (zoneId == QLatin1String("right")) {
            add(Loc::Location_OnRight, nullptr);
            return true;
        }
        return false;
    }

    if (m_layout == QLatin1String("main_left")) {
        if (zoneId == QLatin1String("top_left")) {
            add(Loc::Location_OnTop, nullptr);
            return true;
        }
        if (zoneId == QLatin1String("top_right")) {
            add(Loc::Location_OnRight, relativeRef(QStringLiteral("top_left")));
            return true;
        }
        if (zoneId == QLatin1String("bottom")) {
            add(Loc::Location_OnBottom, nullptr);
            return true;
        }
        return false;
    }

    if (m_layout == QLatin1String("split_v2")) {
        if (zoneId == QLatin1String("top")) {
            add(Loc::Location_OnTop, nullptr);
            return true;
        }
        if (zoneId == QLatin1String("bottom")) {
            add(Loc::Location_OnBottom, relativeRef(QStringLiteral("top")));
            return true;
        }
        return false;
    }

    if (m_layout == QLatin1String("quad")) {
        if (zoneId == QLatin1String("top_left")) {
            add(Loc::Location_OnTop, nullptr);
            return true;
        }
        if (zoneId == QLatin1String("top_right")) {
            add(Loc::Location_OnRight, relativeRef(QStringLiteral("top_left")));
            return true;
        }
        if (zoneId == QLatin1String("bottom_left")) {
            add(Loc::Location_OnBottom, relativeRef(QStringLiteral("top_left")));
            return true;
        }
        if (zoneId == QLatin1String("bottom_right")) {
            if (auto* bl = relativeRef(QStringLiteral("bottom_left")))
                add(Loc::Location_OnRight, bl);
            else
                add(Loc::Location_OnBottom, relativeRef(QStringLiteral("top_right")));
            return true;
        }
    }
    return false;
}

void DockLayoutEngine::placeInZone(const QString& zoneId, KDDockWidgets::QtWidgets::DockWidget* dock, AdaptixWidget* owner) const
{
    if (!dock || !owner)
        return;

    if (dock->isOpen()) {
        dock->setAsCurrentTab();
        rememberZoneDock(zoneId, dock);
        return;
    }

    if (auto* anchor = liveAnchor(zoneId)) {
        owner->PlaceDock(anchor, dock);
        rememberZoneDock(zoneId, dock);
        return;
    }

    if (placeFirstDirect(zoneId, dock))
        return;

    auto* host = ensureZone(zoneId);
    if (!host)
        host = ensureZone(secondaryFor(m_layout));
    if (!host)
        host = ensureZone(primaryFor(m_layout));
    if (!host)
        return;
    owner->PlaceDock(host, dock);
    rememberZoneDock(zoneId, dock);
}

void DockLayoutEngine::placeWidget(const QString& widgetId, KDDockWidgets::QtWidgets::DockWidget* dock, AdaptixWidget* owner, const QString& zoneOverride) const
{
    if (!dock || !owner)
        return;
    DockLayoutSettings s;
    if (GlobalClient && GlobalClient->settings)
        s = GlobalClient->settings->data.DockLayout;
    else
        s = defaultsForLayout(QStringLiteral("main_right"));
    ensureValid(s);

    if (GlobalClient && GlobalClient->settings)
        GlobalClient->settings->data.DockLayout = s;

    QString zone = resolveZone(widgetId, zoneOverride, s);

    if (s.layout == QLatin1String("main_right") && zoneOverride.isEmpty()) {
        const QString want = defaultsForLayout(QStringLiteral("main_right")).openIn.value(widgetId);
        if (!want.isEmpty())
            zone = want;
    }

    placeInZone(zone, dock, owner);
}

void DockLayoutEngine::openStartup(AdaptixWidget* owner, const DockLayoutSettings& settings) const
{
    if (!owner)
        return;
    DockLayoutSettings s = settings;
    ensureValid(s);

    if (GlobalClient && GlobalClient->settings)
        GlobalClient->settings->data.DockLayout = s;

    auto zoneRank = [&](const QString& widgetId) -> int {
        const QString z = resolveZone(widgetId, QString(), s);
        if (s.layout == QLatin1String("main_right")) {
            if (z == QLatin1String("left_top"))
                return 0;
            if (z == QLatin1String("left_bottom"))
                return 1;
            if (z == QLatin1String("right"))
                return 2;
        } else if (s.layout == QLatin1String("main_left")) {
            if (z == QLatin1String("top_left") || z == QLatin1String("top_right"))
                return 0;
            if (z == QLatin1String("bottom"))
                return 1;
        } else {
            if (z == QLatin1String("top") || z == QLatin1String("top_left") || z == QLatin1String("top_right"))
                return 0;
            return 1;
        }
        return 5;
    };

    QStringList ordered = s.startup;

    std::stable_sort(ordered.begin(), ordered.end(), [&](const QString& a, const QString& b) {
        return zoneRank(a) < zoneRank(b);
    });

    auto openOne = [&](const QString& id) {
        if (id == QLatin1String("sessions"))
            owner->SetSessionsTableUI();
        else if (id == QLatin1String("graph"))
            owner->SetGraphUI();
        else if (id == QLatin1String("logs"))
            owner->LoadLogsUI();
        else if (id == QLatin1String("chat"))
            owner->LoadChatUI();
        else if (id == QLatin1String("listeners"))
            owner->LoadListenersUI();
        else if (id == QLatin1String("payloads"))
            owner->LoadPayloadsUI();
        else if (id == QLatin1String("tunnels"))
            owner->LoadTunnelsUI();
        else if (id == QLatin1String("downloads"))
            owner->LoadFilesUI(0);
        else if (id == QLatin1String("screenshots"))
            owner->LoadScreenshotsUI();
        else if (id == QLatin1String("credentials"))
            owner->LoadCredentialsUI();
        else if (id == QLatin1String("targets"))
            owner->LoadTargetsUI();
        else if (id == QLatin1String("scripts"))
            owner->LoadScriptsUI(0);
        else if (id == QLatin1String("tasks"))
            owner->SetTasksUI();
        else if (id == QLatin1String("code_editor"))
            owner->LoadCodeEditorUI();
    };

    for (const QString& id : ordered)
        openOne(id);

    fixMainRightLeftColumn();
}

QList<KDDockWidgets::QtWidgets::DockWidget*> DockLayoutEngine::allHosts() const
{
    return m_zones.values();
}
