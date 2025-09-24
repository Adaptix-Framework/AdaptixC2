#ifndef ADAPTIXCLIENT_ABSTRACTDOCK_H
#define ADAPTIXCLIENT_ABSTRACTDOCK_H

#include <kddockwidgets/qtwidgets/views/DockWidget.h>
#include <kddockwidgets/qtwidgets/views/MainWindow.h>

class DockTab : public QWidget {
Q_OBJECT
protected:
    KDDockWidgets::QtWidgets::DockWidget* dockWidget;

public:
    DockTab(const QString &tabName, const QString &projectName, const QString &icon = "") {
        dockWidget = new KDDockWidgets::QtWidgets::DockWidget(tabName + ":Dock-" + projectName, KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
        dockWidget->setTitle(tabName);
        if (!icon.isEmpty())
            dockWidget->setIcon(QIcon(icon), KDDockWidgets::IconPlace::TabBar);
    };

    ~DockTab() override { dockWidget->deleteLater(); };

    KDDockWidgets::QtWidgets::DockWidget* dock() { return this->dockWidget; };
};

#endif //ADAPTIXCLIENT_ABSTRACTDOCK_H