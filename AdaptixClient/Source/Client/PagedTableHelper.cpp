#include <Client/PagedTableHelper.h>
#include <Client/AuthProfile.h>
#include <Client/HttpRequestManager.h>

#include <QPointer>

PagedTableHelper::PagedTableHelper(AuthProfile* profile, const QString& endpoint, QObject* parent) : QObject(parent), m_profile(profile), m_endpoint(endpoint)
{
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(300);
    connect(m_debounceTimer, &QTimer::timeout, this, &PagedTableHelper::onDebounceTimeout);
}

void PagedTableHelper::setPageSize(int size)
{
    m_pageSize = qBound(5, size, 10000);
}

void PagedTableHelper::setParam(const QString& key, const QString& value)
{
    if (value.isEmpty())
        m_params.remove(key);
    else
        m_params[key] = value;
}

void PagedTableHelper::setFilterText(const QString& text)
{
    QString trimmed = text.trimmed();
    m_filterActive = !trimmed.isEmpty();
    setParam("q", trimmed);
    m_debounceTimer->start();
}

void PagedTableHelper::onDebounceTimeout()
{
    loadPage(0);
}

void PagedTableHelper::loadPage(int offset)
{
    if (!m_profile)
        return;

    if (m_pendingReqId > 0) {
        HttpRequestManager::instance().cancel(m_pendingReqId);
        m_pendingReqId = 0;
    }

    m_offset = offset;

    QUrlQuery query;
    for (auto it = m_params.cbegin(); it != m_params.cend(); ++it) {
        query.addQueryItem(it.key(), it.value());
    }
    query.addQueryItem("offset", QString::number(offset));
    query.addQueryItem("limit",  QString::number(m_pageSize));

    Q_EMIT loadingChanged(true);

    QPointer<PagedTableHelper> self = this;
    m_pendingReqId = HttpRequestManager::instance().getPage( m_profile->GetURL(), m_endpoint, m_profile->GetAccessToken(), query,
        [self](bool success, const QString& message, const QJsonObject& response) {
            if (!self)
                return;

            self->m_pendingReqId = 0;
            Q_EMIT self->loadingChanged(false);
            if (!success) {
                Q_EMIT self->errorOccurred(message);
                return;
            }
            self->m_total = response["total"].toInt();
            Q_EMIT self->pageReady(response);
        }
    );
}

void PagedTableHelper::reload()
{
    loadPage(m_offset);
}

void PagedTableHelper::cancel()
{
    if (m_pendingReqId > 0) {
        HttpRequestManager::instance().cancel(m_pendingReqId);
        m_pendingReqId = 0;
    }
    m_debounceTimer->stop();
}
