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
#include <QScreen>
#include <QLayoutItem>
#include <QSizePolicy>
#include <QButtonGroup>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QSet>
#include <QDateTime>

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

    listenerLabel = new QLabel(QStringLiteral("Listener name:"), this);

    const int ctrlH = FontManager::instance().typography().controlHeight;

    listenerSelectBtn = new oclero::qlementine::PopoverButton("", QIcon(":/icons/plus"), this);
    listenerSelectBtn->setToolTip("Add listener");
    listenerSelectBtn->setFixedSize(ctrlH, ctrlH);
    listenerSelectBtn->setIconSize(QSize(qMax(14, ctrlH - 10), qMax(14, ctrlH - 10)));
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
    listenerChipsContainer->setFixedHeight(ctrlH);
    listenerChipsContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    listenerChipsContainer->setStyleSheet(
        "QWidget#ListenerChipsContainer {"
        "  background: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "}"
    );
    auto chipsLayout = new FlowLayout(listenerChipsContainer, 4, 2);
    chipsLayout->setContentsMargins(4, 2, 4, 2);

    listenerMultiField = new QWidget(this);
    auto multiLay = new QHBoxLayout(listenerMultiField);
    multiLay->setContentsMargins(0, 0, 0, 0);
    multiLay->setSpacing(4);
    multiLay->addWidget(listenerChipsContainer, 1);
    multiLay->addWidget(listenerSelectBtn, 0);

    listenerCombobox = new QComboBox(this);
    listenerCombobox->setFixedHeight(ctrlH);
    listenerCombobox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    listenerCombobox->setVisible(false);

    agentLabel    = new QLabel(QStringLiteral("Agent type:"), this);
    agentCombobox = new QComboBox(this);
    agentCombobox->setFixedHeight(ctrlH);

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

    storeCheck = new QCheckBox(QStringLiteral("Save"), this);
    storeCheck->setChecked(true);
    storeCheck->setToolTip(QStringLiteral("Register the built artifact in the teamserver Payload Store"));
    storeCheck->setFixedHeight(ctrlH);

    inputDescription = new QLineEdit(this);
    inputDescription->setFixedHeight(ctrlH);
    inputDescription->setPlaceholderText(QStringLiteral("Description for Payload Store"));
    inputDescription->setToolTip(QStringLiteral("Stored as payload notes when Save is checked"));
    inputDescription->setEnabled(true);

    menuContext = new oclero::qlementine::Menu(this);
    menuContext->addAction(QIcon(":/icons/edit_note"), "Rename", this, &DialogAgent::onProfileRename);
    menuContext->addAction(QIcon(":/icons/delete"), "Remove", this, &DialogAgent::onProfileRemove);

    label_Profiles = new QLabel(this);
    label_Profiles->setText(QStringLiteral("PROFILES (client)"));
    label_Profiles->setObjectName("ProfilesHeader");
    label_Profiles->setStyleSheet(QStringLiteral(
        "QLabel#ProfilesHeader { color: palette(placeholderText); font-size: %1px; font-weight: 700; letter-spacing: 0.5px; }"
    ).arg(FontManager::instance().typography().captionFontPx));

    inputProfileName->setPlaceholderText("Profile name...");

    cardWidget = new CardListWidget(this);
    cardWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    cardWidget->addAction(menuContext->menuAction());
    cardWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    cardWidget->setFocusPolicy(Qt::ClickFocus);

    auto profileSeparator = new QFrame(this);
    profileSeparator->setFrameShape(QFrame::HLine);
    profileSeparator->setFrameShadow(QFrame::Sunken);

    buttonNewProfile = new QPushButton(QIcon(":/icons/plus"), "", this);
    buttonNewProfile->setIconSize(QSize(18, 18));
    buttonNewProfile->setFixedSize(QSize(28, 28));
    buttonNewProfile->setToolTip("New profile");

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
    profileButtonsLayout->addStretch(1);
    profileButtonsLayout->addWidget(buttonNewProfile);
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
    listenerFieldLayout->setSpacing(0);
    listenerFieldLayout->addWidget(listenerMultiField, 1);
    listenerFieldLayout->addWidget(listenerCombobox, 1);

    const AppTypography& ty = FontManager::instance().typography();

    auto* viewToggleGroup = new QFrame(this);
    viewToggleGroup->setObjectName(QStringLiteral("AgentViewSegment"));
    auto* toggleLay = new QHBoxLayout(viewToggleGroup);
    toggleLay->setContentsMargins(3, 3, 3, 3);
    toggleLay->setSpacing(2);
    viewToggleGroup->setStyleSheet(QStringLiteral(
        "QFrame#AgentViewSegment {"
        "  background: palette(mid);"
        "  border: 1px solid palette(dark);"
        "  border-radius: 6px;"
        "}"
    ));

    auto makeViewBtn = [this, ctrlH](const QString& text) {
        auto* btn = new QPushButton(text, this);
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(qMax(26, ctrlH - 4));
        btn->setMinimumWidth(88);
        btn->setObjectName(QStringLiteral("AgentViewSegBtn"));
        return btn;
    };
    configViewBtn = makeViewBtn(QStringLiteral("Config"));
    logViewBtn    = makeViewBtn(QStringLiteral("Build Log"));
    configViewBtn->setChecked(true);
    const QString segBtnCss = QStringLiteral(
        "QPushButton#AgentViewSegBtn {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 4px;"
        "  color: palette(text);"
        "  font-weight: 600;"
        "  padding: 2px 12px;"
        "}"
        "QPushButton#AgentViewSegBtn:hover {"
        "  background: palette(alternate-base);"
        "}"
        "QPushButton#AgentViewSegBtn:checked {"
        "  background: palette(base);"
        "  color: palette(highlight);"
        "}"
    );
    configViewBtn->setStyleSheet(segBtnCss);
    logViewBtn->setStyleSheet(segBtnCss);
    toggleLay->addWidget(configViewBtn);
    toggleLay->addWidget(logViewBtn);

    viewButtonGroup = new QButtonGroup(this);
    viewButtonGroup->setExclusive(true);
    viewButtonGroup->addButton(configViewBtn, 0);
    viewButtonGroup->addButton(logViewBtn, 1);

    fileChipButton = new QPushButton(this);
    fileChipButton->setObjectName(QStringLiteral("PayloadFileChip"));
    fileChipButton->setCursor(Qt::PointingHandCursor);
    fileChipButton->setFixedHeight(28);
    fileChipButton->setIcon(QIcon(QStringLiteral(":/icons/downloads")));
    fileChipButton->setIconSize(QSize(14, 14));
    fileChipButton->setVisible(false);
    fileChipButton->setToolTip(QStringLiteral("Save built payload to disk"));
    fileChipButton->setStyleSheet(QStringLiteral(
        "QPushButton#PayloadFileChip {"
        "  background: palette(base);"
        "  border: 1px solid palette(highlight);"
        "  border-radius: 4px;"
        "  color: palette(highlight);"
        "  font-size: %1px;"
        "  font-weight: 600;"
        "  padding: 0 12px 0 10px;"
        "  text-align: left;"
        "}"
        "QPushButton#PayloadFileChip:hover {"
        "  background: palette(alternate-base);"
        "}"
        "QPushButton#PayloadFileChip:pressed {"
        "  background: palette(mid);"
        "}"
    ).arg(ty.chipFontPx));
    connect(fileChipButton, &QPushButton::clicked, this, &DialogAgent::onSaveBuildFile);

    buildLogOutput = new QTextEdit(this);
    buildLogOutput->setReadOnly(true);
    buildLogOutput->setLineWrapMode(QTextEdit::NoWrap);
    buildLogOutput->setFont(FontManager::instance().appMonoFont());
    buildLogOutput->setFrameShape(QFrame::NoFrame);
    buildLogOutput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    {
        QPalette pal = buildLogOutput->palette();
        const QColor base = pal.color(QPalette::Base).darker(115);
        const QColor text = pal.color(QPalette::Text);
        pal.setColor(QPalette::Base, base);
        pal.setColor(QPalette::Text, text);
        buildLogOutput->setPalette(pal);
        buildLogOutput->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  background: palette(base);"
            "  color: palette(text);"
            "  border: 1px solid palette(mid);"
            "  border-radius: 4px;"
            "  padding: 6px;"
            "}"
        ));
    }

    buildLogPage = new QWidget(this);
    auto* logPageLay = new QVBoxLayout(buildLogPage);
    logPageLay->setContentsMargins(0, 0, 0, 0);
    logPageLay->setSpacing(0);
    logPageLay->addWidget(buildLogOutput, 1);

    leftContentStack = new QStackedWidget(this);
    leftContentStack->addWidget(agentConfigGroupbox); // 0 = Config
    leftContentStack->addWidget(buildLogPage);        // 1 = Build Log
    leftContentStack->setCurrentIndex(0);

    agentLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    listenerLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    {
        const int labelW = qMax(agentLabel->sizeHint().width(), qMax(listenerLabel->sizeHint().width(), storeCheck->sizeHint().width()));
        agentLabel->setFixedWidth(labelW);
        listenerLabel->setFixedWidth(labelW);
        storeCheck->setMinimumWidth(labelW);
    }

    leftPanelLayout->addWidget(agentLabel,          0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    leftPanelLayout->addWidget(agentCombobox,       0, 1);
    leftPanelLayout->addWidget(listenerLabel,       1, 0, Qt::AlignLeft | Qt::AlignVCenter);
    leftPanelLayout->addLayout(listenerFieldLayout, 1, 1);
    leftPanelLayout->addWidget(storeCheck,          2, 0, Qt::AlignLeft | Qt::AlignVCenter);
    leftPanelLayout->addWidget(inputDescription,    2, 1);
    leftPanelLayout->addWidget(leftContentStack,    3, 0, 1, 2);
    leftPanelLayout->setRowStretch(0, 0);
    leftPanelLayout->setRowStretch(1, 0);
    leftPanelLayout->setRowStretch(2, 0);
    leftPanelLayout->setRowStretch(3, 1);
    leftPanelLayout->setColumnStretch(0, 0);
    leftPanelLayout->setColumnStretch(1, 1);
    leftPanelLayout->setColumnMinimumWidth(0, agentLabel->width());

    connect(viewButtonGroup, &QButtonGroup::idClicked, this, [this](int id) {
        if (leftContentStack)
            leftContentStack->setCurrentIndex(id);
    });

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
    cancelButton->setFixedWidth(120);

    buildButton->setFixedHeight(30);
    buildButton->setFixedWidth(120);

    auto footerLayout = new QHBoxLayout();
    footerLayout->setContentsMargins(12, 0, 10, 0);
    footerLayout->setSpacing(8);
    footerLayout->addWidget(viewToggleGroup, 0, Qt::AlignVCenter);
    footerLayout->addSpacing(6);
    footerLayout->addWidget(fileChipButton, 0, Qt::AlignVCenter);
    footerLayout->addStretch(1);
    footerLayout->addWidget(cancelButton);
    footerLayout->addWidget(buildButton);

    auto footerWidget = new QWidget(this);
    footerWidget->setObjectName("DialogFooter");
    footerWidget->setFixedHeight(50);
    footerWidget->setStyleSheet("QWidget#DialogFooter { border-top: 1px solid palette(mid); }");
    footerWidget->setLayout(footerLayout);

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
    mainLayout->addWidget(footerWidget, 0);

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
    connect(listenerCombobox,   QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DialogAgent::onListenerComboChanged);
    connect(storeCheck,         &QCheckBox::toggled,                      this, &DialogAgent::onStoreCheckToggled);
    onStoreCheckToggled(storeCheck->isChecked());
}

DialogAgent::~DialogAgent()
{
    stopBuild();
}

void DialogAgent::AddExAgents(const QStringList &agents, const QMap<QString, AxUI> &uis)
{
    agentCombobox->blockSignals(true);
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
    agentCombobox->blockSignals(false);

    if (!agents.isEmpty()) {
        agentCombobox->setCurrentIndex(0);
        changeConfig(agents.first());
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
    if (agentCombobox && !agentCombobox->currentText().isEmpty())
        changeConfig(agentCombobox->currentText());
    else
        loadProfiles();
}

void DialogAgent::SetAgentTypes(const QMap<QString, AgentTypeInfo> &types)
{
    this->agentTypes = types;
}

void DialogAgent::Start()
{
    this->setModal(true);
    this->show();
    const QString agentName = agentCombobox ? agentCombobox->currentText() : QString();
    if (!agentName.isEmpty() && ax_uis.contains(agentName)) {
        const AxUI& ui = ax_uis[agentName];
        packDialogSize(ui.width, ui.height);
    }
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
            if (ax_ui->widget) {
                ax_ui->widget->setMinimumWidth(ax_ui->width > 0 ? ax_ui->width : 0);
                ax_ui->widget->adjustSize();
            }
            configStackWidget->updateGeometry();
            if (agentConfigGroupbox)
                agentConfigGroupbox->updateGeometry();
            this->updateGeometry();
            packDialogSize(ax_ui->width, ax_ui->height);
        }
    }

    AgentTypeInfo typeInfo = agentTypes.value(agentName, AgentTypeInfo{false, QStringList()});
    const bool isMultiListeners = typeInfo.multiListeners;
    const QStringList supportedTypes = typeInfo.listenerTypes;

    listenerLabel->setText(isMultiListeners ? QStringLiteral("Listener name:") : QStringLiteral("Listener name:"));
    btnMoveUp->setVisible(isMultiListeners);
    btnMoveDown->setVisible(isMultiListeners);

    if (listenerMultiField)
        listenerMultiField->setVisible(isMultiListeners);
    if (listenerCombobox)
        listenerCombobox->setVisible(!isMultiListeners);

    QString preferredName = listenerName;
    bool preferredOk = false;
    QString firstCompatibleName;
    QString firstCompatibleType;

    for (const auto &listener : availableListeners) {
        if (!supportedTypes.contains(listener.ListenerRegName))
            continue;
        if (firstCompatibleName.isEmpty()) {
            firstCompatibleName = listener.Name;
            firstCompatibleType = listener.ListenerRegName;
        }
        if (listener.Name == listenerName)
            preferredOk = true;
    }
    if (!preferredOk || preferredName.isEmpty()) {
        preferredName = firstCompatibleName;
        if (!firstCompatibleType.isEmpty())
            listenerType = firstCompatibleType;
    }

    if (isMultiListeners) {
        listenerListWidget->blockSignals(true);
        listenerListWidget->clear();

        for (const auto &listener : availableListeners) {
            if (!supportedTypes.contains(listener.ListenerRegName))
                continue;
            auto *item = new QListWidgetItem(listener.Name);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setData(Qt::UserRole, listener.Name);
            item->setData(Qt::UserRole + 1, listener.ListenerRegName);
            item->setCheckState(listener.Name == preferredName ? Qt::Checked : Qt::Unchecked);
            listenerListWidget->addItem(item);
        }
        if (preferredOk || !preferredName.isEmpty())
            listenerName = preferredName;

        listenerListWidget->blockSignals(false);
        rebuildListenerChips();
    } else if (listenerCombobox) {
        listenerCombobox->blockSignals(true);
        listenerCombobox->clear();
        int preferredIdx = -1;
        for (const auto &listener : availableListeners) {
            if (!supportedTypes.contains(listener.ListenerRegName))
                continue;
            const int idx = listenerCombobox->count();
            listenerCombobox->addItem(listener.Name, listener.ListenerRegName);
            if (listener.Name == preferredName)
                preferredIdx = idx;
        }
        if (listenerCombobox->count() > 0) {
            if (preferredIdx < 0)
                preferredIdx = 0;
            listenerCombobox->setCurrentIndex(preferredIdx);
            listenerName = listenerCombobox->currentText();
            listenerType = listenerCombobox->currentData().toString();
        } else {
            listenerName.clear();
            listenerType.clear();
        }
        listenerCombobox->blockSignals(false);
    }

    QString baseName = agentName;
    if (!profileNameManuallyEdited)
        inputProfileName->setText(generateUniqueProfileName(baseName));

    loadProfiles();
}

void DialogAgent::loadProfiles()
{
    if (!cardWidget)
        return;
    cardWidget->clear();

    QString project = authProfile.GetProject();
    if (project.isEmpty())
        return;

    const QString currentAgent = agentCombobox ? agentCombobox->currentText() : QString();

    QVector<QPair<QString, QString>> profiles = Storage::ListAgentProfiles(project);
    for (const auto& profile : profiles) {
        QString profileName = profile.first;
        QString profileData = profile.second;

        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson(profileData.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            continue;

        QJsonObject jsonObject = document.object();

        const QString profileAgent = jsonObject.value(QStringLiteral("agent")).toString();
        const QString profileListenerType = jsonObject.value(QStringLiteral("listener_type")).toString();

        if (!profileAgent.isEmpty()) {
            if (!currentAgent.isEmpty() && profileAgent != currentAgent)
                continue;
        } else if (!listenerType.isEmpty() && !profileListenerType.isEmpty()
                   && profileListenerType != listenerType) {
            continue;
        }

        QString profileListener = jsonObject.value(QStringLiteral("listener")).toString();
        QString timestamp = jsonObject.value(QStringLiteral("timestamp")).toString();

        QString subtitle = profileListener;
        if (!timestamp.isEmpty()) {
            if (subtitle.isEmpty())
                subtitle = timestamp;
            else
                subtitle = profileListener + QStringLiteral(" | ") + timestamp;
        }

        cardWidget->addCard(profileName, subtitle);
    }
}

void DialogAgent::saveProfile(const QString &profileName, const QString &agentName, const QString &configData)
{
    QString project = authProfile.GetProject();
    if (project.isEmpty() || profileName.trimmed().isEmpty())
        return;

    QString activeListener = listenerName;
    QString activeListenerType = listenerType;
    if (listenerCombobox && listenerCombobox->isVisible() && listenerCombobox->currentIndex() >= 0) {
        activeListener = listenerCombobox->currentText();
        activeListenerType = listenerCombobox->currentData().toString();
    }

    QJsonObject dataJson;
    dataJson[QStringLiteral("listener_type")] = activeListenerType;
    dataJson[QStringLiteral("listener")]      = activeListener;
    dataJson[QStringLiteral("agent")]         = agentName;
    dataJson[QStringLiteral("config")]        = configData;
    dataJson[QStringLiteral("timestamp")]     = QDateTime::currentDateTime().toString(QStringLiteral("dd.MM hh:mm"));
    QString profileData = QJsonDocument(dataJson).toJson(QJsonDocument::Compact);

    Storage::AddAgentProfile(project, profileName.trimmed(), profileData);
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
    auto* item = cardWidget->itemAt(pos);
    if (item && !item->isSelected()) {
        cardWidget->clearSelection();
        item->setSelected(true);
        cardWidget->setCurrentItem(item);
    }
    menuContext->exec(cardWidget->mapToGlobal(pos));
}

void DialogAgent::onProfileRemove()
{
    const QList<QListWidgetItem*> selected = cardWidget->selectedItems();
    if (selected.isEmpty())
        return;

    const QString project = authProfile.GetProject();
    for (auto* item : selected) {
        const QString profileName = item->data(CardListWidget::TitleRole).toString();
        if (!profileName.isEmpty() && !project.isEmpty())
            Storage::RemoveAgentProfile(project, profileName);
    }
    loadProfiles();
}

void DialogAgent::onProfileRename()
{
    if (cardWidget->selectedItems().size() > 1) {
        MessageError(QStringLiteral("Select a single profile to rename"));
        return;
    }
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

void DialogAgent::onStoreCheckToggled(bool checked)
{
    if (inputDescription) {
        inputDescription->setEnabled(checked);
        if (!checked)
            inputDescription->clear();
    }
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

    QStringList selectedListeners;
    const bool isMultiListeners = agentTypes.value(agentName, AgentTypeInfo{false, QStringList()}).multiListeners;
    if (isMultiListeners) {
        for (int i = 0; i < listenerListWidget->count(); ++i) {
            auto *item = listenerListWidget->item(i);
            if (item->checkState() == Qt::Checked)
                selectedListeners.append(item->data(Qt::UserRole).toString());
        }
        if (!selectedListeners.isEmpty()) {
            listenerName = selectedListeners.first();
            for (const auto& l : availableListeners) {
                if (l.Name == listenerName) {
                    listenerType = l.ListenerRegName;
                    break;
                }
            }
        }
    } else if (listenerCombobox && listenerCombobox->currentIndex() >= 0) {
        selectedListeners.append(listenerCombobox->currentText());
        listenerName = listenerCombobox->currentText();
        listenerType = listenerCombobox->currentData().toString();
    } else if (!listenerName.isEmpty()) {
        selectedListeners.append(listenerName);
    }
    if (selectedListeners.isEmpty()) {
        MessageError(isMultiListeners ? QStringLiteral("Please select at least one listener") : QStringLiteral("Please select a listener"));
        return;
    }

    if (shouldSaveProfile) {
        saveProfile(profileName, agentName, configData);
        loadProfiles();
    }

    buildLogOutput->clear();
    buildFileName.clear();
    buildFileContent.clear();
    if (fileChipButton) {
        fileChipButton->setVisible(false);
        fileChipButton->setText(QString());
    }
    showBuildLogView();
    buildLogOutput->append(QString("<span style='color: #569cd6;'>[*]</span> Listeners: %1").arg(selectedListeners.join(QStringLiteral(", ")).toHtmlEscaped()));

    buildButton->setText("Stop");

    QJsonArray listenersArray;
    for (const QString &listener : selectedListeners)
        listenersArray.append(listener);

    QJsonObject otpData;
    otpData.insert(QStringLiteral("agent_name"), agentName);
    otpData.insert(QStringLiteral("listeners_name"), listenersArray);
    const bool saveToStore = storeCheck && storeCheck->isChecked();
    otpData.insert(QStringLiteral("save_to_store"), saveToStore);
    if (saveToStore && inputDescription) {
        const QString desc = inputDescription->text().trimmed();
        if (!desc.isEmpty())
            otpData.insert(QStringLiteral("description"), desc);
    }

    QJsonObject body;
    body.insert(QStringLiteral("agent"), agentName);
    body.insert(QStringLiteral("listener_name"), listenersArray);
    body.insert(QStringLiteral("config"), configData);
    body.insert(QStringLiteral("save_to_store"), saveToStore);
    if (saveToStore && inputDescription) {
        const QString desc = inputDescription->text().trimmed();
        if (!desc.isEmpty())
            body.insert(QStringLiteral("description"), desc);
    }

    QString otp;
    bool otpOk = false;
    if (!HttpReqGetOTP(QStringLiteral("channel_agent_build"), otpData, authProfile, &otp, &otpOk) || !otpOk)
        otp.clear();

    const QString wsTemplate = QStringLiteral("wss://%1:%2%3/channel");
    const QUrl wsUrl(wsTemplate.arg(authProfile.GetHost()).arg(authProfile.GetPort()).arg(authProfile.GetEndpoint()));
    const QUrl generateUrl(authProfile.GetURL() + QStringLiteral("/agent/generate"));

    buildThread = new QThread;
    buildWorker = new BuildWorker(otp, wsUrl, configData, generateUrl, authProfile.GetAccessToken(), QJsonDocument(body).toJson(QJsonDocument::Compact));
    buildWorker->moveToThread(buildThread);

    connect(buildThread, &QThread::started,  buildWorker, &BuildWorker::start);
    connect(buildWorker, &BuildWorker::finished, buildThread, &QThread::quit, Qt::QueuedConnection);
    connect(buildThread, &QThread::finished, buildWorker, &QObject::deleteLater);
    connect(buildThread, &QThread::finished, buildThread, &QObject::deleteLater);

    connect(buildWorker, &BuildWorker::connected,           this, &DialogAgent::onBuildConnected,  Qt::QueuedConnection);
    connect(buildWorker, &BuildWorker::textMessageReceived, this, &DialogAgent::onBuildMessage,    Qt::QueuedConnection);
    connect(buildWorker, &BuildWorker::fileReady,           this, &DialogAgent::onBuildFileReady,  Qt::QueuedConnection);
    connect(buildWorker, &BuildWorker::errorOccurred,       this, &DialogAgent::onBuildError,      Qt::QueuedConnection);
    connect(buildWorker, &BuildWorker::finished,            this, &DialogAgent::onBuildFinished,   Qt::QueuedConnection);

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

            if (fileChipButton) {
                fileChipButton->setText(QStringLiteral("  %1").arg(filename));
                fileChipButton->setVisible(true);
            }

            buildLogOutput->append(QString("<span style='color: #dcdcaa;'>[+]</span> File ready: %1").arg(filename.toHtmlEscaped()));
            if (storeCheck && storeCheck->isChecked())
                buildLogOutput->append(QString("<span style='color: #569cd6;'>[*]</span> Registered in Payload Store"));
            else
                buildLogOutput->append(QString("<span style='color: #569cd6;'>[*]</span> Not saved to Payload Store"));

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

void DialogAgent::onBuildFileReady(const QString &filename, const QByteArray &content)
{
    if (filename.isEmpty() || content.isEmpty())
        return;

    buildFileName = filename;
    buildFileContent = content;

    if (fileChipButton) {
        fileChipButton->setText(QStringLiteral("  %1").arg(filename));
        fileChipButton->setVisible(true);
    }

    if (buildLogOutput) {
        buildLogOutput->append(QString("<span style='color: #dcdcaa;'>[+]</span> File ready: %1").arg(filename.toHtmlEscaped()));
        if (storeCheck && storeCheck->isChecked())
            buildLogOutput->append(QString("<span style='color: #569cd6;'>[*]</span> Registered in Payload Store"));
        else
            buildLogOutput->append(QString("<span style='color: #569cd6;'>[*]</span> Not saved to Payload Store"));
    }

    onSaveBuildFile();
}

void DialogAgent::onBuildError(const QString &err)
{
    if (!err.isEmpty() && buildLogOutput)
        buildLogOutput->append(QString("<span style='color: #f14c4c;'>[-]</span> %1").arg(err.toHtmlEscaped()));
}

void DialogAgent::onBuildFinished()
{
    if (buildLogOutput)
        buildLogOutput->append("----- Build process finished -----");

    if (buildButton)
        buildButton->setText(QStringLiteral("Generate"));

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
    if (!buildWorker)
        return;

    QPointer<BuildWorker> worker = buildWorker;
    buildWorker = nullptr;
    buildThread = nullptr;

    if (worker)
        QMetaObject::invokeMethod(worker.data(), "stop", Qt::QueuedConnection);
}

void DialogAgent::onListenerSelectionChanged(const QListWidgetItem *item)
{
    Q_UNUSED(item);

    QString agentName = agentCombobox->currentText();
    const bool isMulti = !agentName.isEmpty() && agentTypes.value(agentName, AgentTypeInfo{false, QStringList()}).multiListeners;
    if (!isMulti)
        return;

    updateListenerDisplay();

    if (agentName.isEmpty())
        return;

    QStringList selectedListeners;
    for (int i = 0; i < listenerListWidget->count(); ++i) {
        auto *listItem = listenerListWidget->item(i);
        if (listItem->checkState() == Qt::Checked)
            selectedListeners.append(listItem->data(Qt::UserRole).toString());
    }
    if (selectedListeners.isEmpty())
        return;

    regenerateAgentUI(agentName, selectedListeners);
}

void DialogAgent::onListenerComboChanged(int index)
{
    if (index < 0 || !listenerCombobox)
        return;

    listenerName = listenerCombobox->itemText(index);
    listenerType = listenerCombobox->itemData(index).toString();

    const QString agentName = agentCombobox->currentText();
    if (agentName.isEmpty() || listenerType.isEmpty())
        return;

    regenerateAgentUI(agentName, QStringList{listenerType});
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

    formElement->widget()->setMinimumSize(0, 0);
    formElement->widget()->setMaximumWidth(QWIDGETSIZE_MAX);
    ax_uis[agentName] = { container, formElement->widget(), h, w };
    configStackWidget->addWidget(formElement->widget());
    configStackWidget->setCurrentWidget(formElement->widget());
    formElement->widget()->adjustSize();
    configStackWidget->updateGeometry();
    if (agentConfigGroupbox)
        agentConfigGroupbox->updateGeometry();
    this->updateGeometry();
    packDialogSize(w, h);
}

void DialogAgent::packDialogSize(int scriptW, int scriptH)
{
    constexpr int kProfilesW  = 220;
    constexpr int kSeparatorW = 1;
    constexpr int kHChrome    = 5 * 2 + 10 * 2 + 5 * 2 + kProfilesW + kSeparatorW;
    constexpr int kFooterH    = 50;
    constexpr int kHeaderH    = 48 + 96;
    constexpr int kVChrome    = 5 * 2 + 10 * 2 + 5 * 2 + kHeaderH + kFooterH + 36;

    int panelW = scriptW > 0 ? scriptW : 360;
    int panelH = scriptH > 0 ? scriptH : 400;
    if (configStackWidget && configStackWidget->currentWidget()) {
        QWidget* cur = configStackWidget->currentWidget();
        cur->setMinimumWidth(0);
        const int minW = cur->minimumSizeHint().width();
        const int minH = cur->minimumSizeHint().height();
        if (minW > 0)
            panelW = qMax(panelW, minW);
        if (minH > 0)
            panelH = qMax(panelH, minH);
    }

    int w = panelW + kHChrome;
    int h = panelH + kVChrome;

    if (const QScreen* scr = this->screen()) {
        const QRect avail = scr->availableGeometry();
        w = qMin(w, int(avail.width() * 0.92));
        h = qMin(h, int(avail.height() * 0.90));
    }

    if (auto* lay = this->layout())
        lay->setSizeConstraint(QLayout::SetNoConstraint);

    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    this->setMinimumSize(0, 0);
    this->resize(w, h);
}

void DialogAgent::showConfigView()
{
    if (configViewBtn) {
        QSignalBlocker b(configViewBtn);
        configViewBtn->setChecked(true);
    }
    if (logViewBtn) {
        QSignalBlocker b(logViewBtn);
        logViewBtn->setChecked(false);
    }
    if (leftContentStack)
        leftContentStack->setCurrentIndex(0);
}

void DialogAgent::showBuildLogView()
{
    if (logViewBtn) {
        QSignalBlocker b(logViewBtn);
        logViewBtn->setChecked(true);
    }
    if (configViewBtn) {
        QSignalBlocker b(configViewBtn);
        configViewBtn->setChecked(false);
    }
    if (leftContentStack)
        leftContentStack->setCurrentIndex(1);
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
    if (!listenerChipsContainer)
        return;
    QLayout *layout = listenerChipsContainer->layout();
    while (layout->count() > 0) {
        QLayoutItem *item = layout->takeAt(0);
        if (item->widget())
            delete item->widget();
        delete item;
    }

    const AppTypography& ty = FontManager::instance().typography();
    const int fieldH = listenerChipsContainer->height() > 0 ? listenerChipsContainer->height() : ty.controlHeight;
    const int chipH = qMax(18, fieldH - 6);
    const int chipBtnSz = qMax(14, chipH - 4);

    for (int i = 0; i < listenerListWidget->count(); ++i) {
        auto *item = listenerListWidget->item(i);
        if (item->checkState() != Qt::Checked)
            continue;

        QString name = item->text();
        int idx = i;

        auto *chip = new QWidget(listenerChipsContainer);
        chip->setObjectName("ListenerChip");
        chip->setFixedHeight(chipH);
        chip->setStyleSheet(
            "QWidget#ListenerChip {"
            "  background: transparent;"
            "  border: 1px solid palette(highlight);"
            "  border-radius: 4px;"
            "}"
        );

        auto *chipLabel = new QLabel(QString(" %1 ").arg(name), chip);
        chipLabel->setStyleSheet(QStringLiteral(
            "color: palette(highlight); font-size: %1px; font-weight: 500; background: transparent; border: none;"
        ).arg(ty.chipFontPx));

        auto *chipBtn = new QPushButton("×", chip);
        chipBtn->setObjectName("ChipRemoveBtn");
        chipBtn->setFixedSize(chipBtnSz, chipBtnSz);
        chipBtn->setCursor(Qt::PointingHandCursor);
        chipBtn->setStyleSheet(QStringLiteral(
            "QPushButton#ChipRemoveBtn {"
            "  background: transparent; border: none;"
            "  color: palette(highlight); font-size: %1px; font-weight: bold;"
            "}"
            "QPushButton#ChipRemoveBtn:hover { color: palette(light); }"
        ).arg(qMax(10, ty.chromeFontPx)));
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