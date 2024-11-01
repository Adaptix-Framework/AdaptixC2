#include <Agent/Agent.h>

Agent::Agent(QJsonObject jsonObj)
{
    data.Id         = jsonObj["a_id"].toString();
    data.Name       = jsonObj["a_name"].toString();
    data.Listener   = jsonObj["a_listener"].toString();
    data.Async      = jsonObj["a_async"].toBool();
    data.ExternalIP = jsonObj["a_external_ip"].toString();
    data.InternalIP = jsonObj["a_internal_ip"].toString();
    data.GmtOffset  = jsonObj["a_gmt_offset"].toDouble();
    data.Sleep      = jsonObj["a_sleep"].toDouble();
    data.Jitter     = jsonObj["a_jitter"].toDouble();
    data.Pid        = jsonObj["a_pid"].toString();
    data.Tid        = jsonObj["a_tid"].toString();
    data.Arch       = jsonObj["a_arch"].toString();
    data.Elevated   = jsonObj["a_elevated"].toBool();
    data.Process    = jsonObj["a_process"].toString();
    data.Os         = jsonObj["a_os"].toDouble();
    data.OsDesc     = jsonObj["a_os_desc"].toString();
    data.Domain     = jsonObj["a_domain"].toString();
    data.Computer   = jsonObj["a_computer"].toString();
    data.Username   = jsonObj["a_username"].toString();
    data.LastTick   = jsonObj["a_last_tick"].toDouble();

    QJsonArray tagsArray = jsonObj["a_tags"].toArray();
    QStringList tags;
    for (const QJsonValue &value : tagsArray) {
        if (value.isString()) {
            data.Tags.append(value.toString());
        }
    }

    auto username = data.Username;
    if ( data.Elevated )
        username = "* " + username;

    auto sleep = QString("%1 (%2%)").arg( FormatSecToStr(data.Sleep) ).arg(data.Jitter);

    QString last = "";
    if ( !data.Async )
        last = QString::fromUtf8("\u221E");

    item_Id       = new TableWidgetItemAgent( data.Id, this );
    item_Type     = new TableWidgetItemAgent( data.Name, this );
    item_Listener = new TableWidgetItemAgent( data.Listener, this );
    item_External = new TableWidgetItemAgent( data.ExternalIP, this );
    item_Internal = new TableWidgetItemAgent( data.InternalIP, this );
    item_Domain   = new TableWidgetItemAgent( data.Domain, this );
    item_Computer = new TableWidgetItemAgent( data.Computer, this );
    item_Username = new TableWidgetItemAgent( data.Username, this );
    item_Os       = new TableWidgetItemAgent( data.OsDesc, this );
    item_Process  = new TableWidgetItemAgent( data.Process, this );
    item_Pid      = new TableWidgetItemAgent( data.Pid, this );
    item_Tid      = new TableWidgetItemAgent( data.Tid, this );
    item_Tags     = new TableWidgetItemAgent( "", this );
    item_Last     = new TableWidgetItemAgent( last, this );
    item_Sleep    = new TableWidgetItemAgent( sleep, this );
    item_Pid      = new TableWidgetItemAgent( data.Pid, this );

    Console = new ConsoleWidget( this );
}

Agent::~Agent()
{

}
