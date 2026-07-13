#ifndef ADAPTIXCLIENT_PAGEDTABLEHELPER_H
#define ADAPTIXCLIENT_PAGEDTABLEHELPER_H

#include <main.h>
#include <QUrlQuery>

class AuthProfile;

class PagedTableHelper : public QObject
{
Q_OBJECT
    AuthProfile* m_profile      = nullptr;
    QString      m_endpoint;
    int          m_pageSize     = 100;
    int          m_offset       = 0;
    int          m_total        = 0;
    bool         m_filterActive = false;
    int          m_pendingReqId = 0;

    QTimer*               m_debounceTimer = nullptr;
    QMap<QString,QString> m_params;

public:
    explicit PagedTableHelper(AuthProfile* profile, const QString& endpoint, QObject* parent = nullptr);

    int  pageSize()      const { return m_pageSize; }
    int  currentOffset() const { return m_offset;   }
    int  total()         const { return m_total;     }
    bool filterActive()  const { return m_filterActive; }

    bool shouldApplyLive() const { return !m_filterActive && m_offset == 0; }

    void setPageSize(int size);
    void setParam(const QString& key, const QString& value);
    void setFilterText(const QString& text);

    void loadPage(int offset);
    void reload();
    void cancel();

Q_SIGNALS:
    void pageReady(QJsonObject response);
    void errorOccurred(const QString& message);
    void loadingChanged(bool loading);

private Q_SLOTS:
    void onDebounceTimeout();
};

#endif
