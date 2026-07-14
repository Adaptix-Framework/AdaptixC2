#ifndef ADAPTIXCLIENT_CREDENTIALWIDGETIFACE_H
#define ADAPTIXCLIENT_CREDENTIALWIDGETIFACE_H

#include <main.h>

namespace KDDockWidgets::QtWidgets { class DockWidget; }

class CredentialWidgetIface
{
public:
    virtual ~CredentialWidgetIface() = default;

    virtual KDDockWidgets::QtWidgets::DockWidget* dock() = 0;

    virtual void SetUpdatesEnabled(bool enabled) = 0;
    virtual void Clear() = 0;

    virtual void AddCredentialsItems(QList<CredentialData> credsList) = 0;
    virtual void EditCredentialsItem(const CredentialData& newCredentials) = 0;
    virtual void RemoveCredentialsItem(const QList<qint64>& credsId) = 0;
    virtual void CredsSetTag(const QList<qint64>& credsIds, const QString& tag) = 0;

    virtual void CredentialsAdd(QList<CredentialData> credsList) = 0;

    virtual void UpdateColumnsSize() const {}
    virtual void UpdateColumnsVisible() {}
    virtual void UpdateFilterComboBoxes() const {}

    virtual QWidget* asWidget() = 0;
};

#endif
