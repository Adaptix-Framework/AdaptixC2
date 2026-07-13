#include <UI/Dialogs/DialogListener.h>
#include <Utils/NonBlockingDialogs.h>
#include <Client/Requestor.h>
#include <Client/Storage.h>
#include <Client/AxScript/AxElementWrappers.h>

DialogListener::DialogListener(QWidget *parent) : QDialog(parent)
{
    this->createUI();

    connect(cardWidget,           &QListWidget::itemPressed,                this, &DialogListener::onProfileSelected);
    connect(cardWidget,           &QListWidget::customContextMenuRequested, this, &DialogListener::handleProfileContextMenu);
    connect(listenerCombobox,     &QComboBox::currentTextChanged,           this, &DialogListener::changeConfig);
    connect(listenerTypeCombobox, &QComboBox::currentTextChanged,           this, &DialogListener::changeType);
    connect(buttonCreate,         &QPushButton::clicked,                    this, &DialogListener::onButtonCreate );
    connect(cancelButton,         &QPushButton::clicked,                    this, &QDialog::close );
    connect(buttonNewProfile,     &QPushButton::clicked,                    this, &DialogListener::onButtonNewProfile );
    connect(buttonLoad,           &QPushButton::clicked,                    this, &DialogListener::onButtonLoad );
    connect(buttonSave,           &QPushButton::clicked,                    this, &DialogListener::onButtonSave );
    connect(inputListenerName,    &QLineEdit::textChanged,                  this, &DialogListener::onListenerNameChanged);
    connect(inputProfileName,     &QLineEdit::textEdited,                   this, &DialogListener::onProfileNameEdited);
    connect(actionSaveProfile,    &QAction::toggled,                        this, &DialogListener::onSaveProfileToggled);
}

DialogListener::~DialogListener() = default;

void DialogListener::createUI()
{
    this->setWindowTitle("Create Listener");
    this->setProperty("Main", "base");

    listenerTypeLabel = new QLabel("Protocol:", this);
    listenerTypeCombobox = new QComboBox(this);

    listenerLabel = new QLabel("Config:", this);
    listenerCombobox = new QComboBox(this);

    listenerNameLabel = new QLabel("Name:", this);
    inputListenerName = new QLineEdit(this);
    inputListenerName->setToolTip("Listener name");

    configStackWidget = new QStackedWidget(this);

    stackGridLayout = new QGridLayout(this);
    stackGridLayout->setHorizontalSpacing(0);
    stackGridLayout->setContentsMargins(0, 0, 0, 0);
    stackGridLayout->addWidget(configStackWidget, 0, 0, 1, 1);

    listenerConfigGroupbox = new QGroupBox("Listener Configuration", this);
    listenerConfigGroupbox->setLayout(stackGridLayout);

    auto leftPanelLayout = new QGridLayout();
    leftPanelLayout->setVerticalSpacing(8);
    leftPanelLayout->setHorizontalSpacing(8);
    leftPanelLayout->setContentsMargins(5, 5, 5, 5);

    leftPanelLayout->addWidget(listenerTypeLabel,      0, 0);
    leftPanelLayout->addWidget(listenerTypeCombobox,   0, 1);
    leftPanelLayout->addWidget(listenerLabel,          0, 2);
    leftPanelLayout->addWidget(listenerCombobox,       0, 3);

    leftPanelLayout->addWidget(listenerNameLabel,      1, 0);
    leftPanelLayout->addWidget(inputListenerName,      1, 1, 1, 3);

    leftPanelLayout->addWidget(listenerConfigGroupbox, 2, 0, 1, 4);

    leftPanelLayout->setRowStretch(0, 0);
    leftPanelLayout->setRowStretch(1, 0);
    leftPanelLayout->setRowStretch(2, 1);
    leftPanelLayout->setColumnStretch(0, 0);
    leftPanelLayout->setColumnStretch(1, 1);
    leftPanelLayout->setColumnStretch(2, 0);
    leftPanelLayout->setColumnStretch(3, 1);

    auto formLayout = new QVBoxLayout();
    formLayout->setContentsMargins(10, 10, 10, 10);
    formLayout->setSpacing(8);
    formLayout->addLayout(leftPanelLayout, 1);

    auto formWidget = new QWidget(this);
    formWidget->setLayout(formLayout);

    auto separatorLine = new QFrame(this);
    separatorLine->setFrameShape(QFrame::VLine);
    separatorLine->setFrameShadow(QFrame::Sunken);

    menuContext = new oclero::qlementine::Menu(this);
    menuContext->addAction("Rename", this, &DialogListener::onProfileRename);
    menuContext->addAction("Remove", this, &DialogListener::onProfileRemove);

    label_Profiles = new QLabel("Profiles", this);
    label_Profiles->setObjectName("ProfilesHeader");
    label_Profiles->setStyleSheet("QLabel#ProfilesHeader { color: palette(placeholderText); font-size: 10px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.5px; }");

    inputProfileName = new QLineEdit(this);
    inputProfileName->setToolTip("Profile name");
    inputProfileName->setPlaceholderText("Profile name...");

    actionSaveProfile = new QAction(this);
    actionSaveProfile->setCheckable(true);
    actionSaveProfile->setChecked(true);
    actionSaveProfile->setToolTip("Click to toggle: Save as profile");
    actionSaveProfile->setIcon(QIcon(":/icons/check"));
    inputProfileName->addAction(actionSaveProfile, QLineEdit::TrailingPosition);

    cardWidget = new CardListWidget(this);
    cardWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    cardWidget->addAction(menuContext->menuAction());
    cardWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    cardWidget->setFocusPolicy(Qt::NoFocus);

    auto profileSeparator = new QFrame(this);
    profileSeparator->setFrameShape(QFrame::HLine);
    profileSeparator->setFrameShadow(QFrame::Sunken);

    buttonNewProfile = new QPushButton("New", this);
    buttonNewProfile->setMinimumSize(QSize(10, 28));

    buttonLoad = new QPushButton(QIcon(":/icons/file_open"), "", this);
    buttonLoad->setIconSize(QSize(18, 18));
    buttonLoad->setFixedSize(QSize(28, 28));
    buttonLoad->setToolTip("Load profile from file");

    buttonSave = new QPushButton(QIcon(":/icons/save_as"), "", this);
    buttonSave->setIconSize(QSize(18, 18));
    buttonSave->setFixedSize(QSize(28, 28));
    buttonSave->setToolTip("Save profile to file");

    auto profileButtonsLayout = new QHBoxLayout();
    profileButtonsLayout->setContentsMargins(0, 0, 0, 0);
    profileButtonsLayout->setSpacing(4);
    profileButtonsLayout->addWidget(buttonNewProfile, 1);
    profileButtonsLayout->addWidget(buttonLoad);
    profileButtonsLayout->addWidget(buttonSave);

    auto profilesLayout = new QVBoxLayout();
    profilesLayout->setContentsMargins(10, 10, 10, 10);
    profilesLayout->setSpacing(6);
    profilesLayout->addWidget(label_Profiles);
    profilesLayout->addWidget(inputProfileName);
    profilesLayout->addWidget(profileSeparator);
    profilesLayout->addWidget(cardWidget, 1);
    profilesLayout->addLayout(profileButtonsLayout);

    auto profilesPanel = new QWidget(this);
    profilesPanel->setFixedWidth(220);
    profilesPanel->setLayout(profilesLayout);

    cancelButton = new QPushButton("Cancel", this);
    cancelButton->setFixedHeight(30);

    buttonCreate = new QPushButton("Create", this);
    buttonCreate->setDefault(true);
    buttonCreate->setFixedHeight(30);
    buttonCreate->setFixedWidth(120);
    buttonCreate->setFocus();

    auto footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(10, 0, 10, 0);
    footerLayout->setSpacing(8);
    footerLayout->addStretch();
    footerLayout->addWidget(cancelButton);
    footerLayout->addWidget(buttonCreate);

    auto footerWidget = new QWidget(this);
    footerWidget->setObjectName("DialogFooter");
    footerWidget->setFixedHeight(46);
    footerWidget->setStyleSheet("QWidget#DialogFooter { border-top: 1px solid palette(mid); }");
    footerWidget->setLayout(footerLayout);

    auto bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(formWidget, 1);
    bodyLayout->addWidget(separatorLine);
    bodyLayout->addWidget(profilesPanel);

    mainGridLayout = new QVBoxLayout(this);
    mainGridLayout->setContentsMargins(5, 5, 5, 5);
    mainGridLayout->setSpacing(0);
    mainGridLayout->addLayout(bodyLayout, 1);
    mainGridLayout->addWidget(footerWidget);

    this->setMinimumWidth(800);
}

void DialogListener::Start()
{
    this->setModal(true);
    this->show();
}

void DialogListener::AddExListeners(const QList<RegListenerConfig> &listeners, const QMap<QString, AxUI> &uis)
{
    listenerCombobox->clear();
    listenerTypeCombobox->clear();

    this->listeners = listeners;
    this->ax_uis    = uis;

    QSet<QString> listenersSet;

    for (auto listener : listeners) {
        auto ax_ui = &this->ax_uis[listener.name];

        ax_ui->widget->setParent(nullptr);
        ax_ui->widget->setParent(this);
        ax_ui->container->setParent(nullptr);
        ax_ui->container->setParent(this);

        configStackWidget->addWidget(ax_ui->widget);
        listenerCombobox->addItem(listener.name);
        QString type = listener.type + " | " + listener.protocol;
        listenersSet.insert(type);
    }
    QList<QString> sortedTypes(listenersSet.begin(), listenersSet.end());
    std::sort(sortedTypes.begin(), sortedTypes.end());
    listenerTypeCombobox->addItem("any");
    listenerTypeCombobox->addItems(sortedTypes);
}

void DialogListener::SetProfile(const AuthProfile &profile)
{
    this->authProfile = profile;
    loadProfiles();
}

void DialogListener::SetEditMode(const QString &name)
{
    this->setWindowTitle( "Edit Listener" );
    inputListenerName->setText(name);
    inputListenerName->setDisabled(true);
    listenerCombobox->setDisabled(true);
    listenerTypeCombobox->setDisabled(true);
    buttonCreate->setText("Edit");
    editMode = true;

    inputProfileName->setReadOnly(true);
    inputProfileName->setToolTip("Profile name (read-only in edit mode)");
    actionSaveProfile->setToolTip("Click to toggle: Update profile data in database");

    cardWidget->setEnabled(false);
    buttonNewProfile->setEnabled(false);
    buttonSave->setEnabled(false);
    buttonLoad->setEnabled(false);
}

void DialogListener::changeConfig(const QString &fn)
{
    if (ax_uis.contains(fn)) {
        auto ax_ui = &ax_uis[fn];
        if (ax_ui)
            configStackWidget->setCurrentWidget(ax_ui->widget);
        this->resize(ax_ui->width, ax_ui->height);
    }
}

void DialogListener::changeType(const QString &type)
{
    listenerCombobox->clear();
    for (auto listener : listeners) {
        QString listenerType = listener.type + " | " + listener.protocol;
        if (listenerType == type || type == "any")
            listenerCombobox->addItem(listener.name);
    }
}

void DialogListener::onButtonCreate()
{
    auto configName = inputListenerName->text();
    auto configType = listenerCombobox->currentText();
    auto profileName = inputProfileName->text().trimmed();
    bool shouldSaveProfile = actionSaveProfile->isChecked() && !profileName.isEmpty();

    auto configData = QString();
    if (ax_uis.contains(configType) && ax_uis[configType].container)
        configData = ax_uis[configType].container->toJson();

    buttonCreate->setEnabled(false);
    buttonCreate->setText(editMode ? "Editing..." : "Creating...");

    bool isEditMode = editMode;
    QPointer<DialogListener> safeThis = this;

    auto callback = [safeThis, isEditMode, configName, configType, configData, profileName, shouldSaveProfile](bool success, const QString &message, const QJsonObject&) {
        if (!safeThis)
            return;

        if (!success) {
            MessageError(message);
            if (safeThis) {
                safeThis->buttonCreate->setEnabled(true);
                safeThis->buttonCreate->setText(isEditMode ? "Edit" : "Create");
            }
        } else {
            if (shouldSaveProfile) {
                safeThis->saveProfile(profileName, configName, configType, configData);
                safeThis->loadProfiles();
            }
            safeThis->close();
        }
    };

    if (editMode)
        HttpReqListenerEditAsync(configName, configType, configData, authProfile, callback);
    else
        HttpReqListenerStartAsync(configName, configType, configData, authProfile, callback);
}

void DialogListener::saveProfile(const QString &profileName, const QString &configName, const QString &configType, const QString &configData)
{
    QJsonObject dataJson;
    dataJson["name"] = configName;
    dataJson["type"] = configType;
    dataJson["config"] = configData;
    dataJson["timestamp"] = QDateTime::currentDateTime().toString("dd.MM hh:mm");
    QString profileData = QJsonDocument(dataJson).toJson(QJsonDocument::Compact);

    Storage::AddListenerProfile(authProfile.GetProject(), profileName, profileData);
}

void DialogListener::onButtonLoad()
{
    QString baseDir = authProfile.GetProjectDir();
    QPointer<DialogListener> safeThis = this;

    NonBlockingDialogs::getOpenFileName(this, "Select file", baseDir, "JSON files (*.json)",
        [safeThis](const QString& filePath) {
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

            if ( !jsonObject.contains("type") || !jsonObject["type"].isString() ) {
                MessageError("Required parameter 'type' is missing");
                return;
            }
            if ( !jsonObject.contains("config") || !jsonObject["config"].isString() ) {
                MessageError("Required parameter 'config' is missing");
                return;
            }

            if (!safeThis)
                return;

            if( jsonObject.contains("name") && jsonObject["name"].isString())
                safeThis->inputListenerName->setText( jsonObject["name"].toString() );

            QString configType = jsonObject["type"].toString();
            int typeIndex = safeThis->listenerCombobox->findText( configType );

            if(typeIndex == -1 || !safeThis->ax_uis.contains(configType)) {
                MessageError("No such listener exists");
                return;
            }

            QString configData = jsonObject["config"].toString();
            safeThis->listenerCombobox->setCurrentIndex(typeIndex);

            safeThis->ax_uis[configType].container->fromJson(configData);
    });
}

void DialogListener::onButtonSave()
{
    auto configName= inputListenerName->text();
    auto configType= listenerCombobox->currentText();
    auto configData = QString();
    if (ax_uis.contains(configType) && ax_uis[configType].container)
        configData = ax_uis[configType].container->toJson();

    QJsonObject dataJson;
    dataJson["name"]   = configName;
    dataJson["type"]   = configType;
    dataJson["config"] = configData;
    QByteArray fileContent = QJsonDocument(dataJson).toJson();

    QString tmpFilename = configName + "_listener_config.json";
    QString baseDir     = authProfile.GetProjectDir();
    QString initialPath = QDir(baseDir).filePath(tmpFilename);
    QPointer<DialogListener> safeThis = this;
    NonBlockingDialogs::getSaveFileName(this, "Save File", initialPath, "JSON files (*.json)",
        [safeThis, fileContent](const QString& filePath) {
            if (!safeThis) return;
            if (filePath.isEmpty())
                return;

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly)) {
                MessageError("Failed to open file for writing");
                return;
            }

            file.write( fileContent );
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

void DialogListener::onButtonNewProfile()
{
    inputListenerName->clear();
    inputProfileName->clear();
    cardWidget->clearSelection();
    actionSaveProfile->setChecked(true);
    profileNameManuallyEdited = false;
    inputListenerName->setFocus();
}

void DialogListener::loadProfiles()
{
    cardWidget->clear();

    QString project = authProfile.GetProject();
    if (project.isEmpty())
        return;

    QVector<QPair<QString, QString>> profiles = Storage::ListListenerProfiles(project);
    for (const auto& profile : profiles) {
        QString profileName = profile.first;
        QString profileData = profile.second;

        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson(profileData.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            continue;

        QJsonObject jsonObject = document.object();
        QString profileType = jsonObject.contains("type") && jsonObject["type"].isString()
                              ? jsonObject["type"].toString()
                              : "";
        QString timestamp = jsonObject.contains("timestamp") && jsonObject["timestamp"].isString()
                            ? jsonObject["timestamp"].toString()
                            : "";

        QString subtitle = profileType;
        if (!timestamp.isEmpty())
            subtitle = profileType + " | " + timestamp;

        cardWidget->addCard(profileName, subtitle);
    }
}

void DialogListener::onProfileSelected()
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

    QString profileData = Storage::GetListenerProfile(project, profileName);
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

    if (jsonObject.contains("name") && jsonObject["name"].isString())
        inputListenerName->setText(jsonObject["name"].toString());

    if (jsonObject.contains("type") && jsonObject["type"].isString()) {
        QString configType = jsonObject["type"].toString();
        int typeIndex = listenerCombobox->findText(configType);
        if (typeIndex != -1 && ax_uis.contains(configType)) {
            listenerCombobox->setCurrentIndex(typeIndex);
            if (jsonObject.contains("config") && jsonObject["config"].isString()) {
                QString configData = jsonObject["config"].toString();
                ax_uis[configType].container->fromJson(configData);
            }
        }
    }
}

void DialogListener::handleProfileContextMenu(const QPoint &pos)
{
    QPoint globalPos = cardWidget->mapToGlobal(pos);
    menuContext->exec(globalPos);
}

void DialogListener::onProfileRemove()
{
    auto* item = cardWidget->currentItem();
    if (!item)
        return;

    QString profileName = item->data(CardListWidget::TitleRole).toString();
    if (!profileName.isEmpty()) {
        QString project = authProfile.GetProject();
        if (!project.isEmpty()) {
            Storage::RemoveListenerProfile(project, profileName);
        }
    }

    delete cardWidget->takeItem(cardWidget->row(item));
    loadProfiles();
}

void DialogListener::onProfileRename()
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
    QString newName = QInputDialog::getText(this, "Rename Profile", "New profile name:", 
                                             QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.trimmed().isEmpty() || newName == oldName)
        return;

    newName = newName.trimmed();

    QString profileData = Storage::GetListenerProfile(project, oldName);
    if (profileData.isEmpty())
        return;

    Storage::RemoveListenerProfile(project, oldName);
    Storage::AddListenerProfile(project, newName, profileData);
    loadProfiles();

    if (inputProfileName->text() == oldName)
        inputProfileName->setText(newName);
}

void DialogListener::onListenerNameChanged(const QString &text)
{
    if (!profileNameManuallyEdited)
        inputProfileName->setText(text);
}

void DialogListener::onProfileNameEdited(const QString &text)
{
    Q_UNUSED(text);
    profileNameManuallyEdited = true;
}

void DialogListener::onSaveProfileToggled(bool checked)
{
    if (checked)
        actionSaveProfile->setIcon(QIcon(":/icons/check"));
    else
        actionSaveProfile->setIcon(QIcon(":/icons/close"));
}