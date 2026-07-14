#ifndef ADAPTIXCLIENT_TARGETWIDGETIFACE_H
#define ADAPTIXCLIENT_TARGETWIDGETIFACE_H

#include <main.h>

namespace KDDockWidgets::QtWidgets { class DockWidget; }

class TargetWidgetIface
{
public:
    virtual ~TargetWidgetIface() = default;

    virtual KDDockWidgets::QtWidgets::DockWidget* dock() = 0;

    virtual void SetUpdatesEnabled(bool enabled) = 0;
    virtual void Clear() = 0;

    virtual void AddTargetsItems(QList<TargetData> targetList) = 0;
    virtual void EditTargetsItem(const TargetData& newTarget) = 0;
    virtual void RemoveTargetsItem(const QList<qint64>& targetsId) = 0;
    virtual void TargetsSetTag(const QList<qint64>& targetIds, const QString& tag) = 0;

    virtual void TargetsAdd(QList<TargetData> targetList) = 0;

    virtual void UpdateColumnsSize() const {}
    virtual void UpdateColumnsVisible() {}

    virtual QWidget* asWidget() = 0;
};

#endif
