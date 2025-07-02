#include <Agent/Agent.h>
#include <Agent/AgentTableWidgetItem.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/TerminalWidget.h>
#include <UI/Widgets/BrowserFilesWidget.h>
#include <UI/Widgets/BrowserProcessWidget.h>
#include <UI/Graph/GraphItem.h>
#include <UI/Graph/SessionsGraph.h>
#include <Client/Requestor.h>
#include <Client/Settings.h>
#include <Client/AuthProfile.h>
#include <MainAdaptix.h>

Agent::Agent(QJsonObject jsonObjAgentData, AdaptixWidget* w)
{
    this->adaptixWidget = w;

    this->data.Id           = jsonObjAgentData["a_id"].toString();
    this->data.Name         = jsonObjAgentData["a_name"].toString();
    this->data.Listener     = jsonObjAgentData["a_listener"].toString();
    this->data.Async        = jsonObjAgentData["a_async"].toBool();
    this->data.ExternalIP   = jsonObjAgentData["a_external_ip"].toString();
    this->data.InternalIP   = jsonObjAgentData["a_internal_ip"].toString();
    this->data.GmtOffset    = jsonObjAgentData["a_gmt_offset"].toDouble();
    this->data.WorkingTime  = jsonObjAgentData["a_workingtime"].toDouble();
    this->data.KillDate     = jsonObjAgentData["a_killdate"].toDouble();
    this->data.Sleep        = jsonObjAgentData["a_sleep"].toDouble();
    this->data.Jitter       = jsonObjAgentData["a_jitter"].toDouble();
    this->data.Pid          = jsonObjAgentData["a_pid"].toString();
    this->data.Tid          = jsonObjAgentData["a_tid"].toString();
    this->data.Arch         = jsonObjAgentData["a_arch"].toString();
    this->data.Elevated     = jsonObjAgentData["a_elevated"].toBool();
    this->data.Process      = jsonObjAgentData["a_process"].toString();
    this->data.Os           = jsonObjAgentData["a_os"].toDouble();
    this->data.OsDesc       = jsonObjAgentData["a_os_desc"].toString();
    this->data.Domain       = jsonObjAgentData["a_domain"].toString();
    this->data.Computer     = jsonObjAgentData["a_computer"].toString();
    this->data.Username     = jsonObjAgentData["a_username"].toString();
    this->data.Impersonated = jsonObjAgentData["a_impersonated"].toString();
    this->data.LastTick     = jsonObjAgentData["a_last_tick"].toDouble();
    this->data.Tags         = jsonObjAgentData["a_tags"].toString();
    this->data.Color        = jsonObjAgentData["a_color"].toString();
    QString mark            = jsonObjAgentData["a_mark"].toString();

    QString process  = QString("%1 (%2)").arg(this->data.Process).arg(this->data.Arch);

    QString username = this->data.Username;
    if ( this->data.Elevated )
        username = "* " + username;
    if ( !this->data.Impersonated.isEmpty() )
        username += " [" + this->data.Impersonated + "]";

    for ( auto listenerData : this->adaptixWidget->Listeners) {
        if ( listenerData.ListenerName == this->data.Listener ) {
            QString listenerType = listenerData.ListenerType.split("/")[0];
            if (listenerType == "internal")
                this->connType = "internal";
            else
                this->connType = "external";
            break;
        }
    }

    QString sleep;
    QString last;
    if (mark == "") {
        if ( !this->data.Async ) {
            if ( this->connType == "internal" )
                sleep = QString::fromUtf8("\u221E  \u221E");
            else
                sleep = QString::fromUtf8("\u27F6\u27F6\u27F6");
        }
        else {
            sleep = QString("%1 (%2%)").arg( FormatSecToStr(this->data.Sleep) ).arg(this->data.Jitter);
        }
    }

    this->item_Id       = new AgentTableWidgetItem( this->data.Id, this );
    this->item_Type     = new AgentTableWidgetItem( this->data.Name, this );
    this->item_Listener = new AgentTableWidgetItem( this->data.Listener, this );
    this->item_External = new AgentTableWidgetItem( this->data.ExternalIP, this );
    this->item_Internal = new AgentTableWidgetItem( this->data.InternalIP, this );
    this->item_Domain   = new AgentTableWidgetItem( this->data.Domain, this );
    this->item_Computer = new AgentTableWidgetItem( this->data.Computer, this );
    this->item_Username = new AgentTableWidgetItem( username, this );
    this->item_Os       = new AgentTableWidgetItem( this->data.OsDesc, this );
    this->item_Process  = new AgentTableWidgetItem( process, this );
    this->item_Pid      = new AgentTableWidgetItem( this->data.Pid, this );
    this->item_Tid      = new AgentTableWidgetItem( this->data.Tid, this );
    this->item_Tags     = new AgentTableWidgetItem( this->data.Tags, this );
    this->item_Last     = new AgentTableWidgetItem( last, this );
    this->item_Sleep    = new AgentTableWidgetItem( sleep, this );
    this->item_Pid      = new AgentTableWidgetItem( this->data.Pid, this );

    if (this->data.WorkingTime || this->data.KillDate) {
        QString toolTip = "";
        if (this->data.WorkingTime) {
            uint startH = ( this->data.WorkingTime >> 24 ) % 64;
            uint startM = ( this->data.WorkingTime >> 16 ) % 64;
            uint endH   = ( this->data.WorkingTime >>  8 ) % 64;
            uint endM   = ( this->data.WorkingTime >>  0 ) % 64;

            QChar c = QLatin1Char('0');
            toolTip = QString("Work time: %1:%2 - %3:%4\n").arg(startH, 2, 10, c).arg(startM, 2, 10, c).arg(endH, 2, 10, c).arg(endM, 2, 10, c);
        }
        if (this->data.KillDate) {
            QDateTime dateTime = QDateTime::fromSecsSinceEpoch(this->data.KillDate);
            toolTip += QString("Kill date: %1").arg(dateTime.toString("dd.MM.yyyy hh:mm:ss"));
        }
        this->item_Sleep->setToolTip(toolTip);
    }

    this->UpdateImage();
    this->graphImage = this->imageActive;

    if (mark == "") {
        if ( !this->data.Color.isEmpty() )
            this->SetColor(this->data.Color);
    } else {
        this->MarkItem(mark);
    }

    auto regAgnet = this->adaptixWidget->GetRegAgent(data.Name, data.Listener, data.Os);
    this->browsers = regAgnet.browsers;

    this->Console = new ConsoleWidget(this, regAgnet.commander);

    if (this->browsers.FileBrowser)
        this->FileBrowser = new BrowserFilesWidget(this);

    if (this->browsers.ProcessBrowser)
        this->ProcessBrowser = new BrowserProcessWidget(this);

    if (this->browsers.RemoteTerminal)
        this->Terminal = new TerminalWidget(this, adaptixWidget);
}

Agent::~Agent() = default;

void Agent::Update(QJsonObject jsonObjAgentData)
{
    int old_Sleep     = this->data.Sleep;
    int old_Jitter    = this->data.Jitter;
    QString old_Color = this->data.Color;

    this->data.Sleep        = jsonObjAgentData["a_sleep"].toDouble();
    this->data.Jitter       = jsonObjAgentData["a_jitter"].toDouble();
    this->data.WorkingTime  = jsonObjAgentData["a_workingtime"].toDouble();
    this->data.KillDate     = jsonObjAgentData["a_killdate"].toDouble();
    this->data.Tags         = jsonObjAgentData["a_tags"].toString();
    this->data.Color        = jsonObjAgentData["a_color"].toString();
    this->data.Impersonated = jsonObjAgentData["a_impersonated"].toString();
    QString mark            = jsonObjAgentData["a_mark"].toString();

    this->item_Tags->setText(this->data.Tags);

    QString username = this->data.Username;
    if ( this->data.Elevated )
        username = "* " + username;
    if ( !this->data.Impersonated.isEmpty() )
        username += " [" + this->data.Impersonated + "]";

    if (this->item_Username->text() != username) {
        this->item_Username->setText(username);

    }

    if (this->data.WorkingTime || this->data.KillDate) {
        QString toolTip = "";
        if (this->data.WorkingTime) {
            uint startH = ( this->data.WorkingTime >> 24 ) % 64;
            uint startM = ( this->data.WorkingTime >> 16 ) % 64;
            uint endH   = ( this->data.WorkingTime >>  8 ) % 64;
            uint endM   = ( this->data.WorkingTime >>  0 ) % 64;

            QChar c = QLatin1Char('0');
            toolTip = QString("Work time: %1:%2 - %3:%4\n").arg(startH, 2, 10, c).arg(startM, 2, 10, c).arg(endH, 2, 10, c).arg(endM, 2, 10, c);
        }
        if (this->data.KillDate) {
            QDateTime dateTime = QDateTime::fromSecsSinceEpoch(this->data.KillDate);
            toolTip += QString("Kill date: %1").arg(dateTime.toString("dd.MM.yyyy hh:mm:ss"));
        }
        this->item_Sleep->setToolTip(toolTip);
    }

    if (this->data.Mark == mark) {
        if (old_Sleep != this->data.Sleep || old_Jitter != this->data.Jitter) {
            QString sleep = QString("%1 (%2%)").arg( FormatSecToStr(this->data.Sleep) ).arg(this->data.Jitter);
            item_Sleep->setText(sleep);
        }
        if (this->data.Color != old_Color) {
            if (this->data.Mark == "") {
                this->SetColor(this->data.Color);
            }
        }
    }
    else {
        this->MarkItem(mark);
        if (mark == "Terminated") {
            adaptixWidget->SessionsGraphPage->RemoveAgent(this, true);
        }
    }
}

void Agent::MarkItem(const QString &mark)
{
    if (this->data.Mark == mark)
        return;

    this->data.Mark = mark;
    QString color;
    QString sleepMark;
    QString lastMarrk;
    if ( mark == "" ) {
        if ( !this->data.Async ) {
            if ( this->connType == "internal" )
                sleepMark = QString::fromUtf8("\u221E  \u221E");
            else
                sleepMark = QString::fromUtf8("\u27F6\u27F6\u27F6");
            item_Last->setText("");
        }
        else {
            sleepMark = QString("%1 (%2%)").arg( FormatSecToStr(this->data.Sleep) ).arg(this->data.Jitter);
        }
        this->active = true;
        color = this->data.Color;
        this->graphImage = this->imageActive;
    }
    else {
        if ( mark == "Terminated" ) {
            this->active = false;
            item_Last->setText(UnixTimestampGlobalToStringLocalSmall(data.LastTick));
        }
        else if ( mark == "Inactive" ) {
            this->active = false;
            item_Last->setText(UnixTimestampGlobalToStringLocalSmall(data.LastTick));
        }
        else if ( mark == "Unlink" ) {
            this->active = false;
            item_Last->setText(UnixTimestampGlobalToStringLocalSmall(data.LastTick));
        }
        else if ( mark == "Disconnect" ) {
            this->active = false;
            item_Last->setText(UnixTimestampGlobalToStringLocalSmall(data.LastTick));
        }
        this->graphImage = this->imageInactive;
        sleepMark = mark;
        color = QString(COLOR_DarkBrownishRed) + "-" + QString(COLOR_LightGray);
    }

    if (this->graphItem)
        this->graphItem->update();

    this->SetColor(color);
    this->item_Sleep->setText(sleepMark);
}

void Agent::SetColor(const QString &color) const
{
    if (color.isEmpty()) {
        this->item_Id->RevertColor();
        this->item_Type->RevertColor();
        this->item_Listener->RevertColor();
        this->item_External->RevertColor();
        this->item_Internal->RevertColor();
        this->item_Domain->RevertColor();
        this->item_Computer->RevertColor();
        this->item_Username->RevertColor();
        this->item_Os->RevertColor();
        this->item_Process->RevertColor();
        this->item_Pid->RevertColor();
        this->item_Tid->RevertColor();
        this->item_Tags->RevertColor();
        this->item_Last->RevertColor();
        this->item_Sleep->RevertColor();
    } else {
        QStringList colors = color.split('-');
        if (colors.size() == 2) {
            this->item_Id->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Type->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Listener->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_External->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Internal->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Domain->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Computer->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Username->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Os->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Process->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Pid->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Tid->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Tags->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Last->SetColor(QColor(colors[0]), QColor(colors[1]));
            this->item_Sleep->SetColor(QColor(colors[0]), QColor(colors[1]));
        }
    }
}

void Agent::UpdateImage()
{
    QString v = "v1";
    if (GlobalClient->settings->data.GraphVersion == "Version 2")
        v = "v2";

    if (data.Os == OS_WINDOWS) {
        if (data.Elevated) {
            this->item_Os->setIcon(QIcon(":/icons/os_win_red"));
            this->imageActive = QImage(":/graph/"+v+"/win_red");
        }
        else {
            this->item_Os->setIcon(QIcon(":/icons/os_win_blue"));
            this->imageActive = QImage(":/graph/"+v+"/win_blue");
        }
        this->imageInactive = QImage(":/graph/"+v+"/win_grey");
    }
    else if (data.Os == OS_LINUX) {
        if (data.Elevated) {
            this->item_Os->setIcon(QIcon(":/icons/os_linux_red"));
            this->imageActive = QImage(":/graph/"+v+"/linux_red");
        } else {
            this->item_Os->setIcon(QIcon(":/icons/os_linux_blue"));
            this->imageActive = QImage(":/graph/"+v+"/linux_blue");
        }
        this->imageInactive = QImage(":/graph/"+v+"/linux_grey");
    }
    else if (data.Os == OS_MAC) {
        if (data.Elevated) {
            this->item_Os->setIcon(QIcon(":/icons/os_mac_red"));
            this->imageActive = QImage(":/graph/"+v+"/mac_red");
        } else {
            this->item_Os->setIcon(QIcon(":/icons/os_mac_blue"));
            this->imageActive = QImage(":/graph/"+v+"/mac_blue");
        }
        this->imageInactive = QImage(":/graph/"+v+"/mac_grey");
    }
    else {
        if (data.Elevated) {
            this->imageActive = QImage(":/graph/"+v+"/unknown_red");
        }
        else {
            this->imageActive = QImage(":/graph/"+v+"/unknown_blue");
        }
        this->imageInactive = QImage(":/graph/"+v+"/unknown_grey");
    }

    if (this->data.Mark == "")
        this->graphImage = this->imageActive;
    else
        this->graphImage = this->imageInactive;
}

/// TASK

QString Agent::TasksStop(const QStringList &tasks) const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqTaskStop( data.Id, tasks, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "Response timeout";

    return message;
}

QString Agent::TasksDelete(const QStringList &tasks) const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqTasksDelete(data.Id, tasks, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "Response timeout";

    return message;
}

/// PIVOT

void Agent::SetParent(const PivotData &pivotData)
{
    this->parentId = pivotData.ParentAgentId;
    this->data.ExternalIP = QString::fromUtf8("%1 \u221E %2").arg(pivotData.ParentAgentId).arg(pivotData.PivotName);
    this->item_External->setText(this->data.ExternalIP);
    this->item_Last->setText( QString::fromUtf8("\u221E\u221E\u221E") );
}

void Agent::UnsetParent(const PivotData &pivotData)
{
    this->parentId = "";
    this->data.ExternalIP = "";
    this->item_External->setText(this->data.ExternalIP);
    this->item_Last->setText( QString::fromUtf8("\u221E  \u221E") );
}

void Agent::AddChild(const PivotData &pivotData)
{
    this->childsId.push_back(pivotData.ChildAgentId);
}

void Agent::RemoveChild(const PivotData &pivotData)
{
    for (int i = 0; i < this->childsId.size(); i++) {
        if (this->childsId[i] == pivotData.ChildAgentId) {
            this->childsId.removeAt(i);
            break;
        }
    }
}

/// BROWSER

QString Agent::BrowserDisks() const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserDisks( data.Id, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "Response timeout";

    return message;
}

QString Agent::BrowserProcess() const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserProcess( data.Id, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "Response timeout";

    return message;
}

QString Agent::BrowserList(const QString &path) const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserList( data.Id, path, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "Response timeout";

    return message;
}

QString Agent::BrowserUpload(const QString &path, const QString &content) const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserUpload( data.Id, path, content, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "Response timeout";

    return message;
}

QString Agent::BrowserDownload(const QString &path) const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqDownloadStart( data.Id, path, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "Response timeout";

    return message;
}
