#include <UI/Dialogs/DialogAgent.h>
#include <UI/Dialogs/ProfileListDelegate.h>
#include <Utils/NonBlockingDialogs.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/Storage.h>
#include <QVBoxLayout>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QListWidget>
#include <QMenu>
#include <QInputDialog>
#include <QGuiApplication>
#include <QDir>
#include <QDateTime>
#include <QColorDialog>
#include <QTimer>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QHBoxLayout>

DialogAgent::DialogAgent(const QString &listenerName, const QString &listenerType)
{
    this->createUI();

    this->listenerInput->setText(listenerName);

    this->listenerName = listenerName;
    this->listenerType = listenerType;

    connect(buttonLoad,     &QPushButton::clicked,          this, &DialogAgent::onButtonLoad);
    connect(buttonSave,     &QPushButton::clicked,          this, &DialogAgent::onButtonSave);
    connect(agentCombobox,  &QComboBox::currentTextChanged, this, &DialogAgent::changeConfig);
    connect(generateButton, &QPushButton::clicked,          this, &DialogAgent::onButtonGenerateAgent);
    connect(closeButton,    &QPushButton::clicked,          this, &DialogAgent::onButtonClose);
    
    connect(listWidgetProfiles, &QListWidget::itemPressed, this, &DialogAgent::onProfileSelected);
    connect(listWidgetProfiles, &QListWidget::customContextMenuRequested, this, &DialogAgent::handleProfileContextMenu);
}

DialogAgent::~DialogAgent() = default;

void DialogAgent::createUI()
{
    this->setWindowTitle( "Generate Agent" );
    this->setProperty("Main", "base");

    listenerLabel = new QLabel("Listener:", this);
    listenerInput = new QLineEdit(this);
    listenerInput->setReadOnly(true);

    agentLabel    = new QLabel("Agent: ", this);
    agentCombobox = new QComboBox(this);

    buttonLoad = new QPushButton(QIcon(":/icons/file_open"), "", this);
    buttonLoad->setIconSize(QSize(25, 25));
    buttonLoad->setToolTip("Load profile from file");

    buttonSave = new QPushButton(QIcon(":/icons/save_as"), "", this);
    buttonSave->setIconSize(QSize(25, 25));
    buttonSave->setToolTip("Save profile to file");

    collapsibleDivider = new QWidget(this);
    collapsibleDivider->setMinimumWidth(20);
    collapsibleDivider->setMaximumWidth(50);
    collapsibleDivider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    collapsibleDivider->setStyleSheet("QWidget { background-color: transparent; }");
    collapsibleDivider->setCursor(Qt::PointingHandCursor);
    collapsibleDivider->setToolTip("Click to toggle right panel");
    
    auto dividerLayout = new QHBoxLayout(collapsibleDivider);
    dividerLayout->setContentsMargins(0, 0, 0, 0);
    dividerLayout->setSpacing(0);
    dividerLayout->addStretch();
    
    line_2 = new QFrame(collapsibleDivider);
    line_2->setFrameShape(QFrame::VLine);
    line_2->setFrameShadow(QFrame::Sunken);
    line_2->setStyleSheet("QFrame { color: rgba(100, 100, 100, 50); background-color: rgba(100, 100, 100, 50); }");
    line_2->setFixedWidth(2);
    line_2->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    line_2->setVisible(false);
    
    dividerLayout->addWidget(line_2);
    dividerLayout->addStretch();
    
    buttonToggleRightPanel = new QPushButton("»", this);
    buttonToggleRightPanel->setFixedSize(28, 40);
    buttonToggleRightPanel->setToolTip("Toggle right panel");
    QFont buttonFont = buttonToggleRightPanel->font();
    buttonFont.setPointSize(20);
    buttonFont.setBold(true);
    buttonToggleRightPanel->setFont(buttonFont);
    buttonToggleRightPanel->setStyleSheet(
        "QPushButton { "
        "border: none; "
        "background-color: transparent; "
        "color: rgba(200, 200, 200, 255); "
        "text-align: center; "
        "min-width: 14px; "
        "font-weight: bold; "
        "}"
        "QPushButton:hover { "
        "color: rgba(255, 255, 255, 255); "
        "}"
    );
    buttonToggleRightPanel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    buttonToggleRightPanel->setEnabled(false);
    buttonToggleRightPanel->raise();
    
    auto updateDividerGeometry = [this]() {
        updateButtonPosition();
    };
    
    QTimer::singleShot(0, this, updateDividerGeometry);
    
    collapsibleDivider->installEventFilter(this);

    listWidgetProfiles = new QListWidget(this);
    listWidgetProfiles->setFixedWidth(190);
    listWidgetProfiles->setContextMenuPolicy(Qt::CustomContextMenu);
    listWidgetProfiles->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidgetProfiles->setFocusPolicy(Qt::NoFocus);
    listWidgetProfiles->setAlternatingRowColors(false);
    listWidgetProfiles->setSpacing(2);
    listWidgetProfiles->setItemDelegate(new ProfileListDelegate(this));
    listWidgetProfiles->setStyleSheet(
        "QListWidget::item { padding: 8px; margin: 1px; }"
        "QScrollBar:vertical { width: 4px; background: transparent; }"
        "QScrollBar::handle:vertical { min-height: 20px; background: rgba(150, 150, 150, 100); border-radius: 2px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(150, 150, 150, 150); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    );

    menuContext = new QMenu(this);
    menuContext->addAction("Set Background Color", this, &DialogAgent::onSetBackgroundColor);
    menuContext->addSeparator();
    menuContext->addAction("Remove", this, &DialogAgent::onProfileRemove);
    listWidgetProfiles->addAction(menuContext->menuAction());

    auto profilesLayout = new QVBoxLayout();
    profilesLayout->setContentsMargins(0, 0, 0, 0);
    profilesLayout->addWidget(listWidgetProfiles);

    profilesGroupbox = new QGroupBox(this);
    profilesGroupbox->setTitle("Profiles");
    profilesGroupbox->setLayout(profilesLayout);

    configStackWidget = new QStackedWidget(this);

    stackGridLayout = new QGridLayout(this);
    stackGridLayout->setHorizontalSpacing(0);
    stackGridLayout->setContentsMargins(0, 0, 0, 0);
    stackGridLayout->addWidget(configStackWidget, 0, 0, 1, 1);

    agentConfigGroupbox = new QGroupBox( "Agent config", this );
    agentConfigGroupbox->setLayout(stackGridLayout);

    generateButton = new QPushButton( "Generate", this);
    generateButton->setProperty( "ButtonStyle", "dialog" );

    closeButton = new QPushButton( "Close", this);
    closeButton->setProperty( "ButtonStyle", "dialog" );

    horizontalSpacer   = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hLayoutBottom = new QHBoxLayout();
    hLayoutBottom->setContentsMargins(0, 10, 0, 0);
    hLayoutBottom->addItem(horizontalSpacer_2);
    hLayoutBottom->addWidget(generateButton);
    hLayoutBottom->addWidget(closeButton);
    hLayoutBottom->addItem(horizontalSpacer_3);

    auto leftPanelLayout = new QGridLayout();
    leftPanelLayout->setVerticalSpacing(10);
    leftPanelLayout->setHorizontalSpacing(5);
    leftPanelLayout->setContentsMargins(5, 5, 5, 5);
    leftPanelLayout->addWidget( listenerLabel,       0, 0, 1, 1);
    leftPanelLayout->addWidget( listenerInput,       0, 1, 1, 1);
    leftPanelLayout->addItem(   new QSpacerItem(10, 0, QSizePolicy::Fixed, QSizePolicy::Minimum), 0, 2, 1, 1);
    leftPanelLayout->addWidget( buttonLoad,          0, 3, 1, 1);
    leftPanelLayout->addWidget( agentLabel,          1, 0, 1, 1);
    leftPanelLayout->addWidget( agentCombobox,        1, 1, 1, 1);
    leftPanelLayout->addItem(   new QSpacerItem(10, 0, QSizePolicy::Fixed, QSizePolicy::Minimum), 1, 2, 1, 1);
    leftPanelLayout->addWidget( buttonSave,          1, 3, 1, 1);
    leftPanelLayout->addItem(   horizontalSpacer,    2, 0, 1, 4);
    leftPanelLayout->addWidget( agentConfigGroupbox, 3, 0, 1, 4);

    leftPanelLayout->setRowStretch(0, 0);
    leftPanelLayout->setRowStretch(1, 0);
    leftPanelLayout->setRowStretch(2, 0);
    leftPanelLayout->setRowStretch(3, 1);
    leftPanelLayout->setColumnStretch(0, 0);
    leftPanelLayout->setColumnStretch(1, 1);
    leftPanelLayout->setColumnStretch(2, 0);
    leftPanelLayout->setColumnStretch(3, 0);

    auto leftPanelWidget = new QWidget(this);
    leftPanelWidget->setLayout(leftPanelLayout);

    auto rightPanelLayout = new QVBoxLayout();
    rightPanelLayout->setSpacing(10);
    rightPanelLayout->setContentsMargins(5, 5, 5, 5);
    rightPanelLayout->addWidget(profilesGroupbox, 1);

    rightPanelWidget = new QWidget(this);
    rightPanelWidget->setLayout(rightPanelLayout);
    rightPanelWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    rightPanelWidget->setFixedWidth(0);
    rightPanelWidget->hide();

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setVerticalSpacing(0);
    mainGridLayout->setHorizontalSpacing(5);
    mainGridLayout->setContentsMargins(10, 10, 10, 10);
    mainGridLayout->addWidget( leftPanelWidget,          0, 0, 5, 1);
    mainGridLayout->addWidget( collapsibleDivider,       0, 1, 5, 1);
    mainGridLayout->addWidget( rightPanelWidget,         0, 2, 5, 1);
    mainGridLayout->addLayout( hLayoutBottom,            5, 0, 1, 3);

    for (int i = 0; i < 6; ++i) {
        mainGridLayout->setRowStretch(i, (i == 4) ? 1 : 0);
    }
    mainGridLayout->setColumnStretch(0, 1);
    mainGridLayout->setColumnStretch(1, 0);
    mainGridLayout->setColumnStretch(2, 0);
    mainGridLayout->setColumnMinimumWidth(1, 20);
    mainGridLayout->setColumnMinimumWidth(2, 0);

    this->setLayout(mainGridLayout);

    int buttonWidth = closeButton->width();
    int buttonHeight = closeButton->height();
    
    generateButton->setFixedSize(buttonWidth, buttonHeight);
    closeButton->setFixedSize(buttonWidth, buttonHeight);
    
    const int squareButtonSize = buttonHeight;
    buttonLoad->setFixedSize(squareButtonSize, squareButtonSize);
    buttonSave->setFixedSize(squareButtonSize, squareButtonSize);

    this->setMinimumWidth(600);
    
    rightPanelCollapsed = true;
    
    panelWidth = 190 + profilesGroupbox->contentsMargins().left() + profilesGroupbox->contentsMargins().right() + rightPanelLayout->contentsMargins().left() + rightPanelLayout->contentsMargins().right();
    if (panelWidth <= 0) {
        panelWidth = 200;
    }
}

void DialogAgent::Start()
{
    this->setModal(true);
    this->show();
    
    QTimer::singleShot(0, this, [this]() {
        collapsedSize = this->size();
    });
    
    if (buttonToggleRightPanel && collapsibleDivider) {
        QTimer::singleShot(0, this, [this]() {
            if (buttonToggleRightPanel && collapsibleDivider) {
                QPoint dividerPos = collapsibleDivider->mapTo(this, QPoint(0, 0));
                int y = dividerPos.y() + (collapsibleDivider->height() - buttonToggleRightPanel->height()) / 2;
                int dividerWidth = collapsibleDivider->width();
                int buttonX = dividerPos.x() + (dividerWidth - buttonToggleRightPanel->width()) / 2;
                buttonToggleRightPanel->setGeometry(buttonX, y, 28, 40);
                buttonToggleRightPanel->raise();
            }
        });
    }
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

void DialogAgent::onButtonGenerateAgent()
{
    QString agentName  = agentCombobox->currentText();
    auto configData = QString();

    if (ax_uis.contains(agentName) && ax_uis[agentName].container)
        configData = ax_uis[agentName].container->toJson();

    QString baseDir = authProfile.GetProjectDir();

    HttpReqAgentGenerateAsync(listenerName, listenerType, agentName, configData, authProfile, [this, baseDir, agentName, configData](bool success, const QString &message, const QJsonObject&) {
        if (!success) {
            MessageError(message);
            return;
        }

        if (currentProfileName.isEmpty()) {
            QJsonObject dataJson;
            dataJson["listener_type"] = listenerType;
            dataJson["agent"] = agentName;
            dataJson["config"] = configData;
            QString profileData = QJsonDocument(dataJson).toJson(QJsonDocument::Compact);

            QString profileName = QString("%1_%2").arg(agentName).arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
            Storage::AddAgentProfile(authProfile.GetProject(), profileName, profileData);
            
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
            
            currentProfileName.clear();
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

void DialogAgent::onButtonClose() { this->close(); }

void DialogAgent::changeConfig(const QString &agentName)
{
    if (ax_uis.contains(agentName)) {
        auto ax_ui = &ax_uis[agentName];
        if (ax_ui)
            configStackWidget->setCurrentWidget(ax_ui->widget);
        this->resize(ax_ui->width, ax_ui->height);
    }
}

void DialogAgent::loadProfiles()
{
    listWidgetProfiles->clear();

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
        QString profileAgent = jsonObject.contains("agent") && jsonObject["agent"].isString() 
                              ? jsonObject["agent"].toString() 
                              : "";

        auto* item = new QListWidgetItem(profileName);
        item->setData(Qt::UserRole, profileAgent);
        item->setData(Qt::UserRole + 1, profileName);
        
        if (jsonObject.contains("backgroundColor") && jsonObject["backgroundColor"].isString()) {
            QString colorStr = jsonObject["backgroundColor"].toString();
            QColor bgColor(colorStr);
            if (bgColor.isValid()) {
                item->setData(Qt::BackgroundRole, QBrush(bgColor));
            }
        }
        
        listWidgetProfiles->addItem(item);
    }
}

void DialogAgent::onProfileSelected()
{
    auto* item = listWidgetProfiles->currentItem();
    if (!item)
        return;

    QString profileName = item->data(Qt::UserRole + 1).toString();
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

    if (jsonObject.contains("listener_type") && jsonObject["listener_type"].isString()) {
        QString profileListenerType = jsonObject["listener_type"].toString();
        if (profileListenerType != listenerType) {
            MessageError("Listener type mismatch");
            return;
        }
    }

    if (jsonObject.contains("agent") && jsonObject["agent"].isString()) {
        QString configAgent = jsonObject["agent"].toString();
        int agentIndex = agentCombobox->findText(configAgent);
        if (agentIndex != -1 && ax_uis.contains(configAgent)) {
            agentCombobox->setCurrentIndex(agentIndex);
            if (jsonObject.contains("config") && jsonObject["config"].isString()) {
                QString configData = jsonObject["config"].toString();
                ax_uis[configAgent].container->fromJson(configData);
            }
        }
    }

    currentProfileName = profileName;
}

void DialogAgent::handleProfileContextMenu(const QPoint &pos)
{
    QPoint globalPos = listWidgetProfiles->mapToGlobal(pos);
    menuContext->exec(globalPos);
}

void DialogAgent::onProfileRemove()
{
    auto* item = listWidgetProfiles->currentItem();
    if (!item)
        return;

    QString profileName = item->data(Qt::UserRole + 1).toString();
    if (!profileName.isEmpty()) {
        QString project = authProfile.GetProject();
        if (!project.isEmpty()) {
            Storage::RemoveAgentProfile(project, profileName);
        }
    }

    delete listWidgetProfiles->takeItem(listWidgetProfiles->row(item));
}

void DialogAgent::onSetBackgroundColor()
{
    auto* item = listWidgetProfiles->currentItem();
    if (!item)
        return;
    
    QString profileName = item->data(Qt::UserRole + 1).toString();
    if (profileName.isEmpty())
        return;
    
    QString project = authProfile.GetProject();
    if (project.isEmpty())
        return;
    
    QVariant bgColorVariant = item->data(Qt::BackgroundRole);
    QColor currentColor;
    if (bgColorVariant.isValid() && bgColorVariant.canConvert<QBrush>()) {
        QBrush brush = bgColorVariant.value<QBrush>();
        if (brush.style() != Qt::NoBrush) {
            currentColor = brush.color();
        }
    }
    
    QColor color = QColorDialog::getColor(currentColor.isValid() ? currentColor : QColor(), this, "Select Background Color");
    if (color.isValid()) {
        item->setData(Qt::BackgroundRole, QBrush(color));
        listWidgetProfiles->viewport()->update();
        
        QString profileData = Storage::GetAgentProfile(project, profileName);
        if (!profileData.isEmpty()) {
            QJsonParseError parseError;
            QJsonDocument document = QJsonDocument::fromJson(profileData.toUtf8(), &parseError);
            if (parseError.error == QJsonParseError::NoError && document.isObject()) {
                QJsonObject jsonObject = document.object();
                jsonObject["backgroundColor"] = color.name();
                QString updatedProfileData = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
                Storage::AddAgentProfile(project, profileName, updatedProfileData);
            }
        }
    }
}

bool DialogAgent::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == collapsibleDivider) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                onToggleRightPanel();
                return true;
            }
        } else if (event->type() == QEvent::Resize) {
            QTimer::singleShot(0, this, [this]() {
                updateButtonPosition();
            });
        }
    }
    return QDialog::eventFilter(obj, event);
}

void DialogAgent::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    updateButtonPosition();
}

void DialogAgent::updateButtonPosition()
{
    if (buttonToggleRightPanel && collapsibleDivider) {
        QPoint dividerPos = collapsibleDivider->mapTo(this, QPoint(0, 0));
        int y = dividerPos.y() + (collapsibleDivider->height() - buttonToggleRightPanel->height()) / 2;
        int dividerWidth = collapsibleDivider->width();
        int buttonX = dividerPos.x() + (dividerWidth - buttonToggleRightPanel->width()) / 2;
        buttonToggleRightPanel->setGeometry(buttonX, y, 28, 40);
        buttonToggleRightPanel->raise();
    }
}

void DialogAgent::onToggleRightPanel()
{
    rightPanelCollapsed = !rightPanelCollapsed;
    
    QFontMetrics fm(buttonToggleRightPanel->font());
    QString arrowText = rightPanelCollapsed ? "»" : "«";
    
    int width1 = fm.horizontalAdvance("«");
    int width2 = fm.horizontalAdvance("»");
    int maxWidth = qMax(width1, width2);
    
    int currentWidth = fm.horizontalAdvance(arrowText);
    int spaceWidth = fm.horizontalAdvance(" ");
    int padding = (maxWidth - currentWidth) / 2;
    int spaces = (spaceWidth > 0) ? padding / spaceWidth : 0;
    
    if (spaces > 0) {
        arrowText = QString(spaces, ' ') + arrowText;
    }
    
    this->setUpdatesEnabled(false);
    
    if (rightPanelCollapsed) {
        if (collapsedSize.isEmpty()) {
            collapsedSize = this->size();
        }
        
        buttonToggleRightPanel->setText(arrowText);
        mainGridLayout->setColumnStretch(2, 0);
        
        rightPanelWidget->setFixedWidth(0);
        rightPanelWidget->setVisible(false);
        line_2->setVisible(false);
        
        if (!collapsedSize.isEmpty()) {
            this->resize(collapsedSize);
        }
    } else {
        if (collapsedSize.isEmpty()) {
            collapsedSize = this->size();
        }
        
        if (panelWidth <= 0) {
            panelWidth = 200;
        }
        
        QSize newSize = collapsedSize;
        newSize.setWidth(collapsedSize.width() + panelWidth);
        
        buttonToggleRightPanel->setText(arrowText);
        mainGridLayout->setColumnStretch(2, 0);
        
        rightPanelWidget->setFixedWidth(panelWidth);
        rightPanelWidget->setVisible(true);
        line_2->setVisible(true);
        
        this->resize(newSize);
    }
    
    QTimer::singleShot(0, this, [this]() {
        this->setUpdatesEnabled(true);
        this->update();
    });
    
    QTimer::singleShot(0, this, [this]() {
        if (buttonToggleRightPanel && collapsibleDivider) {
            QPoint dividerPos = collapsibleDivider->mapTo(this, QPoint(0, 0));
            int y = dividerPos.y() + (collapsibleDivider->height() - buttonToggleRightPanel->height()) / 2;
            int dividerWidth = collapsibleDivider->width();
            int buttonX = dividerPos.x() + (dividerWidth - buttonToggleRightPanel->width()) / 2;
            buttonToggleRightPanel->setGeometry(buttonX, y, 28, 40);
            buttonToggleRightPanel->raise();
        }
    });
}