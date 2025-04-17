#include <Client/LastTickWorker.h>
#include <Agent/Agent.h>

#include <MainAdaptix.h>

LastTickWorker::LastTickWorker(AdaptixWidget *w)
{
    mainWidget = w;
    timer = new QTimer(this);
}

LastTickWorker::~LastTickWorker() = default;

void LastTickWorker::run()
{
    QObject::connect( timer, &QTimer::timeout, this, &LastTickWorker::updateLastItems );
    timer->start( 1000 );
}

void LastTickWorker::updateLastItems() const
{
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
                    agent->MarkItem("No worktime");
                }
            }

            if ( GlobalClient->settings->data.CheckHealth && !isOffHours ) {
                if (diff > agent->data.Sleep * GlobalClient->settings->data.HealthCoaf + GlobalClient->settings->data.HealthOffset) {
                    agent->item_Last->setText( FormatSecToStr(diff) + " / " + FormatSecToStr(agent->data.Sleep));
                    agent->MarkItem("No response");
                    continue;
                }
                else {
                    agent->MarkItem("");
                }
            }
            agent->item_Last->setText( FormatSecToStr(diff) );
        }
    }
}