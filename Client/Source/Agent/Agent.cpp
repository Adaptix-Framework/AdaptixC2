#include <Agent/Agent.h>
#include <Client/Requestor.h>

Agent::Agent(QJsonObject jsonObjAgentData, Commander* commander, AdaptixWidget* w)
{
    adaptixWidget = w;

    data.Id         = jsonObjAgentData["a_id"].toString();
    data.Name       = jsonObjAgentData["a_name"].toString();
    data.Listener   = jsonObjAgentData["a_listener"].toString();
    data.Async      = jsonObjAgentData["a_async"].toBool();
    data.ExternalIP = jsonObjAgentData["a_external_ip"].toString();
    data.InternalIP = jsonObjAgentData["a_internal_ip"].toString();
    data.GmtOffset  = jsonObjAgentData["a_gmt_offset"].toDouble();
    data.Sleep      = jsonObjAgentData["a_sleep"].toDouble();
    data.Jitter     = jsonObjAgentData["a_jitter"].toDouble();
    data.Pid        = jsonObjAgentData["a_pid"].toString();
    data.Tid        = jsonObjAgentData["a_tid"].toString();
    data.Arch       = jsonObjAgentData["a_arch"].toString();
    data.Elevated   = jsonObjAgentData["a_elevated"].toBool();
    data.Process    = jsonObjAgentData["a_process"].toString();
    data.Os         = jsonObjAgentData["a_os"].toDouble();
    data.OsDesc     = jsonObjAgentData["a_os_desc"].toString();
    data.Domain     = jsonObjAgentData["a_domain"].toString();
    data.Computer   = jsonObjAgentData["a_computer"].toString();
    data.Username   = jsonObjAgentData["a_username"].toString();
    data.LastTick   = jsonObjAgentData["a_last_tick"].toDouble();
    data.Tags       = jsonObjAgentData["a_tags"].toString();

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
    item_Username = new TableWidgetItemAgent( username, this );
    item_Os       = new TableWidgetItemAgent( data.OsDesc, this );
    item_Process  = new TableWidgetItemAgent( data.Process, this );
    item_Pid      = new TableWidgetItemAgent( data.Pid, this );
    item_Tid      = new TableWidgetItemAgent( data.Tid, this );
    item_Tags     = new TableWidgetItemAgent( data.Tags, this );
    item_Last     = new TableWidgetItemAgent( last, this );
    item_Sleep    = new TableWidgetItemAgent( sleep, this );
    item_Pid      = new TableWidgetItemAgent( data.Pid, this );

    Console        = new ConsoleWidget(this, commander );
    FileBrowser    = new BrowserFilesWidget(this);
    ProcessBrowser = new BrowserProcessWidget(this);
}

Agent::~Agent() = default;

void Agent::Update(QJsonObject jsonObjAgentData)
{
    data.Sleep    = jsonObjAgentData["a_sleep"].toDouble();
    data.Jitter   = jsonObjAgentData["a_jitter"].toDouble();
    data.Elevated = jsonObjAgentData["a_elevated"].toBool();
    data.Username = jsonObjAgentData["a_username"].toString();
    data.Tags     = jsonObjAgentData["a_tags"].toString();

    auto username = data.Username;
    if ( data.Elevated )
        username = "* " + username;

    auto sleep = QString("%1 (%2%)").arg( FormatSecToStr(data.Sleep) ).arg(data.Jitter);

    item_Username->setText(username);
    item_Tags->setText(data.Tags);
    item_Sleep->setText(sleep);
}

QString Agent::BrowserDisks()
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserDisks( data.Id, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "JWT error";

    return message;
}

QString Agent::BrowserProcess()
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserProcess( data.Id, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "JWT error";

    return message;
}

QString Agent::BrowserList(QString path)
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserList( data.Id, path, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "JWT error";

    return message;
}

QString Agent::BrowserUpload(QString path, QString content)
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserUpload( data.Id, path, content, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "JWT error";

    return message;
}

QString Agent::BrowserDownload(QString path)
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserDownloadStart( data.Id, path, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "JWT error";

    return message;}
