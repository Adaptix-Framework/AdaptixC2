#include <UI/Dialogs/DialogListener.h>
#include <Utils/NonBlockingDialogs.h>
#include <Client/Requestor.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/Storage.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QIODevice>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QColorDialog>
#include <QTimer>
#include <QMouseEvent>
#include <QResizeEvent>

DialogListener::DialogListener(QWidget *parent) : QDialog(parent)
{
    this->createUI();

    connect(listenerCombobox,     &QComboBox::currentTextChanged, this, &DialogListener::changeConfig);
    connect(listenerTypeCombobox, &QComboBox::currentTextChanged, this, &DialogListener::changeType);
    connect(buttonLoad,   &QPushButton::clicked, this, &DialogListener::onButtonLoad );
    connect(buttonSave,   &QPushButton::clicked, this, &DialogListener::onButtonSave );
    connect(buttonCreate, &QPushButton::clicked, this, &DialogListener::onButtonCreate );
    connect(buttonCancel, &QPushButton::clicked, this, &DialogListener::onButtonCancel );
    
    connect(listWidgetProfiles, &QListWidget::itemPressed, this, &DialogListener::onProfileSelected);
    connect(listWidgetProfiles, &QListWidget::customContextMenuRequested, this, &DialogListener::handleProfileContextMenu);
    
    loadProfiles();
}

DialogListener::~DialogListener() = default;

void DialogListener::createUI()
{
    this->setWindowTitle("Create Listener");
    this->setProperty("Main", "base");

    buttonLoad = new QPushButton(QIcon(":/icons/file_open"), "", this);
    buttonLoad->setIconSize( QSize( 25,25 ));
    buttonLoad->setToolTip("Load profile from file");

    buttonSave = new QPushButton(QIcon(":/icons/save_as"), "", this);
    buttonSave->setIconSize( QSize( 25,25 ));
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
    profileDelegate = new ProfileListDelegate(this);
    listWidgetProfiles->setItemDelegate(profileDelegate);
    listWidgetProfiles->setStyleSheet(
        "QListWidget::item { padding: 8px; margin: 1px; }"
        "QScrollBar:vertical { width: 4px; background: transparent; }"
        "QScrollBar::handle:vertical { min-height: 20px; background: rgba(150, 150, 150, 100); border-radius: 2px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(150, 150, 150, 150); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    );

    auto profilesLayout = new QVBoxLayout();
    profilesLayout->setContentsMargins(0, 0, 0, 0);
    profilesLayout->addWidget(listWidgetProfiles);

    profilesGroupbox = new QGroupBox(this);
    profilesGroupbox->setTitle("Profiles");
    profilesGroupbox->setLayout(profilesLayout);

    menuContext = new QMenu(this);
    menuContext->addAction("Set Background Color", this, &DialogListener::onSetBackgroundColor);
    actionResetBackgroundColor = menuContext->addAction("Reset Background Color", this, &DialogListener::onResetBackgroundColor);
    menuContext->addSeparator();
    menuContext->addAction("Remove", this, &DialogListener::onProfileRemove);
    listWidgetProfiles->addAction(menuContext->menuAction());

    configStackWidget = new QStackedWidget(this);

    stackGridLayout = new QGridLayout(this);
    stackGridLayout->setHorizontalSpacing(0);
    stackGridLayout->setContentsMargins(0, 0, 0, 0 );
    stackGridLayout->addWidget(configStackWidget, 0, 0, 1, 1 );

    listenerConfigGroupbox = new QGroupBox(this);
    listenerConfigGroupbox->setTitle("Listener config");
    listenerConfigGroupbox->setLayout(stackGridLayout);

    buttonCreate = new QPushButton(this);
    buttonCreate->setProperty("ButtonStyle", "dialog");
    buttonCreate->setText("Create");

    buttonCancel = new QPushButton(this);
    buttonCancel->setProperty("ButtonStyle", "dialog");
    buttonCancel->setText("Cancel");

    horizontalSpacer   = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    hLayoutBottom = new QHBoxLayout();
    hLayoutBottom->setContentsMargins(0, 10, 0, 0);
    hLayoutBottom->addItem(horizontalSpacer_2);
    hLayoutBottom->addWidget(buttonCreate);
    hLayoutBottom->addWidget(buttonCancel);
    hLayoutBottom->addItem(horizontalSpacer_3);

    listenerNameLabel = new QLabel(this);
    listenerNameLabel->setText("Listener name:");

    inputListenerName = new QLineEdit(this);

    listenerTypeLabel = new QLabel(this);
    listenerTypeLabel->setText("Listener type: ");
    listenerTypeCombobox = new QComboBox(this);

    listenerLabel = new QLabel(this);
    listenerLabel->setText("Listener: ");
    listenerCombobox = new QComboBox(this);

    auto leftPanelLayout = new QGridLayout();
    leftPanelLayout->setVerticalSpacing(10);
    leftPanelLayout->setHorizontalSpacing(5);
    leftPanelLayout->setContentsMargins(5, 5, 5, 5);
    leftPanelLayout->addWidget( listenerNameLabel,      0, 0, 1, 1);
    leftPanelLayout->addWidget( inputListenerName,      0, 1, 1, 1);
    leftPanelLayout->addItem(   new QSpacerItem(10, 0, QSizePolicy::Fixed, QSizePolicy::Minimum), 0, 2, 1, 1);
    leftPanelLayout->addWidget( buttonLoad,             0, 3, 1, 1);
    leftPanelLayout->addWidget( listenerTypeLabel,      1, 0, 1, 1);
    leftPanelLayout->addWidget( listenerTypeCombobox,   1, 1, 1, 1);
    leftPanelLayout->addItem(   new QSpacerItem(10, 0, QSizePolicy::Fixed, QSizePolicy::Minimum), 1, 2, 1, 1);
    leftPanelLayout->addWidget( buttonSave,             1, 3, 1, 1);
    leftPanelLayout->addWidget( listenerLabel,          2, 0, 1, 1);
    leftPanelLayout->addWidget( listenerCombobox,       2, 1, 1, 1);
    leftPanelLayout->addItem(   horizontalSpacer,       3, 0, 1, 4);
    leftPanelLayout->addWidget( listenerConfigGroupbox, 4, 0, 1, 4);

    leftPanelLayout->setRowStretch(0, 0);
    leftPanelLayout->setRowStretch(1, 0);
    leftPanelLayout->setRowStretch(2, 0);
    leftPanelLayout->setRowStretch(3, 0);
    leftPanelLayout->setRowStretch(4, 1);
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

    this->setLayout(mainGridLayout);

    int buttonWidth = buttonCancel->width();
    int buttonHeight = buttonCancel->height();
    
    buttonCreate->setFixedSize(buttonWidth, buttonHeight);
    buttonCancel->setFixedSize(buttonWidth, buttonHeight);
    
    const int rightPanelButtonWidth = 190;
    const int squareButtonSize = buttonHeight;
    const int spacing = 10;
    
    buttonLoad->setFixedSize(squareButtonSize, squareButtonSize);
    buttonSave->setFixedSize(squareButtonSize, squareButtonSize);

    this->setMinimumWidth(800);
    
    rightPanelCollapsed = true;
    rightPanelWidget->setVisible(false);
    line_2->setVisible(false);
    
    panelWidth = 190 + profilesGroupbox->contentsMargins().left() + profilesGroupbox->contentsMargins().right() + rightPanelLayout->contentsMargins().left() + rightPanelLayout->contentsMargins().right();
    if (panelWidth <= 0) {
        panelWidth = 200;
    }
}

void DialogListener::Start()
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
        QString type = listener.type + " (" + listener.protocol + ")";
        listenersSet.insert(type);
    }
    listenerTypeCombobox->addItem("any");
    listenerTypeCombobox->addItems(QList<QString>(listenersSet.begin(), listenersSet.end()));
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
    
    listWidgetProfiles->setEnabled(false);
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
        QString listenerType = listener.type + " (" + listener.protocol + ")";
        if (listenerType == type || type == "any")
            listenerCombobox->addItem(listener.name);
    }
}

void DialogListener::onButtonCreate()
{
    auto configName = inputListenerName->text();
    auto configType = listenerCombobox->currentText();
    auto configData = QString();
    if (ax_uis.contains(configType) && ax_uis[configType].container)
        configData = ax_uis[configType].container->toJson();

    buttonCreate->setEnabled(false);
    buttonCreate->setText(editMode ? "Editing..." : "Creating...");

    auto callback = [this, configName, configType, configData](bool success, const QString &message, const QJsonObject&) {
        if (!success) {
            MessageError(message);
            buttonCreate->setEnabled(true);
            buttonCreate->setText(editMode ? "Edit" : "Create");
        } else {
            QJsonObject dataJson;
            dataJson["name"] = configName;
            dataJson["type"] = configType;
            dataJson["config"] = configData;
            QString profileData = QJsonDocument(dataJson).toJson(QJsonDocument::Compact);

            Storage::AddListenerProfile(authProfile.GetProject(), configName, profileData);
            
            loadProfiles();

            this->close();
        }
    };

    if (editMode)
        HttpReqListenerEditAsync(configName, configType, configData, authProfile, callback);
    else
        HttpReqListenerStartAsync(configName, configType, configData, authProfile, callback);
}

void DialogListener::onButtonLoad()
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

            if ( !jsonObject.contains("type") || !jsonObject["type"].isString() ) {
                MessageError("Required parameter 'type' is missing");
                return;
            }
            if ( !jsonObject.contains("config") || !jsonObject["config"].isString() ) {
                MessageError("Required parameter 'config' is missing");
                return;
            }

            if( jsonObject.contains("name") && jsonObject["name"].isString())
                inputListenerName->setText( jsonObject["name"].toString() );

            QString configType = jsonObject["type"].toString();
            int typeIndex = listenerCombobox->findText( configType );

            if(typeIndex == -1 || !ax_uis.contains(configType)) {
                MessageError("No such listener exists");
                return;
            }

            QString configData = jsonObject["config"].toString();
            listenerCombobox->setCurrentIndex(typeIndex);

            ax_uis[configType].container->fromJson(configData);
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
    NonBlockingDialogs::getSaveFileName(this, "Save File", initialPath, "JSON files (*.json)",
        [this, fileContent](const QString& filePath) {
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

void DialogListener::onButtonCancel() { this->close(); }

void DialogListener::loadProfiles()
{
    listWidgetProfiles->clear();

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

        auto* item = new QListWidgetItem(profileName);
        item->setData(Qt::UserRole, profileType);
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

void DialogListener::onProfileSelected()
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
    auto* item = listWidgetProfiles->itemAt(pos);
    bool hasColor = false;
    
    if (item) {
        QVariant bgColorVariant = item->data(Qt::BackgroundRole);
        if (bgColorVariant.isValid() && bgColorVariant.canConvert<QBrush>()) {
            QBrush brush = bgColorVariant.value<QBrush>();
            if (brush.style() != Qt::NoBrush) {
                hasColor = true;
            }
        }
    }
    
    if (actionResetBackgroundColor) {
        actionResetBackgroundColor->setEnabled(hasColor);
    }
    
    QPoint globalPos = listWidgetProfiles->mapToGlobal(pos);
    menuContext->exec(globalPos);
}

void DialogListener::onProfileRemove()
{
    auto* item = listWidgetProfiles->currentItem();
    if (!item)
        return;

    QString profileName = item->data(Qt::UserRole + 1).toString();
    if (!profileName.isEmpty()) {
        QString project = authProfile.GetProject();
        if (!project.isEmpty()) {
            Storage::RemoveListenerProfile(project, profileName);
        }
    }

    delete listWidgetProfiles->takeItem(listWidgetProfiles->row(item));
}

void DialogListener::onSetBackgroundColor()
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
        
        QString profileData = Storage::GetListenerProfile(project, profileName);
        if (!profileData.isEmpty()) {
            QJsonParseError parseError;
            QJsonDocument document = QJsonDocument::fromJson(profileData.toUtf8(), &parseError);
            if (parseError.error == QJsonParseError::NoError && document.isObject()) {
                QJsonObject jsonObject = document.object();
                jsonObject["backgroundColor"] = color.name();
                QString updatedProfileData = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
                Storage::AddListenerProfile(project, profileName, updatedProfileData);
            }
        }
    }
}

void DialogListener::onResetBackgroundColor()
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
    bool hasColor = false;
    if (bgColorVariant.isValid() && bgColorVariant.canConvert<QBrush>()) {
        QBrush brush = bgColorVariant.value<QBrush>();
        if (brush.style() != Qt::NoBrush) {
            hasColor = true;
        }
    }
    
    if (!hasColor)
        return;
    
    item->setData(Qt::BackgroundRole, QVariant());
    listWidgetProfiles->viewport()->update();
    
    QString profileData = Storage::GetListenerProfile(project, profileName);
    if (!profileData.isEmpty()) {
        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson(profileData.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            QJsonObject jsonObject = document.object();
            jsonObject.remove("backgroundColor");
            QString updatedProfileData = QJsonDocument(jsonObject).toJson(QJsonDocument::Compact);
            Storage::AddListenerProfile(project, profileName, updatedProfileData);
        }
    }
}

bool DialogListener::eventFilter(QObject *obj, QEvent *event)
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

void DialogListener::onToggleRightPanel()
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

void DialogListener::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    updateButtonPosition();
}

void DialogListener::updateButtonPosition()
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