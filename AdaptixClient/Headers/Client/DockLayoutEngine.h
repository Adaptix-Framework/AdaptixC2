#ifndef ADAPTIXCLIENT_DOCKLAYOUTENGINE_H
#define ADAPTIXCLIENT_DOCKLAYOUTENGINE_H

#include <main.h>
#include <QPointer>
#include <kddockwidgets/qtwidgets/views/DockWidget.h>
#include <kddockwidgets/qtwidgets/views/MainWindow.h>

class AdaptixWidget;

class DockLayoutEngine
{
    KDDockWidgets::QtWidgets::DockWidget* makeHost(const QString& zoneId) const;
    void addHost(const QString& zoneId, KDDockWidgets::Location loc, KDDockWidgets::QtWidgets::DockWidget* relativeTo = nullptr) const;
    KDDockWidgets::QtWidgets::DockWidget* ensureZone(const QString& zoneId) const;
    KDDockWidgets::QtWidgets::DockWidget* liveAnchor(const QString& zoneId) const;
    KDDockWidgets::QtWidgets::DockWidget* relativeRef(const QString& zoneId) const;
    bool placeFirstDirect(const QString& zoneId, KDDockWidgets::QtWidgets::DockWidget* dock) const;
    void fixMainRightLeftColumn() const;
    void rememberZoneDock(const QString& zoneId, KDDockWidgets::QtWidgets::DockWidget* dock) const;
    static bool isZoneHost(const KDDockWidgets::QtWidgets::DockWidget* dock);
    static bool isLiveDock(const KDDockWidgets::QtWidgets::DockWidget* dock);

    KDDockWidgets::QtWidgets::MainWindow* m_main = nullptr;
    QString m_project;
    QString m_layout;

    mutable QStringList m_zoneOrder;
    mutable QMap<QString, KDDockWidgets::QtWidgets::DockWidget*> m_zones;
    mutable QMap<QString, QPointer<KDDockWidgets::QtWidgets::DockWidget>> m_zoneAnchors;
    mutable QMap<QString, QStringList> m_zoneDockNames;

public:
    static QStringList layoutIds();
    static QString     layoutLabel(const QString& layoutId);
    static QStringList zoneIdsForLayout(const QString& layoutId);
    static QString     zoneLabel(const QString& zoneId);
    static QStringList widgetIds();
    static QString     widgetLabel(const QString& widgetId);
    static QString     widgetIconPath(const QString& widgetId);
    static QStringList startupCandidateIds();

    static DockLayoutSettings defaultsForLayout(const QString& layoutId);
    static void ensureValid(DockLayoutSettings& s);

    void attach(KDDockWidgets::QtWidgets::MainWindow* mainWindow, const QString& projectName);
    void build(const DockLayoutSettings& settings);
    void clear();

    QStringList zoneIds() const { return zoneIdsForLayout(m_layout); }
    bool hasZone(const QString& zoneId) const { return m_zones.contains(zoneId); }

    KDDockWidgets::QtWidgets::DockWidget* zoneHost(const QString& zoneId) const;
    KDDockWidgets::QtWidgets::DockWidget* primaryHost() const;
    KDDockWidgets::QtWidgets::DockWidget* secondaryHost() const;

    QString resolveZone(const QString& widgetId, const QString& zoneOverride, const DockLayoutSettings& settings) const;
    void placeInZone(const QString& zoneId, KDDockWidgets::QtWidgets::DockWidget* dock, AdaptixWidget* owner) const;
    void placeWidget(const QString& widgetId, KDDockWidgets::QtWidgets::DockWidget* dock, AdaptixWidget* owner, const QString& zoneOverride = QString()) const;
    void openStartup(AdaptixWidget* owner, const DockLayoutSettings& settings) const;

    QList<KDDockWidgets::QtWidgets::DockWidget*> allHosts() const;
};

#endif
