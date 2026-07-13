#include <Agent/Agent.h>
#include <Workers/LastTickWorker.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Settings.h>
#include <Client/AuthProfile.h>
#include <MainAdaptix.h>

LastTickWorker::LastTickWorker(AdaptixWidget *w)
{
    mainWidget = w;
}

LastTickWorker::~LastTickWorker()
{
    if (isRunning()) {
        QMetaObject::invokeMethod(this, "stopWorker", Qt::QueuedConnection);
        wait(5000);
        if (isRunning()) {
            terminate();
            wait();
        }
    }
}

void LastTickWorker::run()
{
    timer = new QTimer();
    QObject::connect( timer, &QTimer::timeout, this, &LastTickWorker::updateLastItems );
    timer->start( 500 );

    exec();
}

void LastTickWorker::stopWorker()
{
    if (timer) {
        timer->stop();
        disconnect(timer, nullptr, nullptr, nullptr);
        delete timer;
        timer = nullptr;
    }
}

void LastTickWorker::updateLastItems()
{
    QList<AgentMarkInfo> marks;
    QReadLocker locker(&mainWidget->AgentsMapLock);
    for ( auto agent : mainWidget->AgentsMap ) {
        if ( !agent->data.Async || !agent->active )
            continue;

        qint64 current = QDateTime::currentSecsSinceEpoch();
        qint64 diff    = current - agent->data.LastTick;

        QString mark;
        QString lastMark;

        bool isOffHours = false;
        if ( agent->data.WorkingTime && diff > 10 ) {
            uint startH = ( agent->data.WorkingTime >> 24 ) % 64;
            uint startM = ( agent->data.WorkingTime >> 16 ) % 64;
            uint endH   = ( agent->data.WorkingTime >>  8 ) % 64;
            uint endM   = ( agent->data.WorkingTime >>  0 ) % 64;

            QDateTime Now = QDateTime::currentDateTimeUtc();
            int nowH = Now.time().hour() + agent->data.GmtOffset;
            int nowM = Now.time().minute();

            if ( startH < nowH && nowH < endH  ){}
            else if ( startH == nowH && startH != endH && startM <= nowM ){}
            else if ( endH == nowH && startM <= nowM && nowM < endM ){}
            else {
                isOffHours = true;
                mark = "No worktime";
            }
        }

        if ( GlobalClient->settings->data.CheckHealth && !isOffHours ) {
            if (diff > agent->data.Sleep * GlobalClient->settings->data.HealthCoaf + GlobalClient->settings->data.HealthOffset) {

                if (diff > 24 * 3600)
                    lastMark = UnixTimestampGlobalToStringLocalSmall(agent->data.LastTick);
                else
                    lastMark = FormatSecToStr(diff) + " / " + FormatSecToStr(agent->data.Sleep);

                mark = "No response";
            }
        }

        if (mark.isEmpty())
            lastMark = FormatSecToStr(diff);

        marks.append({agent->data.Id, mark, lastMark});
    }

    if (!marks.isEmpty())
        Q_EMIT agentTickUpdate(marks);
}