#include <Client/LastTickWorker.h>
#include <Agent/Agent.h>

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
    for ( auto agent : mainWidget->Agents ) {
        if ( agent->data.Async && agent->active ) {
            int current = QDateTime::currentSecsSinceEpoch();
            int diff    = current - agent->data.LastTick;

            agent->item_Last->setText( FormatSecToStr(diff) );
        }
    }
}