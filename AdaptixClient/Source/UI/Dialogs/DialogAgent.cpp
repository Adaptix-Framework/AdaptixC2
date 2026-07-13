#include <UI/Dialogs/DialogAgent.h>
#include <Utils/NonBlockingDialogs.h>
#include <Utils/FontManager.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/Storage.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Workers/BuildWorker.h>
#include <QJSEngine>
#include <QWidgetAction>

#include <QLayoutItem>

class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget *parent, int hGap = 4, int vGap = 4) : QLayout(parent), m_hGap(hGap), m_vGap(vGap) {}
    ~FlowLayout() override { while (auto *item = takeAt(0)) delete item; }

    void addItem(QLayoutItem *item) override { m_items.append(item); }
    int count() const override { return m_items.count(); }
    QLayoutItem *itemAt(int index) const override { return m_items.value(index); }
    QLayoutItem *takeAt(int index) override { return (index >= 0 && index < m_items.size()) ? m_items.takeAt(index) : nullptr; }

    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }

    void setGeometry(const QRect &rect) override {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

    QSize sizeHint() const override { return minimumSize(); }
    QSize minimumSize() const override {
        QSize size;
        for (auto *item : m_items)
            size = size.expandedTo(item->minimumSize());
        return size + QSize(contentsMargins().left() + contentsMargins().right(), contentsMargins().top() + contentsMargins().bottom());
    }

private:
    int doLayout(const QRect &rect, bool testOnly) const {
        int x = rect.x(), y = rect.y(), lineHeight = 0;
        for (auto *item : m_items) {
            QSize space = item->sizeHint();
            int nextX = x + space.width() + m_hGap;
            if (nextX - m_hGap > rect.right() && lineHeight > 0) {
                x = rect.x();
                y += lineHeight + m_vGap;
                nextX = x + space.width() + m_hGap;
                lineHeight = 0;
            }
            if (!testOnly)
                item->setGeometry(QRect(QPoint(x, y), space));
            x = nextX;
            lineHeight = qMax(lineHeight, space.height());
        }
        return y + lineHeight - rect.y();
    }

    QList<QLayoutItem *> m_items;
    int m_hGap, m_vGap;
};

void DialogAgent::createUI()
{
    this->setWindowTitle("Generate Agent");
    this->setProperty("Main", "base");

    listenerLabel = new QLabel("Listener:", this);

    listenerSelectBtn = new oclero::qlementine::PopoverButton("", QIcon(":/icons/plus"), this);
    listenerSelectBtn->setToolTip("Add listener");
    listenerSelectBtn->setFixedSize(28, 28);
    listenerSelectBtn->setIconSize(QSize(18, 18));
    listenerSelectBtn->setShowArrowIndicator(false);

    listenerListWidget = new QListWidget();
    listenerListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    listenerListWidget->setDragDropMode(QAbstractItemView::InternalMove);
    listenerListWidget->setDefaultDropAction(Qt::MoveAction);
    listenerListWidget->setMinimumWidth(250);
    listenerListWidget->setMinimumHeight(150);

    btnMoveUp = new QPushButton("↑");
    btnMoveUp->setToolTip("Move selected listener up");
    btnMoveDown = new QPushButton("↓");
    btnMoveDown->setToolTip("Move selected listener down");

    auto btnLayout = new QVBoxLayout();
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(4);
    btnLayout->addWidget(btnMoveUp);
    btnLayout->addWidget(btnMoveDown);
    btnLayout->addStretch();

    auto popupLayout = new QHBoxLayout();
    popupLayout->setContentsMargins(8, 8, 8, 8);
    popupLayout->setSpacing(4);
    popupLayout->addWidget(listenerListWidget);
    popupLayout->addLayout(btnLayout);

    auto* listenerPopupContent = new QWidget();
    listenerPopupContent->setLayout(popupLayout);

    listenerPopover = listenerSelectBtn->popover();
    listenerPopover->setPreferredPosition(oclero::qlementine::Popover::Position::Bottom);
    listenerPopover->setPreferredAlignment(oclero::qlementine::Popover::Alignment::Begin);
    listenerSelectBtn->setPopoverContentWidget(listenerPopupContent);

    listenerChipsContainer = new QWidget(this);
    listenerChipsContainer->setObjectName("ListenerChipsContainer");
    listenerChipsContainer->setMinimumHeight(28);
    listenerChipsContainer->setStyleSheet(
        "QWidget#ListenerChipsContainer {"
        "  background: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "}"
    );
    auto chipsLayout = new FlowLayout(listenerChipsContainer, 4, 4);
    chipsLayout->setContentsMargins(6, 4, 6, 4);

    agentLabel    = new QLabel("Agent:", this);
    agentCombobox = new QComboBox(this);

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
    agentConfigGroupbox->setAlignment(Qt::AlignHCenter);
    agentConfigGroupbox->setLayout(stackGridLayout);

    buildButton = new QPushButton("Generate", this);
    buildButton->setDefault(true);
    buildButton->setFocus();

    menuContext = new oclero::qlementine::Menu(this);
    menuContext->addAction("Rename", this, &DialogAgent::onProfileRename);
    menuContext->addAction("Remove", this, &DialogAgent::onProfileRemove);

    label_Profiles = new QLabel(this);
    label_Profiles->setText("Profiles");
    label_Profiles->setObjectName("ProfilesHeader");
    label_Profiles->setStyleSheet("QLabel#ProfilesHeader { color: palette(placeholderText); font-size: 10px; font-weight: 700; text-transform: uppercase; letter-spacing: 0.5px; }");

    inputProfileName->setPlaceholderText("Profile name...");

    cardWidget = new CardListWidget(this);
    cardWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    cardWidget->addAction(menuContext->menuAction());
    cardWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    cardWidget->setFocusPolicy(Qt::NoFocus);

    auto profileSeparator = new QFrame(this);
    profileSeparator->setFrameShape(QFrame::HLine);
    profileSeparator->setFrameShadow(QFrame::Sunken);

    buttonNewProfile = new QPushButton(this);
    buttonNewProfile->setText("New");
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

    auto leftPanelLayout = new QGridLayout();
    leftPanelLayout->setVerticalSpacing(8);
    leftPanelLayout->setHorizontalSpacing(8);
    leftPanelLayout->setContentsMargins(5, 5, 5, 5);

    auto listenerFieldLayout = new QHBoxLayout();
    listenerFieldLayout->setContentsMargins(0, 0, 0, 0);
    listenerFieldLayout->setSpacing(4);
    listenerFieldLayout->addWidget(listenerChipsContainer, 1);
    listenerFieldLayout->addWidget(listenerSelectBtn);

    leftPanelLayout->addWidget(agentLabel,          0, 0);
    leftPanelLayout->addWidget(agentCombobox,       0, 1);
    leftPanelLayout->addWidget(listenerLabel,       1, 0);
    leftPanelLayout->addLayout(listenerFieldLayout, 1, 1);
    leftPanelLayout->addWidget(agentConfigGroupbox, 2, 0, 1, 2);
    leftPanelLayout->setRowStretch(0, 0);
    leftPanelLayout->setRowStretch(1, 0);
    leftPanelLayout->setRowStretch(2, 1);
    leftPanelLayout->setColumnStretch(0, 0);
    leftPanelLayout->setColumnStretch(1, 1);

    auto formLayout = new QVBoxLayout();
    formLayout->setContentsMargins(10, 10, 10, 10);
    formLayout->setSpacing(8);
    formLayout->addLayout(leftPanelLayout, 1);

    auto formWidget = new QWidget(this);
    formWidget->setLayout(formLayout);

    auto separatorLine = new QFrame(this);
    separatorLine->setFrameShape(QFrame::VLine);
    separatorLine->setFrameShadow(QFrame::Sunken);

    cancelButton = new QPushButton("Cancel", this);
    cancelButton->setFixedHeight(30);

    buildButton->setFixedHeight(30);
    buildButton->setFixedWidth(120);

    collapseButton = new QPushButton(QIcon(":/icons/arrow_right"), " Build Log", this);
    collapseButton->setFlat(true);
    collapseButton->setIconSize(QSize(14, 14));
    collapseButton->setFixedHeight(28);
    collapseButton->setCursor(Qt::PointingHandCursor);
    collapseButton->setStyleSheet("font-size: 11px;");
    {
        QPalette p = collapseButton->palette();
        p.setColor(QPalette::ButtonText, p.color(QPalette::PlaceholderText));
        collapseButton->setPalette(p);
    }

    fileLinkButton = new QPushButton(this);
    fileLinkButton->setFlat(true);
    fileLinkButton->setFixedHeight(28);
    fileLinkButton->setCursor(Qt::PointingHandCursor);
    fileLinkButton->setIcon(QIcon(":/icons/downloads"));
    fileLinkButton->setIconSize(QSize(14, 14));
    fileLinkButton->setStyleSheet("font-size: 11px; font-weight: 500;");
    fileLinkButton->setVisible(false);
    fileLinkButton->setToolTip("Click to save file");
    {
        QPalette p = fileLinkButton->palette();
        p.setColor(QPalette::ButtonText, p.color(QPalette::Highlight));
        fileLinkButton->setPalette(p);
    }
    connect(fileLinkButton, &QPushButton::clicked, this, &DialogAgent::onSaveBuildFile);

    auto footerLeftLayout = new QHBoxLayout();
    footerLeftLayout->setContentsMargins(0, 0, 0, 0);
    footerLeftLayout->setSpacing(8);
    footerLeftLayout->addWidget(collapseButton);
    footerLeftLayout->addWidget(fileLinkButton);

    auto footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(10, 0, 10, 0);
    footerLayout->setSpacing(8);
    footerLayout->addLayout(footerLeftLayout);
    footerLayout->addStretch();
    footerLayout->addWidget(cancelButton);
    footerLayout->addWidget(buildButton);

    auto footerWidget = new QWidget(this);
    footerWidget->setObjectName("DialogFooter");
    footerWidget->setFixedHeight(46);
    footerWidget->setStyleSheet("QWidget#DialogFooter { border-top: 1px solid palette(mid); }");
    footerWidget->setLayout(footerLayout);

    buildLogOutput = new QTextEdit(this);
    buildLogOutput->setReadOnly(true);
    buildLogOutput->setMinimumHeight(140);
    buildLogOutput->setStyleSheet("background-color: #151515; color: #BEBEBE; border: 1px solid #2A2A2A; border-radius: 4px;");
    buildLogOutput->setFont(FontManager::instance().getFont("Hack"));

    buildLogExpander = new oclero::qlementine::Expander(this);
    buildLogExpander->setContent(buildLogOutput);
    buildLogExpander->setExpanded(false);

    connect(collapseButton, &QPushButton::clicked, this, [this]() {
        buildLogExpander->toggleExpanded();
    });
    connect(buildLogExpander, &oclero::qlementine::Expander::expandedChanged, this, [this]() {
        collapseButton->setIcon(QIcon(buildLogExpander->expanded() ? ":/icons/arrow_drop_down" : ":/icons/arrow_right"));
    });

    buildLogPanel = new QWidget(this);
    auto buildLogPanelLayout = new QVBoxLayout(buildLogPanel);
    buildLogPanelLayout->setContentsMargins(0, 0, 0, 0);
    buildLogPanelLayout->setSpacing(0);
    buildLogPanelLayout->addWidget(buildLogExpander, 1);
    buildLogPanel->setVisible(false);

    auto bodyLayout = new QHBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(formWidget, 1);
    bodyLayout->addWidget(separatorLine);
    bodyLayout->addWidget(profilesPanel);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(0);
    mainLayout->addLayout(bodyLayout, 1);
    mainLayout->addWidget(footerWidget);
    mainLayout->addWidget(buildLogPanel);

    this->setLayout(mainLayout);
}

DialogAgent::DialogAgent(AdaptixWidget* adaptixWidget, const QString &listenerName, const QString &listenerType)
{
    this->adaptixWidget = adaptixWidget;
    this->createUI();

    this->listenerName = listenerName;
    this->listenerType = listenerType;

    connect(cardWidget,         &QListWidget::itemPressed,                this, &DialogAgent::onProfileSelected);
    connect(cardWidget,         &QListWidget::customContextMenuRequested, this, &DialogAgent::handleProfileContextMenu);
    connect(agentCombobox,      &QComboBox::currentTextChanged,           this, &DialogAgent::changeConfig);
    connect(buildButton,        &QPushButton::clicked,                    this, &DialogAgent::onButtonBuild);
    connect(cancelButton,       &QPushButton::clicked,                    this, &QDialog::close);
    connect(buttonNewProfile,   &QPushButton::clicked,                    this, &DialogAgent::onButtonNewProfile);
    connect(buttonLoad,         &QPushButton::clicked,                    this, &DialogAgent::onButtonLoad);
    connect(buttonSave,         &QPushButton::clicked,                    this, &DialogAgent::onButtonSave);
    connect(inputProfileName,   &QLineEdit::textEdited,                   this, &DialogAgent::onProfileNameEdited);
    connect(actionSaveProfile,  &QAction::toggled,                        this, &DialogAgent::onSaveProfileToggled);
    connect(listenerListWidget, &QListWidget::itemChanged,                this, &DialogAgent::onListenerSelectionChanged);
    connect(btnMoveUp,          &QPushButton::clicked,                    this, &DialogAgent::onMoveListenerUp);
    connect(btnMoveDown,        &QPushButton::clicked,                    this, &DialogAgent::onMoveListenerDown);
    connect(listenerSelectBtn,  &QPushButton::clicked,                    this, &DialogAgent::showListenerPopup);

    rebuildSingleListenerChip(listenerName);
}

DialogAgent::~DialogAgent()
{
    stopBuild();
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

void DialogAgent::SetAvailableListeners(const QVector<ListenerData> &listeners)
{
    this->availableListeners = listeners;
}

void DialogAgent::SetAgentTypes(const QMap<QString, AgentTypeInfo> &types)
{
    this->agentTypes = types;
}

void DialogAgent::Start()
{
    this->setModal(true);
    this->show();
}

void DialogAgent::onButtonLoad()
{
    QString baseDir = authProfile.GetProjectDir();
    QPointer<DialogAgent> safeThis = this;
    QString currentListenerType = listenerType;

    NonBlockingDialogs::getOpenFileName(this, "Select file", baseDir, "JSON files (*.json)",
        [safeThis, currentListenerType](const QString& filePath) {
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

            if(currentListenerType != jsonObject["listener_type"].toString()) {
                MessageError("Listener type mismatch");
                return;
            }

            if (!safeThis)
                return;

            QString agentType = jsonObject["agent"].toString();
            int typeIndex = safeThis->agentCombobox->findText( agentType );
            if ( typeIndex == -1 ) {
                MessageError("No such agent exists");
                return;
            }
            safeThis->agentCombobox->setCurrentIndex(typeIndex);
            safeThis->changeConfig(agentType);

            QString configData = jsonObject["config"].toString();

            safeThis->ax_uis[agentType].container->fromJson(configData);
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
    QPointer<DialogAgent> safeThis = this;
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
        if (ax_ui) {
            configStackWidget->setCurrentWidget(ax_ui->widget);
            this->resize(ax_ui->width, ax_ui->height);
        }
    }

    AgentTypeInfo typeInfo = agentTypes.value(agentName, AgentTypeInfo{false, QStringList()});
    bool isMultiListeners = typeInfo.multiListeners;
    listenerSelectBtn->setVisible(isMultiListeners);
    listenerLabel->setText(isMultiListeners ? "Listeners:" : "Listener:");

    if (isMultiListeners) {
        QStringList supportedTypes = typeInfo.listenerTypes;

        listenerListWidget->blockSignals(true);
        listenerListWidget->clear();

        for (const auto &listener : availableListeners) {
            if (!supportedTypes.contains(listener.ListenerRegName))
                continue;

            auto *item = new QListWidgetItem(listener.Name);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(listener.Name == listenerName ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, listener.Name);
            listenerListWidget->addItem(item);
        }

        listenerListWidget->blockSignals(false);
        rebuildListenerChips();
    } else {
        rebuildSingleListenerChip(listenerName);
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
                                      ? jsonObject["listener_type"].toString() : "";

        if (profileListenerType != listenerType)
            continue;

        QString profileListener = jsonObject.contains("listener") && jsonObject["listener"].isString()
                                  ? jsonObject["listener"].toString() : "";
        QString timestamp = jsonObject.contains("timestamp") && jsonObject["timestamp"].isString()
                            ? jsonObject["timestamp"].toString() : "";

        QString subtitle = profileListener;
        if (!timestamp.isEmpty())
            subtitle = profileListener + " | " + timestamp;

        cardWidget->addCard(profileName, subtitle);
    }
}

void DialogAgent::saveProfile(const QString &profileName, const QString &agentName, const QString &configData)
{
    QString project = authProfile.GetProject();
    QVector<QPair<QString, QString>> existingProfiles = Storage::ListAgentProfiles(project);

    for (const auto& profile : existingProfiles) {
        QJsonDocument doc = QJsonDocument::fromJson(profile.second.toUtf8());
        QJsonObject obj = doc.object();

        if (obj["listener_type"].toString() == listenerType && obj["agent"].toString() == agentName && obj["config"].toString() == configData)
            return;
    }

    QJsonObject dataJson;
    dataJson["listener_type"] = listenerType;
    dataJson["listener"]      = listenerName;
    dataJson["agent"]         = agentName;
    dataJson["config"]        = configData;
    dataJson["timestamp"]     = QDateTime::currentDateTime().toString("dd.MM hh:mm");
    QString profileData = QJsonDocument(dataJson).toJson(QJsonDocument::Compact);

    Storage::AddAgentProfile(project, profileName, profileData);
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

void DialogAgent::onButtonBuild()
{
    if (buildWorker) {
        stopBuild();
        return;
    }

    QString agentName  = agentCombobox->currentText();
    QString profileName = inputProfileName->text().trimmed();
    bool shouldSaveProfile = actionSaveProfile->isChecked() && !profileName.isEmpty();

    auto configData = QString();
    if (ax_uis.contains(agentName) && ax_uis[agentName].container)
        configData = ax_uis[agentName].container->toJson();

    if (shouldSaveProfile) {
        saveProfile(profileName, agentName, configData);
        loadProfiles();
    }

    QStringList selectedListeners;
    bool isMultiListeners = agentTypes.value(agentName, AgentTypeInfo{false, QStringList()}).multiListeners;
    if (isMultiListeners) {
        for (int i = 0; i < listenerListWidget->count(); ++i) {
            auto *item = listenerListWidget->item(i);
            if (item->checkState() == Qt::Checked) {
                selectedListeners.append(item->data(Qt::UserRole).toString());
            }
        }
        if (selectedListeners.isEmpty()) {
            MessageError("Please select at least one listener");
            return;
        }
    } else {
        selectedListeners.append(listenerName);
    }

    buildLogOutput->clear();
    buildFileName.clear();
    buildFileContent.clear();
    fileLinkButton->setVisible(false);
    buildLogPanel->setVisible(true);
    buildLogExpander->setExpanded(true);
    collapseButton->setIcon(QIcon(":/icons/arrow_drop_down"));

    buildButton->setText("Stop");

    QString urlTemplate = "wss://%1:%2%3/channel";
    QString sUrl = urlTemplate.arg(authProfile.GetHost()).arg(authProfile.GetPort()).arg(authProfile.GetEndpoint());

    QJsonArray listenersArray;
    for (const QString &listener : selectedListeners)
        listenersArray.append(listener);

    QJsonObject otpData;
    otpData["agent_name"] = agentName;
    otpData["listeners_name"] = listenersArray;

    QString otp;
    bool ok = false;
    if (!HttpReqGetOTP("channel_agent_build", otpData, authProfile, &otp, &ok) || !ok) {
        buildButton->setText("Build");
        MessageError("Failed to generate OTP for build");
        return;
    }

    buildThread = new QThread;
    buildWorker = new BuildWorker(otp, sUrl, configData);
    buildWorker->moveToThread(buildThread);

    connect(buildThread, &QThread::started,              buildWorker, &BuildWorker::start);
    connect(buildWorker, &BuildWorker::finished,         buildThread, &QThread::quit);
    connect(buildWorker, &BuildWorker::finished,         buildWorker, &BuildWorker::deleteLater);
    connect(buildThread, &QThread::finished,             buildThread, &QThread::deleteLater);

    connect(buildWorker, &BuildWorker::connected,            this, &DialogAgent::onBuildConnected,  Qt::QueuedConnection);
    connect(buildWorker, &BuildWorker::textMessageReceived,  this, &DialogAgent::onBuildMessage,    Qt::QueuedConnection);
    connect(buildWorker, &BuildWorker::finished,             this, &DialogAgent::onBuildFinished,   Qt::QueuedConnection);

    buildThread->start();
}

void DialogAgent::onBuildConnected()
{
    buildLogOutput->append("----- Build process start -----");
}

void DialogAgent::onBuildMessage(const QString &msg)
{
    if (msg.isEmpty())
        return;

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(msg.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        buildLogOutput->append(msg.toHtmlEscaped());
        return;
    }

    QJsonObject obj = doc.object();
    int status = obj.value("status").toInt(0);
    QString message = obj.value("message").toString();

    if (status != 4 && message.isEmpty())
        return;

    QString htmlMsg = message.toHtmlEscaped();
    htmlMsg.replace("\\n", "<br>");

    switch (status) {
        case 0: // BUILD_LOG_NONE
            buildLogOutput->append(htmlMsg);
            break;
        case 1: // BUILD_LOG_INFO
            buildLogOutput->append(QString("<span style='color: #569cd6;'>[*]</span> %1").arg(htmlMsg));
            break;
        case 2: // BUILD_LOG_ERROR
            buildLogOutput->append(QString("<span style='color: #f14c4c;'>[-]</span> %1").arg(htmlMsg));
            break;
        case 3: // BUILD_LOG_SUCCESS
            buildLogOutput->append(QString("<span style='color: #dcdcaa;'>[+]</span> %1").arg(htmlMsg));
            break;
        case 4: { // BUILD_LOG_SAVE_FILE
            QString filename = obj.value("filename").toString();
            QString contentBase64 = obj.value("content").toString();
            QByteArray content = QByteArray::fromBase64(contentBase64.toUtf8());

            if (filename.isEmpty() || content.isEmpty())
                return;

            buildFileName = filename;
            buildFileContent = content;

            fileLinkButton->setText(filename);
            fileLinkButton->setVisible(true);

            buildLogOutput->append(QString("<span style='color: #dcdcaa;'>[+]</span> File ready: %1").arg(filename.toHtmlEscaped()));

            QString baseDir = authProfile.GetProjectDir();
            QString initialPath = QDir(baseDir).filePath(filename);

            QPointer<DialogAgent> safeThis = this;
            NonBlockingDialogs::getSaveFileName(this, "Save File", initialPath, "All Files (*.*)",
                [safeThis, content](const QString& filePath) {
                    if (filePath.isEmpty())
                        return;

                    QFile file(filePath);
                    if (!file.open(QIODevice::WriteOnly)) {
                        MessageError("Failed to open file for writing");
                        return;
                    }

                    file.write(content);
                    file.close();

                    if (safeThis) {
                        safeThis->buildLogOutput->append(QString("<span style='color: #dcdcaa;'>[+]</span> File saved: %1").arg(filePath.toHtmlEscaped()));
                    }
                });
            return;
        }
        default:
            break;
    }

    QScrollBar *scrollBar = buildLogOutput->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void DialogAgent::onBuildFinished()
{
    buildLogOutput->append("----- Build process finished -----");

    buildButton->setText("Build");

    buildWorker = nullptr;
    buildThread = nullptr;
}

void DialogAgent::onSaveBuildFile()
{
    if (buildFileName.isEmpty() || buildFileContent.isEmpty())
        return;

    QString baseDir = authProfile.GetProjectDir();
    QString initialPath = QDir(baseDir).filePath(buildFileName);

    QPointer<DialogAgent> safeThis = this;
    QByteArray content = buildFileContent;
    NonBlockingDialogs::getSaveFileName(this, "Save File", initialPath, "All Files (*.*)",
        [safeThis, content](const QString& filePath) {
            if (filePath.isEmpty())
                return;

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly)) {
                MessageError("Failed to open file for writing");
                return;
            }

            file.write(content);
            file.close();

            if (safeThis) {
                safeThis->buildLogOutput->append(QString("<span style='color: #dcdcaa;'>[+]</span> File saved: %1").arg(filePath.toHtmlEscaped()));
            }
        });
}

void DialogAgent::stopBuild()
{
    if (!buildWorker || !buildThread)
        return;

    auto worker = buildWorker;
    auto thread = buildThread;

    buildWorker = nullptr;
    buildThread = nullptr;

    QMetaObject::invokeMethod(worker, "stop", Qt::QueuedConnection);

    connect(thread, &QThread::finished, thread, [thread, worker]() {
        worker->deleteLater();
        thread->deleteLater();
    });
}

void DialogAgent::onListenerSelectionChanged(const QListWidgetItem *item)
{
    Q_UNUSED(item);

    updateListenerDisplay();

    QString agentName = agentCombobox->currentText();
    if (agentName.isEmpty() || !agentTypes.value(agentName, AgentTypeInfo{false, QStringList()}).multiListeners)
        return;

    QStringList selectedListeners;
    for (int i = 0; i < listenerListWidget->count(); ++i) {
        auto *listItem = listenerListWidget->item(i);
        if (listItem->checkState() == Qt::Checked) {
            selectedListeners.append(listItem->data(Qt::UserRole).toString());
        }
    }

    regenerateAgentUI(agentName, selectedListeners);
}

void DialogAgent::onMoveListenerUp()
{
    int currentRow = listenerListWidget->currentRow();
    if (currentRow <= 0)
        return;

    listenerListWidget->blockSignals(true);
    QListWidgetItem *item = listenerListWidget->takeItem(currentRow);
    listenerListWidget->insertItem(currentRow - 1, item);
    listenerListWidget->setCurrentRow(currentRow - 1);
    listenerListWidget->blockSignals(false);

    updateListenerDisplay();

    QString agentName = agentCombobox->currentText();
    if (!agentName.isEmpty()) {
        QStringList selectedListeners;
        for (int i = 0; i < listenerListWidget->count(); ++i) {
            auto *listItem = listenerListWidget->item(i);
            if (listItem->checkState() == Qt::Checked) {
                selectedListeners.append(listItem->data(Qt::UserRole).toString());
            }
        }
        regenerateAgentUI(agentName, selectedListeners);
    }
}

void DialogAgent::onMoveListenerDown()
{
    int currentRow = listenerListWidget->currentRow();
    if (currentRow < 0 || currentRow >= listenerListWidget->count() - 1)
        return;

    listenerListWidget->blockSignals(true);
    QListWidgetItem *item = listenerListWidget->takeItem(currentRow);
    listenerListWidget->insertItem(currentRow + 1, item);
    listenerListWidget->setCurrentRow(currentRow + 1);
    listenerListWidget->blockSignals(false);

    updateListenerDisplay();

    QString agentName = agentCombobox->currentText();
    if (!agentName.isEmpty()) {
        QStringList selectedListeners;
        for (int i = 0; i < listenerListWidget->count(); ++i) {
            auto *listItem = listenerListWidget->item(i);
            if (listItem->checkState() == Qt::Checked) {
                selectedListeners.append(listItem->data(Qt::UserRole).toString());
            }
        }
        regenerateAgentUI(agentName, selectedListeners);
    }
}

void DialogAgent::regenerateAgentUI(const QString &agentName, const QStringList &selectedListeners)
{
    if (!adaptixWidget || agentName.isEmpty())
        return;

    auto engine = adaptixWidget->ScriptManager->AgentScriptEngine(agentName);
    if (engine == nullptr)
        return;

    QJSValue func = engine->globalObject().property("GenerateUI");
    if (!func.isCallable())
        return;

    QJSValue jsListeners = engine->newArray(selectedListeners.size());
    for (int i = 0; i < selectedListeners.size(); ++i) {
        jsListeners.setProperty(i, selectedListeners[i]);
    }

    QJSValueList args;
    args << jsListeners;
    QJSValue result = func.call(args);
    if (result.isError()) {
        QString error = QStringLiteral("%1\n  at line %2 in %3\n  stack: %4")
            .arg(result.toString())
            .arg(result.property("lineNumber").toInt())
            .arg(agentName)
            .arg(result.property("stack").toString());
        adaptixWidget->ScriptManager->consolePrintError(error);
        return;
    }

    if (!result.isObject())
        return;

    QJSValue ui_container = result.property("ui_container");
    QJSValue ui_panel     = result.property("ui_panel");
    QJSValue ui_height    = result.property("ui_height");
    QJSValue ui_width     = result.property("ui_width");

    if (ui_container.isUndefined() || !ui_container.isObject() || ui_panel.isUndefined() || !ui_panel.isQObject())
        return;

    QObject* objPanel = ui_panel.toQObject();
    auto* formElement = dynamic_cast<AxPanelWrapper*>(objPanel);
    if (!formElement)
        return;

    QObject* objContainer = ui_container.toQObject();
    auto* container = dynamic_cast<AxContainerWrapper*>(objContainer);
    if (!container)
        return;

    int h = 550;
    if (ui_height.isNumber() && ui_height.toInt() > 0)
        h = ui_height.toInt();

    int w = 550;
    if (ui_width.isNumber() && ui_width.toInt() > 0)
        w = ui_width.toInt();

    if (ax_uis.contains(agentName)) {
        auto &oldUi = ax_uis[agentName];
        if (oldUi.widget) {
            configStackWidget->removeWidget(oldUi.widget);
            oldUi.widget->deleteLater();
        }
    }

    ax_uis[agentName] = { container, formElement->widget(), h, w };
    configStackWidget->addWidget(formElement->widget());
    configStackWidget->setCurrentWidget(formElement->widget());
    this->resize(w, h);
}

void DialogAgent::showListenerPopup()
{
    listenerSelectBtn->setPopoverOpened(true);
}

void DialogAgent::updateListenerDisplay()
{
    rebuildListenerChips();
}

void DialogAgent::rebuildListenerChips()
{
    QLayout *layout = listenerChipsContainer->layout();
    while (layout->count() > 0) {
        QLayoutItem *item = layout->takeAt(0);
        if (item->widget())
            delete item->widget();
        delete item;
    }

    for (int i = 0; i < listenerListWidget->count(); ++i) {
        auto *item = listenerListWidget->item(i);
        if (item->checkState() != Qt::Checked)
            continue;

        QString name = item->text();
        int idx = i;

        auto *chip = new QWidget(listenerChipsContainer);
        chip->setObjectName("ListenerChip");
        chip->setFixedHeight(24);
        chip->setStyleSheet(
            "QWidget#ListenerChip {"
            "  background: transparent;"
            "  border: 1px solid palette(highlight);"
            "  border-radius: 4px;"
            "}"
        );

        auto *chipLabel = new QLabel(QString(" %1 ").arg(name), chip);
        chipLabel->setStyleSheet("color: palette(highlight); font-size: 13px; font-weight: 500; background: transparent; border: none;");

        auto *chipBtn = new QPushButton("×", chip);
        chipBtn->setObjectName("ChipRemoveBtn");
        chipBtn->setFixedSize(20, 20);
        chipBtn->setCursor(Qt::PointingHandCursor);
        chipBtn->setStyleSheet(
            "QPushButton#ChipRemoveBtn {"
            "  background: transparent; border: none;"
            "  color: palette(highlight); font-size: 12px; font-weight: bold;"
            "}"
            "QPushButton#ChipRemoveBtn:hover { color: palette(light); }"
        );
        connect(chipBtn, &QPushButton::clicked, this, [this, idx]() {
            auto *listItem = listenerListWidget->item(idx);
            if (listItem)
                listItem->setCheckState(Qt::Unchecked);
        });

        auto *chipLayout = new QHBoxLayout(chip);
        chipLayout->setContentsMargins(6, 0, 4, 0);
        chipLayout->setSpacing(2);
        chipLayout->setAlignment(Qt::AlignVCenter);
        chipLayout->addWidget(chipLabel);
        chipLayout->addWidget(chipBtn);

        layout->addWidget(chip);
    }
}

void DialogAgent::rebuildSingleListenerChip(const QString &name)
{
    QLayout *layout = listenerChipsContainer->layout();
    while (layout->count() > 0) {
        QLayoutItem *item = layout->takeAt(0);
        if (item->widget())
            delete item->widget();
        delete item;
    }

    auto *chip = new QWidget(listenerChipsContainer);
    chip->setObjectName("ListenerChip");
    chip->setFixedHeight(24);
    chip->setStyleSheet(
        "QWidget#ListenerChip {"
        "  background: transparent;"
        "  border: 1px solid palette(highlight);"
        "  border-radius: 4px;"
        "}"
    );

    auto *chipLabel = new QLabel(QString(" %1 ").arg(name), chip);
    chipLabel->setStyleSheet("color: palette(highlight); font-size: 13px; font-weight: 500; background: transparent; border: none;");

    auto *chipLayout = new QHBoxLayout(chip);
    chipLayout->setContentsMargins(6, 0, 6, 0);
    chipLayout->setSpacing(0);
    chipLayout->setAlignment(Qt::AlignVCenter);
    chipLayout->addWidget(chipLabel);

    layout->addWidget(chip);
}