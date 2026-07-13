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
    this->setWindowTitle("Edit Agent Data");
    this->setProperty("Main", "base");
    this->setMinimumWidth(600);

    groupIdentity = new QGroupBox("Identity", this);
    layoutIdentity = new QGridLayout(groupIdentity);
    layoutIdentity->setContentsMargins(12, 12, 12, 12);
    layoutIdentity->setSpacing(8);

    auto* labelDomain = new QLabel("Domain", groupIdentity);
    inputDomain = new QLineEdit(groupIdentity);
    auto* labelComputer = new QLabel("Computer", groupIdentity);
    inputComputer = new QLineEdit(groupIdentity);
    auto* labelUsername = new QLabel("Username", groupIdentity);
    inputUsername = new QLineEdit(groupIdentity);
    auto* labelImpersonated = new QLabel("Impersonated", groupIdentity);
    inputImpersonated = new QLineEdit(groupIdentity);

    layoutIdentity->addWidget(labelDomain,       0, 0);
    layoutIdentity->addWidget(inputDomain,       0, 1);
    layoutIdentity->addWidget(labelComputer,     1, 0);
    layoutIdentity->addWidget(inputComputer,     1, 1);
    layoutIdentity->addWidget(labelUsername,     2, 0);
    layoutIdentity->addWidget(inputUsername,     2, 1);
    layoutIdentity->addWidget(labelImpersonated, 3, 0);
    layoutIdentity->addWidget(inputImpersonated, 3, 1);

    groupProcess = new QGroupBox("Process", this);
    layoutProcess = new QGridLayout(groupProcess);
    layoutProcess->setContentsMargins(12, 12, 12, 12);
    layoutProcess->setSpacing(8);

    auto* labelProcess = new QLabel("Process", groupProcess);
    inputProcess = new QLineEdit(groupProcess);
    auto* labelArch = new QLabel("Arch", groupProcess);
    inputArch = new QComboBox(groupProcess);
    inputArch->addItems({"x64", "x86"});
    auto* labelPid = new QLabel("PID", groupProcess);
    inputPid = new QSpinBox(groupProcess);
    inputPid->setRange(0, 999999);
    auto* labelTid = new QLabel("TID", groupProcess);
    inputTid = new QSpinBox(groupProcess);
    inputTid->setRange(0, 999999);
    auto* labelElevated = new QLabel("Elevated", groupProcess);
    inputElevated = new oclero::qlementine::Switch(groupProcess);

    layoutProcess->addWidget(labelProcess,  0, 0);
    layoutProcess->addWidget(inputProcess,  0, 1);
    layoutProcess->addWidget(labelArch,     1, 0);
    layoutProcess->addWidget(inputArch,     1, 1);
    layoutProcess->addWidget(labelPid,      2, 0);
    layoutProcess->addWidget(inputPid,      2, 1);
    layoutProcess->addWidget(labelTid,      3, 0);
    layoutProcess->addWidget(inputTid,      3, 1);
    layoutProcess->addWidget(labelElevated, 4, 0);
    layoutProcess->addWidget(inputElevated, 4, 1);

    auto* leftLayout = new QVBoxLayout();
    leftLayout->addWidget(groupIdentity);
    leftLayout->addWidget(groupProcess);

    groupNetwork = new QGroupBox("Network", this);
    layoutNetwork = new QGridLayout(groupNetwork);
    layoutNetwork->setContentsMargins(12, 12, 12, 12);
    layoutNetwork->setSpacing(8);

    auto* labelInternalIP = new QLabel("Internal IP", groupNetwork);
    inputInternalIP = new QLineEdit(groupNetwork);
    auto* labelExternalIP = new QLabel("External IP", groupNetwork);
    inputExternalIP = new QLineEdit(groupNetwork);

    layoutNetwork->addWidget(labelInternalIP, 0, 0);
    layoutNetwork->addWidget(inputInternalIP, 0, 1);
    layoutNetwork->addWidget(labelExternalIP, 1, 0);
    layoutNetwork->addWidget(inputExternalIP, 1, 1);

    groupOS = new QGroupBox("OS", this);
    layoutOS = new QGridLayout(groupOS);
    layoutOS->setContentsMargins(12, 12, 12, 12);
    layoutOS->setSpacing(8);

    auto* labelOs = new QLabel("Type", groupOS);
    inputOs = new QComboBox(groupOS);
    inputOs->addItem("Windows", OS_WINDOWS);
    inputOs->addItem("Linux", OS_LINUX);
    inputOs->addItem("macOS", OS_MAC);
    inputOs->addItem("Unknown", OS_UNKNOWN);
    auto* labelOsDesc = new QLabel("Description", groupOS);
    inputOsDesc = new QLineEdit(groupOS);
    auto* labelGmtOffset = new QLabel("GMT", groupOS);
    inputGmtOffset = new QSpinBox(groupOS);
    inputGmtOffset->setRange(-12, 14);
    auto* labelACP = new QLabel("ACP", groupOS);
    inputACP = new QSpinBox(groupOS);
    inputACP->setRange(0, 65535);
    auto* labelOemCP = new QLabel("OEM CP", groupOS);
    inputOemCP = new QSpinBox(groupOS);
    inputOemCP->setRange(0, 65535);

    layoutOS->addWidget(labelOs,        0, 0);
    layoutOS->addWidget(inputOs,        0, 1);
    layoutOS->addWidget(labelOsDesc,    1, 0);
    layoutOS->addWidget(inputOsDesc,    1, 1);
    layoutOS->addWidget(labelGmtOffset, 2, 0);
    layoutOS->addWidget(inputGmtOffset, 2, 1);
    layoutOS->addWidget(labelACP,       3, 0);
    layoutOS->addWidget(inputACP,       3, 1);
    layoutOS->addWidget(labelOemCP,     4, 0);
    layoutOS->addWidget(inputOemCP,     4, 1);

    auto* rightLayout = new QVBoxLayout();
    rightLayout->addWidget(groupNetwork);
    rightLayout->addWidget(groupOS);

    columnsLayout = new QHBoxLayout();
    columnsLayout->addLayout(leftLayout);
    columnsLayout->addLayout(rightLayout);

    buttonUpdate = new QPushButton("Update", this);
    buttonUpdate->setDefault(true);
    buttonCancel = new QPushButton("Cancel", this);

    buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(buttonCancel);
    buttonLayout->addWidget(buttonUpdate);

    mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(columnsLayout);
    mainLayout->addLayout(buttonLayout);

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
    originalPid          = data.Pid.toInt();
    originalTid          = data.Tid.toInt();
    originalArch         = data.Arch;
    originalElevated     = data.Elevated;
    originalProcess      = data.Process;
    originalOs           = data.Os;
    originalOsDesc       = data.OsDesc;
    originalDomain       = data.Domain;
    originalComputer     = data.Computer;
    originalUsername     = data.Username;
    originalImpersonated = data.Impersonated;

    inputDomain->setText(data.Domain);
    inputComputer->setText(data.Computer);
    inputUsername->setText(data.Username);
    inputImpersonated->setText(data.Impersonated);
    inputProcess->setText(data.Process);
    inputArch->setCurrentText(data.Arch);
    inputPid->setValue(data.Pid.toInt());
    inputTid->setValue(data.Tid.toInt());
    inputElevated->setChecked(data.Elevated);
    inputInternalIP->setText(data.InternalIP);
    inputExternalIP->setText(data.ExternalIP);

    int osIndex = inputOs->findData(data.Os);
    if (osIndex >= 0)
        inputOs->setCurrentIndex(osIndex);

    inputOsDesc->setText(data.OsDesc);
    inputGmtOffset->setValue(data.GmtOffset);
    inputACP->setValue(data.ACP);
    inputOemCP->setValue(data.OemCP);
}

void DialogAgentData::Start()
{
    this->setModal(true);
    this->show();
}

void DialogAgentData::onButtonUpdate()
{
    QJsonObject updateData;

    if (inputDomain->text() != originalDomain)
        updateData["domain"] = inputDomain->text();

    if (inputComputer->text() != originalComputer)
        updateData["computer"] = inputComputer->text();

    if (inputUsername->text() != originalUsername)
        updateData["username"] = inputUsername->text();

    if (inputImpersonated->text() != originalImpersonated)
        updateData["impersonated"] = inputImpersonated->text();

    if (inputProcess->text() != originalProcess)
        updateData["process"] = inputProcess->text();

    if (inputArch->currentText() != originalArch)
        updateData["arch"] = inputArch->currentText();

    if (inputPid->value() != originalPid)
        updateData["pid"] = QString::number(inputPid->value());

    if (inputTid->value() != originalTid)
        updateData["tid"] = QString::number(inputTid->value());

    if (inputElevated->isChecked() != originalElevated)
        updateData["elevated"] = inputElevated->isChecked();

    if (inputInternalIP->text() != originalInternalIP)
        updateData["internal_ip"] = inputInternalIP->text();

    if (inputExternalIP->text() != originalExternalIP)
        updateData["external_ip"] = inputExternalIP->text();

    int currentOs = inputOs->currentData().toInt();
    if (currentOs != originalOs)
        updateData["os"] = currentOs;

    if (inputOsDesc->text() != originalOsDesc)
        updateData["os_desc"] = inputOsDesc->text();

    if (inputGmtOffset->value() != originalGmtOffset)
        updateData["gmt_offset"] = inputGmtOffset->value();

    if (inputACP->value() != originalACP)
        updateData["acp"] = inputACP->value();

    if (inputOemCP->value() != originalOemCP)
        updateData["oemcp"] = inputOemCP->value();

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
