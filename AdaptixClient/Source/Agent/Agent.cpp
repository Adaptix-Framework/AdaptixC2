#include <Agent/Agent.h>
#include <Client/Requestor.h>

Agent::Agent(QJsonObject jsonObjAgentData, Commander* commander, AdaptixWidget* w)
{
    this->adaptixWidget = w;

    this->data.Id           = jsonObjAgentData["a_id"].toString();
    this->data.Name         = jsonObjAgentData["a_name"].toString();
    this->data.Listener     = jsonObjAgentData["a_listener"].toString();

    this->data.Async        = jsonObjAgentData["a_async"].toBool();
    this->data.ExternalIP   = jsonObjAgentData["a_external_ip"].toString();
    this->data.InternalIP   = jsonObjAgentData["a_internal_ip"].toString();
    this->data.GmtOffset    = jsonObjAgentData["a_gmt_offset"].toDouble();
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

    QString sleep;
    QString last;
    if ( mark.isEmpty()) {
        sleep = QString("%1 (%2%)").arg( FormatSecToStr(this->data.Sleep) ).arg(this->data.Jitter);
        if ( !this->data.Async )
            last = QString::fromUtf8("\u221E  \u221E");
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

    this->SetImage();
    this->graphImage = this->imageActive;

    if ( mark.isEmpty() ) {
        if ( !this->data.Color.isEmpty() )
            this->SetColor(this->data.Color);
    } else {
        this->MarkItem(mark);
    }

    this->Console        = new ConsoleWidget(this, commander);
    this->FileBrowser    = new BrowserFilesWidget(this);
    this->ProcessBrowser = new BrowserProcessWidget(this);
}

Agent::~Agent() = default;

void Agent::Update(QJsonObject jsonObjAgentData)
{
    int old_Sleep     = this->data.Sleep;
    int old_Jitter    = this->data.Jitter;
    QString old_Color = this->data.Color;

    this->data.Sleep        = jsonObjAgentData["a_sleep"].toDouble();
    this->data.Jitter       = jsonObjAgentData["a_jitter"].toDouble();
    this->data.Impersonated = jsonObjAgentData["a_impersonated"].toString();
    this->data.Tags         = jsonObjAgentData["a_tags"].toString();
    this->data.Color        = jsonObjAgentData["a_color"].toString();
    QString mark            = jsonObjAgentData["a_mark"].toString();

    this->item_Tags->setText(this->data.Tags);

    QString username = this->data.Username;
    if ( this->data.Elevated )
        username = "* " + username;
    if ( !this->data.Impersonated.isEmpty() )
        username += " [" + this->data.Impersonated + "]";
    this->item_Username->setText(username);

    if (this->data.Mark == mark) {
        if (old_Sleep != this->data.Sleep || old_Jitter != this->data.Jitter) {
            QString status = QString("%1 (%2%)").arg( FormatSecToStr(this->data.Sleep) ).arg(this->data.Jitter);
            item_Last->setText(status);
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

        // else if ( Reason == "Work time" ) {
        //     session.WorkingTime  = (uint32_t) strtoul(Package->Body.Info["String"].c_str(), NULL, 0);
        // }
    }
}

void Agent::MarkItem(const QString &mark)
{
    if (this->data.Mark == mark)
        return;

    this->data.Mark = mark;
    QString color;
    QString status;
    if ( mark == "" ) {
        this->active = true;
        status = QString("%1 (%2%)").arg( FormatSecToStr(this->data.Sleep) ).arg(this->data.Jitter);
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
        this->graphImage = this->imageInactive;
        status = mark;
        color = QString(COLOR_DarkBrownishRed) + "-" + QString(COLOR_LightGray);
    }

    if (this->graphItem)
        this->graphItem->update();

    this->SetColor(color);
    this->item_Sleep->setText(status);
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

void Agent::SetImage()
{
    if (data.Os == OS_WINDOWS) {
        if (data.Elevated) {
            this->item_Os->setIcon(QIcon(":/icons/os_win_red"));
            this->imageActive = QImage(":/graph/win_red");
        }
        else {
            this->item_Os->setIcon(QIcon(":/icons/os_win_blue"));
            this->imageActive = QImage(":/graph/win_blue");
        }
        this->imageInactive = QImage(":/graph/win_grey");
    }
    else if (data.Os == OS_LINUX) {
        if (data.Elevated) {
            this->item_Os->setIcon(QIcon(":/icons/os_linux_red"));
            this->imageActive = QImage(":/graph/linux_red");
        } else {
            this->item_Os->setIcon(QIcon(":/icons/os_linux_blue"));
            this->imageActive = QImage(":/graph/linux_blue");
        }
        this->imageInactive = QImage(":/graph/linux_grey");
    }
    else if (data.Os == OS_MAC) {
        if (data.Elevated) {
            this->item_Os->setIcon(QIcon(":/icons/os_mac_red"));
            this->imageActive = QImage(":/graph/mac_red");
        } else {
            this->item_Os->setIcon(QIcon(":/icons/os_mac_blue"));
            this->imageActive = QImage(":/graph/mac_blue");
        }
        this->imageInactive = QImage(":/graph/mac_grey");
    }
    else {
        if (data.Elevated) {
            // this->item_Os->setIcon(QIcon(":/icons/unknown_red"));
            this->imageActive = QImage(":/graph/unknown_red");
        }
        else {
            // this->item_Os->setIcon(QIcon(":/icons/unknown_blue"));
            this->imageActive = QImage(":/graph/unknown_blue");
        }
        this->imageInactive = QImage(":/graph/unknown_grey");
    }
}

/// TASK

QString Agent::TasksStop(const QStringList &tasks) const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqTaskStop( data.Id, tasks, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "JWT error";

    return message;
}

QString Agent::TasksDelete(const QStringList &tasks) const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqTasksDelete(data.Id, tasks, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "JWT error";

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
        return "JWT error";

    return message;
}

QString Agent::BrowserProcess() const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserProcess( data.Id, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "JWT error";

    return message;
}

QString Agent::BrowserList(const QString &path) const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserList( data.Id, path, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "JWT error";

    return message;
}

QString Agent::BrowserUpload(const QString &path, const QString &content) const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserUpload( data.Id, path, content, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "JWT error";

    return message;
}

QString Agent::BrowserDownload(const QString &path) const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqBrowserDownloadStart( data.Id, path, *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result)
        return "JWT error";

    return message;
}
