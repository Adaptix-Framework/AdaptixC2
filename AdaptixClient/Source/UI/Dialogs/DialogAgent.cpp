#include <UI/Dialogs/DialogAgent.h>
#include <Utils/NonBlockingDialogs.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/Storage.h>
#include <Client/AxScript/AxElementWrappers.h>

DialogAgent::DialogAgent(const QString &listenerName, const QString &listenerType)
{
    this->createUI();

    this->listenerInput->setText(listenerName);

    this->listenerName = listenerName;
    this->listenerType = listenerType;

    connect(cardWidget,       &QListWidget::itemPressed,                this, &DialogAgent::onProfileSelected);
    connect(cardWidget,       &QListWidget::customContextMenuRequested, this, &DialogAgent::handleProfileContextMenu);
    connect(agentCombobox,    &QComboBox::currentTextChanged,           this, &DialogAgent::changeConfig);
    connect(generateButton,   &QPushButton::clicked,                    this, &DialogAgent::onButtonGenerate);
    connect(buttonNewProfile, &QPushButton::clicked,                    this, &DialogAgent::onButtonNewProfile);
    connect(buttonLoad,       &QPushButton::clicked,                    this, &DialogAgent::onButtonLoad);
    connect(buttonSave,       &QPushButton::clicked,                    this, &DialogAgent::onButtonSave);
    connect(inputProfileName, &QLineEdit::textEdited,                   this, &DialogAgent::onProfileNameEdited);
    connect(actionSaveProfile,&QAction::toggled,                        this, &DialogAgent::onSaveProfileToggled);
}

DialogAgent::~DialogAgent() = default;

void DialogAgent::createUI()
{
    this->setWindowTitle("Generate Agent");
    this->setProperty("Main", "base");

    listenerLabel = new QLabel("Listener:", this);
    listenerInput = new QLineEdit(this);
    listenerInput->setReadOnly(true);

    agentLabel    = new QLabel("Agent:", this);
    agentCombobox = new QComboBox(this);

    profileLabel = new QLabel("Profile:", this);

    inputProfileName = new QLineEdit(this);
    inputProfileName->setToolTip("Profile name");

    actionSaveProfile = new QAction(this);
    actionSaveProfile->setCheckable(true);
    actionSaveProfile->setChecked(true);
    actionSaveProfile->setToolTip("Click to toggle: Save as profile");
    actionSaveProfile->setIcon(QIcon(":/icons/check"));
    inputProfileName->addAction(actionSaveProfile, QLineEdit::TrailingPosition);

    auto stackGridLayout = new QGridLayout();
    stackGridLayout->setHorizontalSpacing(0);
    stackGridLayout->setContentsMargins(0, 0, 0, 0);

    configStackWidget = new QStackedWidget(this);
    stackGridLayout->addWidget(configStackWidget, 0, 0, 1, 1);

    agentConfigGroupbox = new QGroupBox("Agent config", this);
    agentConfigGroupbox->setLayout(stackGridLayout);

    generateButton = new QPushButton("Generate", this);
    generateButton->setProperty("ButtonStyle", "dialog_apply");
    generateButton->setFixedWidth(160);
    generateButton->setFocus();

    menuContext = new QMenu(this);
    menuContext->addAction("Rename", this, &DialogAgent::onProfileRename);
    menuContext->addAction("Remove", this, &DialogAgent::onProfileRemove);

    label_Profiles = new QLabel(this);
    label_Profiles->setAlignment(Qt::AlignCenter);
    label_Profiles->setText("Profiles");

    cardWidget = new CardListWidget(this);
    cardWidget->setFixedWidth(220);
    cardWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    cardWidget->addAction(menuContext->menuAction());
    cardWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    cardWidget->setFocusPolicy(Qt::NoFocus);

    buttonNewProfile = new QPushButton(this);
    buttonNewProfile->setProperty("ButtonStyle", "dialog");
    buttonNewProfile->setText("New Profile");
    buttonNewProfile->setMinimumSize(QSize(10, 30));

    buttonLoad = new QPushButton(QIcon(":/icons/file_open"), "", this);
    buttonLoad->setProperty("ButtonStyle", "dialog");
    buttonLoad->setIconSize(QSize(20, 20));
    buttonLoad->setFixedSize(QSize(30, 30));
    buttonLoad->setToolTip("Load profile from file");

    buttonSave = new QPushButton(QIcon(":/icons/save_as"), "", this);
    buttonSave->setProperty("ButtonStyle", "dialog");
    buttonSave->setIconSize(QSize(20, 20));
    buttonSave->setFixedSize(QSize(30, 30));
    buttonSave->setToolTip("Save profile to file");

    auto leftPanelLayout = new QGridLayout();
    leftPanelLayout->setVerticalSpacing(8);
    leftPanelLayout->setHorizontalSpacing(8);
    leftPanelLayout->setContentsMargins(5, 5, 5, 5);

    leftPanelLayout->addWidget(listenerLabel,       0, 0);
    leftPanelLayout->addWidget(listenerInput,       0, 1);

    leftPanelLayout->addWidget(agentLabel,          1, 0);
    leftPanelLayout->addWidget(agentCombobox,       1, 1);

    leftPanelLayout->addWidget(profileLabel,        2, 0);
    leftPanelLayout->addWidget(inputProfileName,    2, 1);

    leftPanelLayout->addWidget(agentConfigGroupbox, 3, 0, 1, 2);

    leftPanelLayout->setRowStretch(0, 0);
    leftPanelLayout->setRowStretch(1, 0);
    leftPanelLayout->setRowStretch(2, 0);
    leftPanelLayout->setRowStretch(3, 1);
    leftPanelLayout->setColumnStretch(0, 0);
    leftPanelLayout->setColumnStretch(1, 1);

    auto actionButtonsLayout = new QHBoxLayout();
    actionButtonsLayout->addStretch();
    actionButtonsLayout->addWidget(generateButton);
    actionButtonsLayout->addStretch();

    auto formLayout = new QVBoxLayout();
    formLayout->setContentsMargins(10, 10, 10, 10);
    formLayout->setSpacing(10);
    formLayout->addLayout(leftPanelLayout);
    formLayout->addStretch(1);
    formLayout->addLayout(actionButtonsLayout);

    auto formWidget = new QWidget(this);
    formWidget->setLayout(formLayout);

    auto separatorLine = new QFrame(this);
    separatorLine->setFrameShape(QFrame::VLine);
    separatorLine->setFrameShadow(QFrame::Sunken);
    separatorLine->setStyleSheet("QFrame { color: rgba(100, 100, 100, 50); background-color: rgba(100, 100, 100, 50); }");

    auto profileButtonsLayout = new QHBoxLayout();
    profileButtonsLayout->setContentsMargins(0, 0, 0, 0);
    profileButtonsLayout->setSpacing(5);
    profileButtonsLayout->addWidget(buttonNewProfile, 1);
    profileButtonsLayout->addWidget(buttonLoad);
    profileButtonsLayout->addWidget(buttonSave);


    auto profileButtonsWidget = new QWidget(this);
    profileButtonsWidget->setLayout(profileButtonsLayout);

    auto mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(5, 5, 5, 5);
    mainGridLayout->addWidget(formWidget,            0, 0, 3, 1);
    mainGridLayout->addWidget(separatorLine,         0, 1, 3, 1);
    mainGridLayout->addWidget(label_Profiles,        0, 2, 1, 1);
    mainGridLayout->addWidget(cardWidget,            1, 2, 1, 1);
    mainGridLayout->addWidget(profileButtonsWidget,  2, 2, 1, 1);
    mainGridLayout->setColumnStretch(0, 1);
    mainGridLayout->setColumnStretch(1, 0);
    mainGridLayout->setColumnStretch(2, 0);

    this->setLayout(mainGridLayout);
}

void DialogAgent::Start()
{
    this->setModal(true);
    this->show();
}

void DialogAgent::AddExAgents(const QStringList &agents, const QMap<QString, AxUI> &uis)
{
    agentCombobox->clear();

    this->agents = agents;
    this->ax_uis = uis;

    for (auto agent : agents) {
        auto ax_ui = &this->ax_uis[agent];
        ax_ui->widget->setParent(nullptr);
        ax_ui->widget->setParent(this);
        ax_ui->container->setParent(nullptr);
        ax_ui->container->setParent(this);

        configStackWidget->addWidget(ax_ui->widget);

        agentCombobox->addItem(agent);
    }
 }

void DialogAgent::SetProfile(const AuthProfile &profile)
{
    this->authProfile = profile;
    loadProfiles();
}

void DialogAgent::onButtonGenerate()
{
    QString agentName  = agentCombobox->currentText();
    QString profileName = inputProfileName->text().trimmed();
    bool shouldSaveProfile = actionSaveProfile->isChecked() && !profileName.isEmpty();

    auto configData = QString();
    if (ax_uis.contains(agentName) && ax_uis[agentName].container)
        configData = ax_uis[agentName].container->toJson();

    QString baseDir = authProfile.GetProjectDir();

    HttpReqAgentGenerateAsync(listenerName, listenerType, agentName, configData, authProfile, 
        [this, baseDir, agentName, configData, profileName, shouldSaveProfile](bool success, const QString &message, const QJsonObject&) {
        if (!success) {
            MessageError(message);
            return;
        }

        if (shouldSaveProfile) {
            saveProfile(profileName, agentName, configData);
            loadProfiles();
        }

        QStringList parts = message.split(":");
        if (parts.size() != 2) {
            MessageError("The response format is not supported");
            return;
        }

        QByteArray content     = QByteArray::fromBase64(parts[1].toUtf8());
        QString    filename    = QString( QByteArray::fromBase64(parts[0].toUtf8()));
        QString    initialPath = QDir(baseDir).filePath(filename);

        NonBlockingDialogs::getSaveFileName(this, "Save File", initialPath, "All Files (*.*)",
            [this, content](const QString& filePath) {
                if (filePath.isEmpty())
                    return;

                QFile file(filePath);
                if (!file.open(QIODevice::WriteOnly)) {
                    MessageError("Failed to open file for writing");
                    return;
                }

                file.write(content);
                file.close();

                QInputDialog inputDialog;
                inputDialog.setWindowTitle("Save agent");
                inputDialog.setLabelText("File saved to:");
                inputDialog.setTextEchoMode(QLineEdit::Normal);
                inputDialog.setTextValue(filePath);
                inputDialog.adjustSize();
                inputDialog.move(QGuiApplication::primaryScreen()->geometry().center() - inputDialog.geometry().center());
                inputDialog.exec();

                this->close();
            });
    });
}

void DialogAgent::onButtonLoad()
{
    QString baseDir = authProfile.GetProjectDir();
    NonBlockingDialogs::getOpenFileName(this, "Select file", baseDir, "JSON files (*.json)",
        [this](const QString& filePath) {
            if (filePath.isEmpty())
                return;

            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly))
                return;

            QByteArray fileContent = file.readAll();
            file.close();

            QJsonParseError parseError;
            QJsonDocument document = QJsonDocument::fromJson(fileContent, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                MessageError("Error JSON parse");
                return;
            }
            QJsonObject jsonObject = document.object();

            if ( !jsonObject.contains("listener_type") || !jsonObject["listener_type"].isString() ) {
                MessageError("Required parameter 'listener_type' is missing");
                return;
            }
            if ( !jsonObject.contains("agent") || !jsonObject["agent"].isString() ) {
                MessageError("Required parameter 'agent' is missing");
                return;
            }
            if ( !jsonObject.contains("config") || !jsonObject["config"].isString() ) {
                MessageError("Required parameter 'config' is missing");
                return;
            }

            if(listenerType != jsonObject["listener_type"].toString()) {
                MessageError("Listener type mismatch");
                return;
            }

            QString agentType = jsonObject["agent"].toString();
            int typeIndex = agentCombobox->findText( agentType );
            if ( typeIndex == -1 ) {
                MessageError("No such agent exists");
                return;
            }
            agentCombobox->setCurrentIndex(typeIndex);
            this->changeConfig(agentType);

            QString configData = jsonObject["config"].toString();

            ax_uis[agentType].container->fromJson(configData);
    });
}

void DialogAgent::onButtonSave()
{
    QString configType = agentCombobox->currentText();
    auto configData = QString();
    if (ax_uis.contains(configType) && ax_uis[configType].container)
        configData = ax_uis[configType].container->toJson();

    QJsonObject dataJson;
    dataJson["listener_type"] = listenerType;
    dataJson["agent"]         = configType;
    dataJson["config"]        = configData;
    QByteArray fileContent = QJsonDocument(dataJson).toJson();

    QString tmpFilename = QString("%1_config.json").arg(configType);
    QString baseDir     = authProfile.GetProjectDir();
    QString initialPath = QDir(baseDir).filePath(tmpFilename);
    NonBlockingDialogs::getSaveFileName(this, "Save File", initialPath, "JSON files (*.json)",
        [this, fileContent](const QString& filePath) {
            if (filePath.isEmpty())
                return;

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly)) {
                MessageError("Failed to open file for writing");
                return;
            }

            file.write(fileContent);
            file.close();

            QInputDialog inputDialog;
            inputDialog.setWindowTitle("Save config");
            inputDialog.setLabelText("File saved to:");
            inputDialog.setTextEchoMode(QLineEdit::Normal);
            inputDialog.setTextValue(filePath);
            inputDialog.adjustSize();
            inputDialog.move(QGuiApplication::primaryScreen()->geometry().center() - inputDialog.geometry().center());
            inputDialog.exec();
    });
}

void DialogAgent::changeConfig(const QString &agentName)
{
    if (ax_uis.contains(agentName)) {
        auto ax_ui = &ax_uis[agentName];
        if (ax_ui)
            configStackWidget->setCurrentWidget(ax_ui->widget);
        this->resize(ax_ui->width, ax_ui->height);
    }

    QString baseName = agentName;
    if (!profileNameManuallyEdited)
        inputProfileName->setText(generateUniqueProfileName(baseName));
}

void DialogAgent::loadProfiles()
{
    cardWidget->clear();

    QString project = authProfile.GetProject();
    if (project.isEmpty())
        return;

    QVector<QPair<QString, QString>> profiles = Storage::ListAgentProfiles(project);
    for (const auto& profile : profiles) {
        QString profileName = profile.first;
        QString profileData = profile.second;

        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson(profileData.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            continue;

        QJsonObject jsonObject = document.object();

        QString profileListenerType = jsonObject.contains("listener_type") && jsonObject["listener_type"].isString()
                                      ? jsonObject["listener_type"].toString()
                                      : "";

        if (profileListenerType != listenerType)
            continue;

        QString profileListener = jsonObject.contains("listener") && jsonObject["listener"].isString()
                                  ? jsonObject["listener"].toString()
                                  : "";
        QString timestamp = jsonObject.contains("timestamp") && jsonObject["timestamp"].isString()
                            ? jsonObject["timestamp"].toString()
                            : "";

        QString subtitle = profileListener;
        if (!timestamp.isEmpty())
            subtitle = profileListener + " | " + timestamp;

        cardWidget->addCard(profileName, subtitle);
    }
}

void DialogAgent::saveProfile(const QString &profileName, const QString &agentName, const QString &configData)
{
    QJsonObject dataJson;
    dataJson["listener_type"] = listenerType;
    dataJson["listener"]      = listenerName;
    dataJson["agent"]         = agentName;
    dataJson["config"]        = configData;
    dataJson["timestamp"]     = QDateTime::currentDateTime().toString("dd.MM hh:mm");
    QString profileData = QJsonDocument(dataJson).toJson(QJsonDocument::Compact);

    Storage::AddAgentProfile(authProfile.GetProject(), profileName, profileData);
}

QString DialogAgent::generateUniqueProfileName(const QString &baseName)
{
    QString project = authProfile.GetProject();
    if (project.isEmpty())
        return baseName + "_1";

    QVector<QPair<QString, QString>> profiles = Storage::ListAgentProfiles(project);

    QSet<QString> existingNames;
    for (const auto& profile : profiles)
        existingNames.insert(profile.first);

    int num = 1;
    QString candidate;
    do {
        candidate = QString("%1_%2").arg(baseName).arg(num);
        num++;
    } while (existingNames.contains(candidate));

    return candidate;
}

void DialogAgent::onButtonNewProfile()
{
    inputProfileName->clear();
    cardWidget->clearSelection();
    actionSaveProfile->setChecked(true);
    profileNameManuallyEdited = false;

    QString agentName = agentCombobox->currentText();
    if (!agentName.isEmpty())
        inputProfileName->setText(generateUniqueProfileName(agentName));
}

void DialogAgent::onProfileSelected()
{
    auto* item = cardWidget->currentItem();
    if (!item)
        return;

    QString profileName = item->data(CardListWidget::TitleRole).toString();
    if (profileName.isEmpty())
        return;

    QString project = authProfile.GetProject();
    if (project.isEmpty())
        return;

    QString profileData = Storage::GetAgentProfile(project, profileName);
    if (profileData.isEmpty())
        return;

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(profileData.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        MessageError("Error parsing profile data");
        return;
    }

    QJsonObject jsonObject = document.object();

    profileNameManuallyEdited = true;
    inputProfileName->setText(profileName);

    if (jsonObject.contains("agent") && jsonObject["agent"].isString()) {
        QString agentType = jsonObject["agent"].toString();
        int typeIndex = agentCombobox->findText(agentType);
        if (typeIndex != -1 && ax_uis.contains(agentType)) {
            agentCombobox->setCurrentIndex(typeIndex);
            if (jsonObject.contains("config") && jsonObject["config"].isString()) {
                QString configData = jsonObject["config"].toString();
                ax_uis[agentType].container->fromJson(configData);
            }
        }
    }
}

void DialogAgent::handleProfileContextMenu(const QPoint &pos)
{
    QPoint globalPos = cardWidget->mapToGlobal(pos);
    menuContext->exec(globalPos);
}

void DialogAgent::onProfileRemove()
{
    auto* item = cardWidget->currentItem();
    if (!item)
        return;

    QString profileName = item->data(CardListWidget::TitleRole).toString();
    if (!profileName.isEmpty()) {
        QString project = authProfile.GetProject();
        if (!project.isEmpty())
            Storage::RemoveAgentProfile(project, profileName);
    }

    delete cardWidget->takeItem(cardWidget->row(item));
    loadProfiles();
}

void DialogAgent::onProfileRename()
{
    auto* item = cardWidget->currentItem();
    if (!item)
        return;

    QString oldName = item->data(CardListWidget::TitleRole).toString();
    if (oldName.isEmpty())
        return;

    QString project = authProfile.GetProject();
    if (project.isEmpty())
        return;

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Profile", "New profile name:", QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.trimmed().isEmpty() || newName == oldName)
        return;

    newName = newName.trimmed();

    QString profileData = Storage::GetAgentProfile(project, oldName);
    if (profileData.isEmpty())
        return;

    Storage::RemoveAgentProfile(project, oldName);
    Storage::AddAgentProfile(project, newName, profileData);
    loadProfiles();

    if (inputProfileName->text() == oldName)
        inputProfileName->setText(newName);
}

void DialogAgent::onProfileNameEdited(const QString &text)
{
    Q_UNUSED(text);
    profileNameManuallyEdited = true;
}

void DialogAgent::onSaveProfileToggled(bool checked)
{
    if (checked)
        actionSaveProfile->setIcon(QIcon(":/icons/check"));
    else
        actionSaveProfile->setIcon(QIcon(":/icons/close"));
}