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

            if ( GlobalClient->settings->data.CheckHealth ) {
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
