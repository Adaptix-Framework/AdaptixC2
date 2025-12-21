#include <Agent/Agent.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Widgets/TerminalContainerWidget.h>
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
    this->data.Mark         = jsonObjAgentData["a_mark"].toString();

    for ( auto listenerData : this->adaptixWidget->Listeners) {
        if ( listenerData.Name == this->data.Listener ) {
            this->listenerType = listenerData.ListenerRegName;
            if (listenerData.ListenerType == "internal")
                this->connType = "internal";
            else
                this->connType = "external";
            break;
        }
    }

    this->UpdateImage();
    this->graphImage = this->imageActive;

    this->MarkItem(data.Mark);

    auto regAgent = this->adaptixWidget->GetRegAgent(data.Name, data.Listener, data.Os);

    if (regAgent.commander)
        this->commander = regAgent.commander;
    else
        this->commander = new Commander();

    this->FileBrowser    = new BrowserFilesWidget(adaptixWidget, this);
    this->ProcessBrowser = new BrowserProcessWidget(adaptixWidget, this);
    this->Terminal       = new TerminalContainerWidget(this, adaptixWidget);
    this->Console        = new ConsoleWidget(adaptixWidget, this, this->commander);
    this->Console->SetUpdatesEnabled(adaptixWidget->IsSynchronized());
}

Agent::~Agent() = default;

void Agent::Update(QJsonObject jsonObjAgentData)
{
    QString old_Color = this->data.Color;

    this->data.Sleep        = jsonObjAgentData["a_sleep"].toDouble();
    this->data.Jitter       = jsonObjAgentData["a_jitter"].toDouble();
    this->data.WorkingTime  = jsonObjAgentData["a_workingtime"].toDouble();
    this->data.KillDate     = jsonObjAgentData["a_killdate"].toDouble();
    this->data.Tags         = jsonObjAgentData["a_tags"].toString();
    this->data.Color        = jsonObjAgentData["a_color"].toString();
    this->data.Impersonated = jsonObjAgentData["a_impersonated"].toString();
    QString mark            = jsonObjAgentData["a_mark"].toString();

    if (this->data.Mark == mark) {
        if (this->data.Color != old_Color) {
            if (this->data.Mark == "") {
                if (this->data.Color.isEmpty()) {
                    this->bg_color = QColor();
                    this->fg_color = QColor();
                } else {
                    QStringList colors = this->data.Color.split('-');
                    if (colors.size() == 2) {
                        this->bg_color = QColor(colors[0]);
                        this->fg_color = QColor(colors[1]);
                    }
                }
            }
        }
    }
    else {
        QString oldMark = this->data.Mark;
        this->MarkItem(mark);

        if (mark == "Terminated" || mark == "Inactive") {
            adaptixWidget->SessionsGraphDock->RemoveAgent(this, true);
        }
        else if ((oldMark == "Terminated" || oldMark == "Inactive") && !this->graphItem) {
            adaptixWidget->SessionsGraphDock->AddAgent(this, true);
        }
    }
}

void Agent::MarkItem(const QString &mark)
{
    QString color;
    if ( mark == "" ) {
        this->active = true;
        color = this->data.Color;
        this->graphImage = this->imageActive;
    }
    else {
        if ( mark == "Terminated" || mark == "Inactive" ||  mark == "Unlink" || mark == "Disconnect" ) {
            this->active = false;
        }
        this->graphImage = this->imageInactive;
        color = QString(COLOR_DarkBrownishRed) + "-" + QString(COLOR_LightGray);
    }

    if (this->graphItem)
        this->graphItem->update();

    if (color.isEmpty()) {
        this->bg_color = QColor();
        this->fg_color = QColor();
    } else {
        QStringList colors = color.split('-');
        if (colors.size() == 2) {
            this->bg_color = QColor(colors[0]);
            this->fg_color = QColor(colors[1]);
        }
    }

    this->data.Mark = mark;
}

void Agent::UpdateImage()
{
    QString v = "v1";
    if (GlobalClient->settings->data.GraphVersion == "Version 2")
        v = "v2";

    if (data.Os == OS_WINDOWS) {
        if (data.Elevated) {
            this->iconOs = QIcon(":/icons/os_win_red");
            this->imageActive = QImage(":/graph/"+v+"/win_red");
        }
        else {
            this->iconOs = QIcon(":/icons/os_win_blue");
            this->imageActive = QImage(":/graph/"+v+"/win_blue");
        }
        this->imageInactive = QImage(":/graph/"+v+"/win_grey");
    }
    else if (data.Os == OS_LINUX) {
        if (data.Elevated) {
            this->iconOs = QIcon(":/icons/os_linux_red");
            this->imageActive = QImage(":/graph/"+v+"/linux_red");
        } else {
            this->iconOs = QIcon(":/icons/os_linux_blue");
            this->imageActive = QImage(":/graph/"+v+"/linux_blue");
        }
        this->imageInactive = QImage(":/graph/"+v+"/linux_grey");
    }
    else if (data.Os == OS_MAC) {
        if (data.Elevated) {
            this->iconOs = QIcon(":/icons/os_mac_red");
            this->imageActive = QImage(":/graph/"+v+"/mac_red");
        } else {
            this->iconOs = QIcon(":/icons/os_mac_blue");
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

QString Agent::TasksCancel(const QStringList &tasks) const
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqTaskCancel( data.Id, tasks, *(adaptixWidget->GetProfile()), &message, &ok);
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
    this->LastMark = QString::fromUtf8("\u221E\u221E\u221E");
}

void Agent::UnsetParent(const PivotData &pivotData)
{
    this->parentId = "";
    this->data.ExternalIP = "";
    this->LastMark = QString::fromUtf8("\u221E  \u221E");
}

void Agent::AddChild(const PivotData &pivotData) { this->childsId.push_back(pivotData.ChildAgentId); }

void Agent::RemoveChild(const PivotData &pivotData)
{
    for (int i = 0; i < this->childsId.size(); i++) {
        if (this->childsId[i] == pivotData.ChildAgentId) {
            this->childsId.removeAt(i);
            break;
        }
    }
}
