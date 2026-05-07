#include <UI/Dialogs/DialogAgentData.h>
#include <Client/Requestor.h>

DialogAgentData::DialogAgentData(QWidget* parent) : QDialog(parent)
{
    this->createUI();

    connect(buttonUpdate, &QPushButton::clicked, this, &DialogAgentData::onButtonUpdate);
    connect(buttonCancel, &QPushButton::clicked, this, &DialogAgentData::onButtonCancel);
}

DialogAgentData::~DialogAgentData() = default;

void DialogAgentData::createUI()
{
    this->setWindowTitle("设置代理数据");
    this->setProperty("Main", "base");
    this->setMinimumWidth(450);

    groupNetwork = new QGroupBox("网络", this);
    layoutNetwork = new QGridLayout(groupNetwork);

    labelInternalIP = new QLabel("内部 IP：", this);
    inputInternalIP = new QLineEdit(this);
    labelExternalIP = new QLabel("外部 IP：", this);
    inputExternalIP = new QLineEdit(this);

    layoutNetwork->addWidget(labelInternalIP, 0, 0);
    layoutNetwork->addWidget(inputInternalIP, 0, 1);
    layoutNetwork->addWidget(labelExternalIP, 0, 2);
    layoutNetwork->addWidget(inputExternalIP, 0, 3);

    groupCoding = new QGroupBox("编码", this);
    layoutCoding = new QGridLayout(groupCoding);

    labelACP = new QLabel("ACP：", this);
    inputACP = new QSpinBox(this);
    inputACP->setRange(0, 65535);

    labelOemCP = new QLabel("OEM CP：", this);
    inputOemCP = new QSpinBox(this);
    inputOemCP->setRange(0, 65535);

    layoutCoding->addWidget(labelACP,    0, 0);
    layoutCoding->addWidget(inputACP,    0, 1);
    layoutCoding->addWidget(labelOemCP,  0, 2);
    layoutCoding->addWidget(inputOemCP,  0, 3);

    groupProcess = new QGroupBox("进程", this);
    layoutProcess = new QGridLayout(groupProcess);

    labelProcess = new QLabel("进程：", this);
    inputProcess = new QLineEdit(this);

    inputArch = new QComboBox(this);
    inputArch->addItems({"x86", "x64"});

    inputElevated = new QCheckBox("提升", this);

    labelPid = new QLabel("PID：", this);
    inputPid = new QLineEdit(this);
    labelTid = new QLabel("TID：", this);
    inputTid = new QLineEdit(this);

    layoutProcess->addWidget(labelProcess,   0, 0);
    layoutProcess->addWidget(inputProcess,   0, 1);
    layoutProcess->addWidget(inputArch,      0, 2);
    layoutProcess->addWidget(inputElevated,  0, 3);
    layoutProcess->addWidget(labelPid,       1, 0);
    layoutProcess->addWidget(inputPid,       1, 1);
    layoutProcess->addWidget(labelTid,       1, 2);
    layoutProcess->addWidget(inputTid,       1, 3);

    groupOS = new QGroupBox("操作系统", this);
    layoutOS = new QGridLayout(groupOS);

    labelOs = new QLabel("操作系统：", this);
    inputOs = new QComboBox(this);
    inputOs->addItem("Windows", OS_WINDOWS);
    inputOs->addItem("Linux", OS_LINUX);
    inputOs->addItem("macOS", OS_MAC);
    inputOs->addItem("未知", OS_UNKNOWN);

    inputOsDesc = new QLineEdit(this);
    inputOsDesc->setPlaceholderText("description");

    labelGmtOffset = new QLabel("GMT 偏移：", this);
    inputGmtOffset = new QSpinBox(this);
    inputGmtOffset->setRange(-12, 14);

    layoutOS->addWidget(labelOs,        0, 0);
    layoutOS->addWidget(inputOs,        0, 1);
    layoutOS->addWidget(inputOsDesc,    0, 2);
    layoutOS->addWidget(labelGmtOffset, 0, 3);
    layoutOS->addWidget(inputGmtOffset, 0, 4);

    groupContext = new QGroupBox("上下文", this);
    layoutContext = new QGridLayout(groupContext);

    labelDomain = new QLabel("域：", this);
    inputDomain = new QLineEdit(this);

    labelComputer = new QLabel("计算机：", this);
    inputComputer = new QLineEdit(this);

    labelUsername = new QLabel("用户名：", this);
    inputUsername = new QLineEdit(this);

    labelImpersonated = new QLabel("模拟用户：", this);
    inputImpersonated = new QLineEdit(this);

    layoutContext->addWidget(labelDomain,       0, 0);
    layoutContext->addWidget(inputDomain,       0, 1);
    layoutContext->addWidget(labelComputer,     0, 2);
    layoutContext->addWidget(inputComputer,     0, 3);
    layoutContext->addWidget(labelUsername,     1, 0);
    layoutContext->addWidget(inputUsername,     1, 1);
    layoutContext->addWidget(labelImpersonated, 1, 2);
    layoutContext->addWidget(inputImpersonated, 1, 3);

    buttonUpdate = new QPushButton("更新", this);
    buttonUpdate->setDefault(true);

    buttonCancel = new QPushButton("取消", this);

    horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hLayoutBottom = new QHBoxLayout();
    hLayoutBottom->addItem(horizontalSpacer);
    hLayoutBottom->addWidget(buttonUpdate);
    hLayoutBottom->addWidget(buttonCancel);

    mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(groupNetwork);
    mainLayout->addWidget(groupCoding);
    mainLayout->addWidget(groupProcess);
    mainLayout->addWidget(groupOS);
    mainLayout->addWidget(groupContext);
    mainLayout->addLayout(hLayoutBottom);

    this->setLayout(mainLayout);
}

void DialogAgentData::SetProfile(const AuthProfile &profile)
{
    this->authProfile = profile;
}

void DialogAgentData::SetAgentData(const AgentData &data)
{
    this->agentId = data.Id;

    originalInternalIP   = data.InternalIP;
    originalExternalIP   = data.ExternalIP;
    originalGmtOffset    = data.GmtOffset;
    originalACP          = data.ACP;
    originalOemCP        = data.OemCP;
    originalPid          = data.Pid;
    originalTid          = data.Tid;
    originalArch         = data.Arch;
    originalElevated     = data.Elevated;
    originalProcess      = data.Process;
    originalOs           = data.Os;
    originalOsDesc       = data.OsDesc;
    originalDomain       = data.Domain;
    originalComputer     = data.Computer;
    originalUsername     = data.Username;
    originalImpersonated = data.Impersonated;

    inputInternalIP->setText(data.InternalIP);
    inputExternalIP->setText(data.ExternalIP);
    inputGmtOffset->setValue(data.GmtOffset);
    inputACP->setValue(data.ACP);
    inputOemCP->setValue(data.OemCP);
    inputPid->setText(data.Pid);
    inputTid->setText(data.Tid);
    inputArch->setCurrentText(data.Arch);
    inputElevated->setChecked(data.Elevated);
    inputProcess->setText(data.Process);

    int osIndex = inputOs->findData(data.Os);
    if (osIndex >= 0)
        inputOs->setCurrentIndex(osIndex);

    inputOsDesc->setText(data.OsDesc);
    inputDomain->setText(data.Domain);
    inputComputer->setText(data.Computer);
    inputUsername->setText(data.Username);
    inputImpersonated->setText(data.Impersonated);
}

void DialogAgentData::Start()
{
    this->setModal(true);
    this->show();
}

void DialogAgentData::onButtonUpdate()
{
    QJsonObject updateData;

    if (inputInternalIP->text() != originalInternalIP)
        updateData["internal_ip"] = inputInternalIP->text();

    if (inputExternalIP->text() != originalExternalIP)
        updateData["external_ip"] = inputExternalIP->text();

    if (inputGmtOffset->value() != originalGmtOffset)
        updateData["gmt_offset"] = inputGmtOffset->value();

    if (inputACP->value() != originalACP)
        updateData["acp"] = inputACP->value();

    if (inputOemCP->value() != originalOemCP)
        updateData["oemcp"] = inputOemCP->value();

    if (inputPid->text() != originalPid)
        updateData["pid"] = inputPid->text();

    if (inputTid->text() != originalTid)
        updateData["tid"] = inputTid->text();

    if (inputArch->currentText() != originalArch)
        updateData["arch"] = inputArch->currentText();

    if (inputElevated->isChecked() != originalElevated)
        updateData["elevated"] = inputElevated->isChecked();

    if (inputProcess->text() != originalProcess)
        updateData["process"] = inputProcess->text();

    int currentOs = inputOs->currentData().toInt();
    if (currentOs != originalOs)
        updateData["os"] = currentOs;

    if (inputOsDesc->text() != originalOsDesc)
        updateData["os_desc"] = inputOsDesc->text();

    if (inputDomain->text() != originalDomain)
        updateData["domain"] = inputDomain->text();

    if (inputComputer->text() != originalComputer)
        updateData["computer"] = inputComputer->text();

    if (inputUsername->text() != originalUsername)
        updateData["username"] = inputUsername->text();

    if (inputImpersonated->text() != originalImpersonated)
        updateData["impersonated"] = inputImpersonated->text();

    if (updateData.isEmpty()) {
        this->close();
        return;
    }

    HttpReqAgentUpdateDataAsync(agentId, updateData, authProfile, nullptr);
    this->close();
}

void DialogAgentData::onButtonCancel()
{
    this->close();
}
