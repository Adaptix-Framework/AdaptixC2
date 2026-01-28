#include <UI/Dialogs/DialogConnect.h>
#include <Client/AuthProfile.h>
#include <Client/Storage.h>
#include <MainAdaptix.h>
#include <Utils/NonBlockingDialogs.h>
#include <QVBoxLayout>
#include <QFile>
#include <QIODevice>
#include <QFrame>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>

static QString defaultProjectDir(const QString &projectName)
{
    return QDir::home().filePath("AdaptixProjects/" + projectName.trimmed());
}

bool DialogConnect::parseUrl(QString &host, QString &port, QString &endpoint) const
{
    QUrl url(lineEdit_Url->text().trimmed());
    if (!url.isValid() || url.host().isEmpty())
        return false;

    host = url.host();
    port = url.port(-1) != -1 ? QString::number(url.port()) : "443";
    endpoint = url.path().isEmpty() ? "/" : url.path();
    return true;
}

QString DialogConnect::buildUrl(const QString &host, const QString &port, const QString &endpoint) const
{
    QUrl url;
    url.setScheme("https");
    url.setHost(host);
    if (!port.isEmpty() && port != "443")
        url.setPort(port.toInt());
    url.setPath(endpoint.isEmpty() ? "/" : endpoint);
    return url.toString();
}

DialogConnect::DialogConnect()
{
    createUI();
    loadProjects();

    connect(cardWidget,          &QListWidget::itemPressed,                this, &DialogConnect::onProfileSelected);
    connect(cardWidget,          &QListWidget::customContextMenuRequested, this, &DialogConnect::handleContextMenu);
    connect(buttonNewProfile,    &QPushButton::clicked,                    this, &DialogConnect::onButton_NewProfile);
    connect(buttonLoad,          &QPushButton::clicked,                    this, &DialogConnect::onButton_Load);
    connect(buttonSave,          &QPushButton::clicked,                    this, &DialogConnect::onButton_Save);
    connect(lineEdit_Project,    &QLineEdit::textChanged,                  this, &DialogConnect::onProjectNameChanged);
    connect(lineEdit_ProjectDir, &QLineEdit::textEdited,                   this, &DialogConnect::onProjectDirEdited);
    connect(buttonConnect,       &QPushButton::clicked,                    this, &DialogConnect::onButton_Connect);

    auto connectReturnPressed = [this](const QLineEdit* edit) {
        connect(edit, &QLineEdit::returnPressed, this, &DialogConnect::onButton_Connect);
    };
    connectReturnPressed(lineEdit_Project);
    connectReturnPressed(lineEdit_Url);
    connectReturnPressed(lineEdit_User);
    connectReturnPressed(lineEdit_Password);

    auto action = lineEdit_ProjectDir->addAction(QIcon(":/icons/folder"), QLineEdit::TrailingPosition);
    connect(action, &QAction::triggered, this, &DialogConnect::onSelectProjectDir);

    connect(subsSelectBtn,   &QPushButton::clicked,     this, &DialogConnect::showSubsPopup);
    connect(dataListWidget,  &QListWidget::itemChanged, this, &DialogConnect::onSubsSelectionChanged);
    connect(agentListWidget, &QListWidget::itemChanged, this, &DialogConnect::onSubsSelectionChanged);
}

DialogConnect::~DialogConnect() = default;

void DialogConnect::createUI()
{
    resize(720, 420);
    setFixedSize(720, 420);
    setWindowTitle("Connect");
    setProperty("Main", "base");

    groupUserInfo = new QGroupBox("User Info", this);
    auto userLayout = new QGridLayout(groupUserInfo);
    userLayout->setContentsMargins(10, 10, 10, 10);
    userLayout->setHorizontalSpacing(12);
    userLayout->setVerticalSpacing(8);

    label_User = new QLabel("User:", this);
    label_Password = new QLabel("Password:", this);

    lineEdit_User = new QLineEdit(this);
    lineEdit_User->setToolTip("Enter your username");

    lineEdit_Password = new QLineEdit(this);
    lineEdit_Password->setEchoMode(QLineEdit::Password);
    lineEdit_Password->setToolTip("Enter your password");

    userLayout->addWidget(label_User,        0, 0);
    userLayout->addWidget(lineEdit_User,     0, 1);
    userLayout->addWidget(label_Password,    1, 0);
    userLayout->addWidget(lineEdit_Password, 1, 1);
    userLayout->setColumnMinimumWidth(0, 100);

    groupServerDetails = new QGroupBox("Server Details", this);
    auto serverLayout = new QGridLayout(groupServerDetails);
    serverLayout->setContentsMargins(10, 10, 10, 10);
    serverLayout->setHorizontalSpacing(12);
    serverLayout->setVerticalSpacing(8);

    label_Url = new QLabel("URL:", this);

    lineEdit_Url = new QLineEdit(this);
    lineEdit_Url->setPlaceholderText("https://address:4321/endpoint");
    lineEdit_Url->setToolTip("Enter full server URL (e.g., https://host:port/endpoint)");

    serverLayout->addWidget(label_Url,     0, 0);
    serverLayout->addWidget(lineEdit_Url,  0, 1);
    serverLayout->setColumnMinimumWidth(0, 100);

    groupProject = new QGroupBox("Project", this);
    auto projectLayout = new QGridLayout(groupProject);
    projectLayout->setContentsMargins(10, 10, 10, 10);
    projectLayout->setHorizontalSpacing(12);
    projectLayout->setVerticalSpacing(8);

    label_Project = new QLabel("Name:", this);
    label_ProjectDir = new QLabel("Directory:", this);

    lineEdit_Project = new QLineEdit(this);
    lineEdit_Project->setToolTip("Enter project name");

    lineEdit_ProjectDir = new QLineEdit(this);
    lineEdit_ProjectDir->setToolTip("Enter path to project directory (auto-generated if empty)");

    subsSelectBtn = new QPushButton(QIcon(":/icons/settings_account"), "", this);
    subsSelectBtn->setFixedWidth(30);
    subsSelectBtn->setToolTip("Select subscriptions");

    auto historyLabel = new QLabel("History:");
    historyLabel->setStyleSheet("font-weight: bold;");
    dataListWidget = new QListWidget();
    dataListWidget->setSelectionMode(QAbstractItemView::NoSelection);
    dataListWidget->setStyleSheet(
        "QListWidget::item { padding: 3px 4px; margin: 1px 0px; }"
        "QListWidget::indicator { width: 14px; height: 14px; }"
    );
    auto makeSectionHeader = [](const QString &title) -> QListWidgetItem* {
        auto *item = new QListWidgetItem(title);
        QFont f = item->font();
        f.setBold(true);
        item->setFont(f);
        item->setFlags(Qt::ItemIsEnabled);
        item->setData(Qt::UserRole, true);
        return item;
    };

    dataListWidget->addItem(makeSectionHeader("Data"));
    for (const QString &cat : {"chat_history", "downloads_history", "screenshot_history", "credentials_history", "targets_history"}) {
        auto *item = new QListWidgetItem(cat);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        dataListWidget->addItem(item);
    }
    dataListWidget->addItem(makeSectionHeader("Agent"));
    for (const QString &cat : {"console_history", "tasks_history"}) {
        auto *item = new QListWidgetItem(cat);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        dataListWidget->addItem(item);
    }

    auto realtimeLabel = new QLabel("RealTime:");
    realtimeLabel->setStyleSheet("font-weight: bold;");
    agentListWidget = new QListWidget();
    agentListWidget->setSelectionMode(QAbstractItemView::NoSelection);
    agentListWidget->setStyleSheet(
        "QListWidget::item { padding: 3px 4px; margin: 1px 0px; }"
        "QListWidget::indicator { width: 14px; height: 14px; }"
    );

    agentListWidget->addItem(makeSectionHeader("Data"));
    for (const QString &cat : {"chat_realtime", "downloads_realtime", "screenshot_realtime", "credentials_realtime", "targets_realtime", "notifications", "tunnels"}) {
        auto *item = new QListWidgetItem(cat);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        agentListWidget->addItem(item);
    }
    agentListWidget->addItem(makeSectionHeader("Agent"));
    for (const QString &cat : {"tasks_manager"}) {
        auto *item = new QListWidgetItem(cat);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        agentListWidget->addItem(item);
    }

    multiuserCheck = new QCheckBox("Console Team Mode", this);
    multiuserCheck->setChecked(true);
    multiuserCheck->setToolTip("See console output from all operators");

    auto agentsOnlyActiveCheck = new QCheckBox("Only active agents", this);
    agentsOnlyActiveCheck->setChecked(false);
    agentsOnlyActiveCheck->setToolTip("Synchronize only active agents");

    auto tasksOnlyJobsCheck = new QCheckBox("Only JOB tasks", this);
    tasksOnlyJobsCheck->setChecked(false);
    tasksOnlyJobsCheck->setToolTip("Synchronize only jobs");

    auto leftCol = new QVBoxLayout();
    leftCol->setSpacing(6);
    leftCol->addWidget(historyLabel);
    leftCol->addWidget(dataListWidget);

    auto rightCol = new QVBoxLayout();
    rightCol->setSpacing(6);
    rightCol->addWidget(realtimeLabel);
    rightCol->addWidget(agentListWidget);
    rightCol->addWidget(multiuserCheck);
    rightCol->addWidget(agentsOnlyActiveCheck);
    rightCol->addWidget(tasksOnlyJobsCheck);

    auto popupLayout = new QHBoxLayout();
    popupLayout->setContentsMargins(8, 8, 8, 8);
    popupLayout->setSpacing(12);
    popupLayout->addLayout(leftCol, 1);
    popupLayout->addLayout(rightCol, 1);

    subsPopupDialog = new QDialog(this, Qt::Popup | Qt::FramelessWindowHint);
    subsPopupDialog->setLayout(popupLayout);
    subsPopupDialog->setProperty("Main", "base");
    subsPopupDialog->setMinimumWidth(600);

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(agentsOnlyActiveCheck, &QCheckBox::checkStateChanged, this, &DialogConnect::onSubsSelectionChanged);
    connect(tasksOnlyJobsCheck, &QCheckBox::checkStateChanged, this, &DialogConnect::onSubsSelectionChanged);
#else
    connect(agentsOnlyActiveCheck, &QCheckBox::stateChanged, this, &DialogConnect::onSubsSelectionChanged);
    connect(tasksOnlyJobsCheck, &QCheckBox::stateChanged, this, &DialogConnect::onSubsSelectionChanged);
#endif
    agentsOnlyActiveCheck->setObjectName("agentsOnlyActiveCheck");
    tasksOnlyJobsCheck->setObjectName("tasksOnlyJobsCheck");

    projectLayout->addWidget(label_Project,      0, 0, 1, 1);
    projectLayout->addWidget(lineEdit_Project,   0, 1, 1, 1);
    projectLayout->addWidget(subsSelectBtn,      0, 2, 1, 1);
    projectLayout->addWidget(label_ProjectDir,   1, 0, 1, 1);
    projectLayout->addWidget(lineEdit_ProjectDir,1, 1, 1, 2);
    projectLayout->setColumnMinimumWidth(0, 100);

    buttonConnect = new QPushButton(this);
    buttonConnect->setProperty("ButtonStyle", "dialog_apply");
    buttonConnect->setText("Connect");
    buttonConnect->setFixedWidth(160);
    buttonConnect->setFocus();

    auto actionButtonsLayout = new QHBoxLayout();
    actionButtonsLayout->addStretch();
    actionButtonsLayout->addWidget(buttonConnect);
    actionButtonsLayout->addStretch();

    auto formLayout = new QVBoxLayout();
    formLayout->setContentsMargins(10, 10, 10, 10);
    formLayout->setSpacing(10);
    formLayout->addWidget(groupUserInfo);
    formLayout->addWidget(groupServerDetails);
    formLayout->addWidget(groupProject);
    formLayout->addStretch(1);
    formLayout->addLayout(actionButtonsLayout);

    auto formWidget = new QWidget(this);
    formWidget->setLayout(formLayout);

    menuContext = new QMenu(this);
    menuContext->addAction("Remove", this, &DialogConnect::itemRemove);

    label_Profiles = new QLabel(this);
    label_Profiles->setAlignment(Qt::AlignCenter);
    label_Profiles->setText("Profiles");

    cardWidget = new CardListWidget(this);
    cardWidget->setFixedWidth(240);
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

    auto profileButtonsLayout = new QHBoxLayout();
    profileButtonsLayout->addWidget(buttonNewProfile);
    profileButtonsLayout->addWidget(buttonLoad);
    profileButtonsLayout->addWidget(buttonSave);
    profileButtonsLayout->setSpacing(5);
    profileButtonsLayout->setContentsMargins(0, 0, 0, 0);

    auto profileButtonsWidget = new QWidget(this);
    profileButtonsWidget->setLayout(profileButtonsLayout);

    auto separatorLine = new QFrame(this);
    separatorLine->setFrameShape(QFrame::VLine);
    separatorLine->setFrameShadow(QFrame::Sunken);

    gridLayout = new QGridLayout(this);
    gridLayout->setContentsMargins(5, 5, 5, 5);
    gridLayout->addWidget(formWidget,           0, 0, 3, 1);
    gridLayout->addWidget(separatorLine,        0, 1, 3, 1);
    gridLayout->addWidget(label_Profiles,       0, 2, 1, 1);
    gridLayout->addWidget(cardWidget,           1, 2, 1, 1);
    gridLayout->addWidget(profileButtonsWidget, 2, 2, 1, 1);

    gridLayout->setRowStretch( 0, 0 );
    gridLayout->setRowStretch( 1, 1 );
    gridLayout->setRowStretch( 2, 0 );
    gridLayout->setColumnStretch( 0, 1 );
    gridLayout->setColumnStretch( 1, 0 );
    gridLayout->setColumnStretch( 2, 0 );
    gridLayout->setColumnStretch( 3, 0 );
}

void DialogConnect::loadProjects()
{
    cardWidget->clear();
    listProjects = GlobalClient->storage->ListProjects();
    for (auto& profile : listProjects) {
        QString subtitle = profile.GetUsername() + " @ " + profile.GetHost();
        cardWidget->addCard(profile.GetProject(), subtitle);
    }
}

AuthProfile* DialogConnect::StartDialog()
{
    toConnect = false;
    exec();
    if (!toConnect)
        return nullptr;

    QString projectDir = lineEdit_ProjectDir->text().trimmed();
    if (projectDir.isEmpty())
        projectDir = defaultProjectDir(lineEdit_Project->text());

    QString host, port, endpoint;
    parseUrl(host, port, endpoint);

    auto* newProfile = new AuthProfile(lineEdit_Project->text(), lineEdit_User->text(), lineEdit_Password->text(), host, port, endpoint, projectDir);

    QStringList selectedSubs;
    for (int i = 0; i < dataListWidget->count(); ++i) {
        auto *item = dataListWidget->item(i);
        if (item && item->data(Qt::UserRole).toBool())
            continue;
        if (item->checkState() == Qt::Checked)
            selectedSubs.append(item->text());
    }
    for (int i = 0; i < agentListWidget->count(); ++i) {
        auto *item = agentListWidget->item(i);
        if (item && item->data(Qt::UserRole).toBool())
            continue;
        if (item->checkState() == Qt::Checked)
            selectedSubs.append(item->text());
    }

    auto agentsOnlyActiveCheck = subsPopupDialog ? subsPopupDialog->findChild<QCheckBox*>("agentsOnlyActiveCheck") : nullptr;
    if (agentsOnlyActiveCheck && agentsOnlyActiveCheck->isChecked())
        selectedSubs.append("agents_only_active");

    auto tasksOnlyJobsCheck = subsPopupDialog ? subsPopupDialog->findChild<QCheckBox*>("tasksOnlyJobsCheck") : nullptr;
    if (tasksOnlyJobsCheck && tasksOnlyJobsCheck->isChecked())
        selectedSubs.append("tasks_only_jobs");

    newProfile->SetSubscriptions(selectedSubs);
    newProfile->SetRegisteredCategories(selectedSubs);

    updateSubsDisplay();
    newProfile->SetConsoleMultiuser(multiuserCheck->isChecked());

    if (GlobalClient->storage->ExistsProject(lineEdit_Project->text()))
        GlobalClient->storage->UpdateProject(*newProfile);
    else
        GlobalClient->storage->AddProject(*newProfile);

    return newProfile;
}

void DialogConnect::updateSubsDisplay()
{
    QStringList selectedSubs;
    for (int i = 0; i < dataListWidget->count(); ++i) {
        auto *item = dataListWidget->item(i);
        if (item && item->data(Qt::UserRole).toBool())
            continue;
        if (item->checkState() == Qt::Checked)
            selectedSubs.append(item->text());
    }
    for (int i = 0; i < agentListWidget->count(); ++i) {
        auto *item = agentListWidget->item(i);
        if (item && item->data(Qt::UserRole).toBool())
            continue;
        if (item->checkState() == Qt::Checked)
            selectedSubs.append(item->text());
    }

    auto agentsOnlyActiveCheck = subsPopupDialog ? subsPopupDialog->findChild<QCheckBox*>("agentsOnlyActiveCheck") : nullptr;
    if (agentsOnlyActiveCheck && agentsOnlyActiveCheck->isChecked())
        selectedSubs.append("agents_only_active");

    auto tasksOnlyJobsCheck = subsPopupDialog ? subsPopupDialog->findChild<QCheckBox*>("tasksOnlyJobsCheck") : nullptr;
    if (tasksOnlyJobsCheck && tasksOnlyJobsCheck->isChecked())
        selectedSubs.append("tasks_only_jobs");

    subsSelectBtn->setToolTip(selectedSubs.isEmpty() ? "Select subscriptions" : selectedSubs.join(", "));
}

void DialogConnect::itemRemove()
{
    auto* item = cardWidget->currentItem();
    if (!item)
        return;

    QString project = item->data(CardListWidget::TitleRole).toString();
    Storage::RemoveAllListenerProfiles(project);
    Storage::RemoveAllAgentProfiles(project);
    GlobalClient->storage->RemoveProject(project);
    delete cardWidget->takeItem(cardWidget->row(item));
    loadProjects();
}

void DialogConnect::onProfileSelected()
{
    auto* item = cardWidget->currentItem();
    if (!item)
        return;

    const QString project = item->data(CardListWidget::TitleRole).toString();
    isNewProject = false;
    projectDirTouched = true;

    for (auto& p : listProjects) {
        if (p.GetProject() == project) {
            lineEdit_Project->setText(p.GetProject());
            lineEdit_ProjectDir->setText(p.GetProjectDir());
            lineEdit_Url->setText(buildUrl(p.GetHost(), p.GetPort(), p.GetEndpoint()));
            lineEdit_User->setText(p.GetUsername());
            lineEdit_Password->setText(p.GetPassword());

            QStringList subs = p.GetSubscriptions();
            dataListWidget->blockSignals(true);
            for (int i = 0; i < dataListWidget->count(); ++i) {
                auto *subItem = dataListWidget->item(i);
                if (subItem && subItem->data(Qt::UserRole).toBool())
                    continue;
                subItem->setCheckState(subs.contains(subItem->text()) ? Qt::Checked : Qt::Unchecked);
            }
            dataListWidget->blockSignals(false);
            agentListWidget->blockSignals(true);
            for (int i = 0; i < agentListWidget->count(); ++i) {
                auto *subItem = agentListWidget->item(i);
                if (subItem && subItem->data(Qt::UserRole).toBool())
                    continue;
                subItem->setCheckState(subs.contains(subItem->text()) ? Qt::Checked : Qt::Unchecked);
            }
            agentListWidget->blockSignals(false);
            multiuserCheck->setChecked(p.GetConsoleMultiuser());

            auto agentsOnlyActiveCheck = subsPopupDialog ? subsPopupDialog->findChild<QCheckBox*>("agentsOnlyActiveCheck") : nullptr;
            if (agentsOnlyActiveCheck)
                agentsOnlyActiveCheck->setChecked(subs.contains("agents_only_active"));

            auto tasksOnlyJobsCheck = subsPopupDialog ? subsPopupDialog->findChild<QCheckBox*>("tasksOnlyJobsCheck") : nullptr;
            if (tasksOnlyJobsCheck)
                tasksOnlyJobsCheck->setChecked(subs.contains("tasks_only_jobs"));

            return;
        }
    }
}

void DialogConnect::handleContextMenu(const QPoint &pos)
{
    QPoint globalPos = cardWidget->mapToGlobal( pos );
    menuContext->exec( globalPos );
}

bool DialogConnect::checkValidInput() const
{
    const auto checkEmpty = [](const QLineEdit* edit, const QString& msg) {
        if (edit->text().isEmpty()) {
            MessageError(msg);
            return false;
        }
        return true;
    };

    if (!checkEmpty(lineEdit_Project, "Project is empty")) return false;
    if (!checkEmpty(lineEdit_Url, "URL is empty")) return false;
    if (!checkEmpty(lineEdit_User, "Username is empty")) return false;
    if (!checkEmpty(lineEdit_Password, "Password is empty")) return false;

    QString host, port, endpoint;
    if (!parseUrl(host, port, endpoint)) {
        MessageError("Invalid URL format (Example: https://host:port/endpoint)");
        return false;
    }

    if (GlobalClient->storage->ExistsProject(lineEdit_Project->text()) && isNewProject) {
        MessageError("Project already exists");
        return false;
    }

    return true;
}

void DialogConnect::onButton_Connect()
{
    if (checkValidInput()) {
        toConnect = true;
        close();
    }
}

void DialogConnect::clearFields()
{
    lineEdit_User->clear();
    lineEdit_Password->clear();
    lineEdit_Project->clear();
    lineEdit_ProjectDir->clear();
    lineEdit_Url->clear();
    cardWidget->clearSelection();

    dataListWidget->blockSignals(true);
    for (int i = 0; i < dataListWidget->count(); ++i)
        dataListWidget->item(i)->setCheckState(Qt::Checked);
    dataListWidget->blockSignals(false);
    agentListWidget->blockSignals(true);
    for (int i = 0; i < agentListWidget->count(); ++i)
        agentListWidget->item(i)->setCheckState(Qt::Checked);
    agentListWidget->blockSignals(false);
    multiuserCheck->setChecked(true);

    lineEdit_Project->setFocus();
}

void DialogConnect::onProjectNameChanged(const QString &text)
{
    if (projectDirTouched)
        return;

    const QString name = text.trimmed();
    lineEdit_ProjectDir->setText(name.isEmpty() ? QString() : defaultProjectDir(name));
}

void DialogConnect::onProjectDirEdited(const QString &) { projectDirTouched = true; }

void DialogConnect::onSelectProjectDir()
{
    QString current = lineEdit_ProjectDir->text().trimmed();
    if (current.isEmpty())
        current = defaultProjectDir(lineEdit_Project->text());

    QString dir = QFileDialog::getExistingDirectory(this, "Select project directory", current);
    if (!dir.isEmpty()) {
        lineEdit_ProjectDir->setText(dir);
        projectDirTouched = true;
    }
}

void DialogConnect::onButton_NewProfile()
{
    isNewProject = true;
    projectDirTouched = false;
    clearFields();
}

void DialogConnect::onButton_Load()
{
    QString baseDir = QDir::home().filePath("AdaptixProjects");
    QDir(baseDir).mkpath(".");

    QString projectDir = lineEdit_ProjectDir->text().trimmed();
    if (!projectDir.isEmpty() && QDir(projectDir).exists())
        baseDir = projectDir;

    NonBlockingDialogs::getOpenFileName(this, "Load Profile", baseDir, "Adaptix Profile files (*.adaptixProfile)",
        [this](const QString& filePath) {
            if (filePath.isEmpty())
                return;

            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                MessageError("Failed to open file for reading");
                return;
            }

            QJsonParseError parseError;
            QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
            file.close();

            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                MessageError("Error JSON parse");
                return;
            }

            QJsonObject json = document.object();
            const QStringList requiredFields = {"project", "host", "port", "endpoint", "username", "password"};
            for (const QString& field : requiredFields) {
                if (!json.contains(field) || !json[field].isString()) {
                    MessageError(QString("Required parameter '%1' is missing").arg(field));
                    return;
                }
            }

            const QString project = json["project"].toString();
            isNewProject = !GlobalClient->storage->ExistsProject(project);
            lineEdit_Project->setText(project);

            QString projectDirFinal;
            if (json.contains("projectDir") && json["projectDir"].isString()) {
                projectDirFinal = json["projectDir"].toString();
                projectDirTouched = true;
            } else {
                projectDirFinal = defaultProjectDir(project);
                projectDirTouched = false;
            }
            lineEdit_ProjectDir->setText(projectDirFinal);

            lineEdit_User->setText(json["username"].toString());
            lineEdit_Password->setText(json["password"].toString());
            lineEdit_Url->setText(buildUrl(json["host"].toString(), json["port"].toString(), json["endpoint"].toString()));

            AuthProfile loadedProfile(project, json["username"].toString(), json["password"].toString(), json["host"].toString(), json["port"].toString(), json["endpoint"].toString(), projectDirFinal);

            if (isNewProject)
                GlobalClient->storage->AddProject(loadedProfile);
            else
                GlobalClient->storage->UpdateProject(loadedProfile);

            loadProjects();

            for (int i = 0; i < cardWidget->count(); ++i) {
                if (cardWidget->item(i)->data(CardListWidget::TitleRole).toString() == project) {
                    cardWidget->setCurrentRow(i);
                    onProfileSelected();
                    break;
                }
            }
        });
}

void DialogConnect::onButton_Save()
{
    if (!checkValidInput())
        return;

    const QString projectName = lineEdit_Project->text().trimmed();
    QString projectDir = lineEdit_ProjectDir->text().trimmed();
    if (projectDir.isEmpty())
        projectDir = defaultProjectDir(projectName);

    QString host, port, endpoint;
    parseUrl(host, port, endpoint);

    QJsonObject json;
    json["project"] = projectName;
    json["host"] = host;
    json["port"] = port;
    json["endpoint"] = endpoint;
    json["username"] = lineEdit_User->text().trimmed();
    json["password"] = lineEdit_Password->text();
    json["projectDir"] = projectDir;

    QString baseDir = QDir::homePath();
    QString projectDirText = lineEdit_ProjectDir->text().trimmed();
    if (!projectDirText.isEmpty())
        baseDir = projectDirText;

    QString initialPath = QDir(baseDir).filePath(projectName + ".adaptixProfile");

    NonBlockingDialogs::getSaveFileName(this, "Save Profile", initialPath, "Adaptix Profile files (*.adaptixProfile)",
        [json](const QString& filePath) {
            if (filePath.isEmpty())
                return;

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly)) {
                MessageError("Failed to open file for writing");
                return;
            }

            file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
            file.close();
            MessageSuccess("Profile saved successfully");
        });
}

void DialogConnect::showSubsPopup()
{
    QPoint pos = subsSelectBtn->mapToGlobal(QPoint(0, subsSelectBtn->height()));
    subsPopupDialog->move(pos);
    subsPopupDialog->show();
    subsPopupDialog->raise();
    subsPopupDialog->activateWindow();
}

void DialogConnect::onSubsSelectionChanged()
{
    updateSubsDisplay();
}