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
    quit();
}

void LastTickWorker::updateLastItems()
{
    QStringList updatedAgents;

    QReadLocker locker(&mainWidget->AgentsMapLock);
    for ( auto agent : mainWidget->AgentsMap ) {
        if ( agent->data.Async && agent->active ) {
            int current = QDateTime::currentSecsSinceEpoch();
            int diff    = current - agent->data.LastTick;

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
                    if (agent->data.Mark != "No worktime")
                        agent->MarkItem("No worktime");
                }
            }

            if ( GlobalClient->settings->data.CheckHealth && !isOffHours ) {
                if (diff > agent->data.Sleep * GlobalClient->settings->data.HealthCoaf + GlobalClient->settings->data.HealthOffset) {

                    if (diff > 24 * 3600)
                        agent->LastMark = UnixTimestampGlobalToStringLocalSmall(agent->data.LastTick);
                    else
                        agent->LastMark = FormatSecToStr(diff) + " / " + FormatSecToStr(agent->data.Sleep);

                    if (agent->data.Mark != "No response")
                        agent->MarkItem("No response");

                    updatedAgents.append(agent->data.Id);
                    continue;
                }
                else {
                    agent->MarkItem("");
                }
            }
            agent->LastMark = FormatSecToStr(diff);
            updatedAgents.append(agent->data.Id);
        }
    }

    if (!updatedAgents.isEmpty())
        Q_EMIT agentsUpdated(updatedAgents);
}