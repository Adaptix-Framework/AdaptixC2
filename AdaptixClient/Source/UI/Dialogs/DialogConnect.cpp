#include <UI/Dialogs/DialogConnect.h>
#include <Client/AuthProfile.h>
#include <Client/Storage.h>
#include <MainAdaptix.h>
#include <Utils/NonBlockingDialogs.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QIODevice>
#include <QFrame>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

ProfileListDelegate::ProfileListDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

QFont ProfileListDelegate::getFont(const QWidget *widget) const
{
    return widget ? widget->font() : QApplication::font();
}

void ProfileListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    
    QString projectName = index.data(Qt::DisplayRole).toString();
    QString username = index.data(Qt::UserRole).toString();
    QString host = index.data(Qt::UserRole + 1).toString();
    
    opt.text.clear();
    
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    painter->save();

    QRect rect = option.rect.adjusted(12, 10, -12, -10);
    int padding = 2;

    QFont titleFont = getFont(opt.widget);
    
    QVariant fontVariant = index.data(Qt::FontRole);
    if (fontVariant.isValid() && fontVariant.canConvert<QFont>()) {
        QFont itemFont = fontVariant.value<QFont>();
        if (itemFont.bold()) {
            titleFont.setBold(true);
        }
    }
    
    if (option.state & QStyle::State_Selected) {
        painter->setPen(option.palette.highlightedText().color());
    } else {
        painter->setPen(option.palette.text().color());
    }
    
    QFontMetrics titleFm(titleFont);
    painter->setFont(titleFont);

    QRect titleRect = rect;
    titleRect.setHeight(titleFm.height());
    QString elidedTitle = titleFm.elidedText(projectName, Qt::ElideRight, titleRect.width());
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignTop, elidedTitle);

    if (!username.isEmpty() && !host.isEmpty()) {
        QColor subtitleColor;
        if (option.state & QStyle::State_Selected) {
            subtitleColor = option.palette.highlightedText().color();
            subtitleColor.setAlpha(180);
        } else {
            subtitleColor = option.palette.color(QPalette::Disabled, QPalette::Text);
        }
        
        QFont subtitleFont = titleFont;
        subtitleFont.setPointSize(subtitleFont.pointSize() - 3);
        painter->setFont(subtitleFont);
        painter->setPen(subtitleColor);

        QString subtitle = QString("%1@%2").arg(username, host);

        QFontMetrics subtitleFm(subtitleFont);
        QRect subtitleRect = rect;
        subtitleRect.setTop(titleRect.bottom() + padding);
        subtitleRect.setHeight(subtitleFm.height());
        QString elidedSubtitle = subtitleFm.elidedText(subtitle, Qt::ElideRight, subtitleRect.width());
        painter->drawText(subtitleRect, Qt::AlignLeft | Qt::AlignTop, elidedSubtitle);
    }

    painter->restore();
}

QSize ProfileListDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QFont titleFont = getFont(option.widget);
    
    QFontMetrics fm(titleFont);
    int baseHeight = fm.height() + 20;
    
    QString username = index.data(Qt::UserRole).toString();
    QString host = index.data(Qt::UserRole + 1).toString();

    if (!username.isEmpty() && !host.isEmpty()) {
        QFont subtitleFont = titleFont;
        subtitleFont.setPointSize(subtitleFont.pointSize() - 3);
        QFontMetrics subtitleFm(subtitleFont);
        baseHeight += subtitleFm.height() + 2;
    }

    return QSize(100, baseHeight);
}

static QString defaultProjectDir(const QString &projectName)
{
    return QDir::home().filePath("AdaptixProjects/" + projectName.trimmed());
}

DialogConnect::DialogConnect()
{
    createUI();
    loadProjects();

    connect(listWidget, &QListWidget::itemPressed, this, &DialogConnect::itemSelected);
    connect(listWidget, &QListWidget::customContextMenuRequested, this, &DialogConnect::handleContextMenu);
    connect(ButtonNewProfile, &QPushButton::clicked, this, &DialogConnect::onButton_NewProfile);
    connect(ButtonLoad, &QPushButton::clicked, this, &DialogConnect::onButton_Load);
    connect(ButtonSave, &QPushButton::clicked, this, &DialogConnect::onButton_Save);

    auto connectReturnPressed = [this](QLineEdit* edit) {
        connect(edit, &QLineEdit::returnPressed, this, &DialogConnect::onButton_Connect);
    };
    connectReturnPressed(lineEdit_Project);
    connectReturnPressed(lineEdit_Host);
    connectReturnPressed(lineEdit_Port);
    connectReturnPressed(lineEdit_Endpoint);
    connectReturnPressed(lineEdit_User);
    connectReturnPressed(lineEdit_Password);

    connect(lineEdit_Project, &QLineEdit::textChanged, this, &DialogConnect::onProjectNameChanged);
    connect(lineEdit_ProjectDir, &QLineEdit::textEdited, this, &DialogConnect::onProjectDirEdited);
    connect(ButtonConnect, &QPushButton::clicked, this, &DialogConnect::onButton_Connect);
    connect(ButtonClear, &QPushButton::clicked, this, &DialogConnect::onButton_Clear);

    auto action = lineEdit_ProjectDir->addAction(QIcon(":/icons/folder"), QLineEdit::TrailingPosition);
    connect(action, &QAction::triggered, this, &DialogConnect::onSelectProjectDir);
}

DialogConnect::~DialogConnect() = default;

void DialogConnect::createUI()
{
    resize(700, 400);
    setFixedSize(700, 400);
    setWindowTitle("Connect");
    setProperty("Main", "base");

    label_UserInfo = new QLabel("User information", this);
    label_UserInfo->setAlignment(Qt::AlignCenter);

    label_User = new QLabel("User:", this);
    label_Password = new QLabel("Password:", this);

    label_ServerDetails = new QLabel("Server details", this);
    label_ServerDetails->setAlignment(Qt::AlignCenter);

    label_Project = new QLabel("Project:", this);
    label_ProjectDir = new QLabel("Project directory:", this);
    label_Host = new QLabel("Host:", this);
    label_Port = new QLabel("Port:", this);
    label_Endpoint = new QLabel("Endpoint:", this);


    lineEdit_User = new QLineEdit( this );
    lineEdit_User->setPlaceholderText("operator1");
    lineEdit_User->setToolTip("Enter your username");

    lineEdit_Password = new QLineEdit( this );
    lineEdit_Password->setPlaceholderText("••••");
    lineEdit_Password->setEchoMode( QLineEdit::Password );
    lineEdit_Password->setToolTip("Enter your password");

    lineEdit_Project = new QLineEdit( this );
    lineEdit_Project->setPlaceholderText("MyProject");
    lineEdit_Project->setToolTip("Enter project name");

    lineEdit_ProjectDir = new QLineEdit( this );
    lineEdit_ProjectDir->setPlaceholderText("~/AdaptixProjects/MyProject");
    lineEdit_ProjectDir->setToolTip("Enter path to project directory (auto-generated if empty)");

    lineEdit_Host = new QLineEdit( this );
    lineEdit_Host->setPlaceholderText("192.168.1.1 or example.com");
    lineEdit_Host->setToolTip("Enter server host (IP address, domain name, or localhost)");

    lineEdit_Port = new QLineEdit( this );
    lineEdit_Port->setPlaceholderText("4321");
    lineEdit_Port->setToolTip("Enter server port number (1-65535)");
    lineEdit_Port->setValidator( new QRegularExpressionValidator( QRegularExpression( "[0-9]*" ), lineEdit_Port ) );

    lineEdit_Endpoint = new QLineEdit( this );
    lineEdit_Endpoint->setPlaceholderText("/endpoint");
    lineEdit_Endpoint->setToolTip("Enter API endpoint URI (must start with /, example: /endpoint)");


    ButtonConnect = new QPushButton(this);
    ButtonConnect->setProperty("ButtonStyle", "dialog");
    ButtonConnect->setText("Connect");
    ButtonConnect->setFocus();

    ButtonClear = new QPushButton(this);
    ButtonClear->setProperty("ButtonStyle", "dialog");
    ButtonClear->setText("Clear");


    menuContex = new QMenu( this );
    menuContex->addAction( "Remove", this, &DialogConnect::itemRemove );

    label_Profiles = new QLabel( this );
    label_Profiles->setAlignment(Qt::AlignCenter);
    label_Profiles->setText( "Profiles" );

    listWidget = new QListWidget(this);
    listWidget->setFixedWidth(190);
    listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    listWidget->addAction(menuContex->menuAction());
    listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget->setFocusPolicy(Qt::NoFocus);
    listWidget->setAlternatingRowColors(true);
    listWidget->setSpacing(2);
    listWidget->setItemDelegate(new ProfileListDelegate(this));
    listWidget->setStyleSheet(
        "QListWidget::item { padding: 8px; margin: 1px; }"
        "QScrollBar:vertical { width: 4px; background: transparent; }"
        "QScrollBar::handle:vertical { min-height: 20px; background: rgba(150, 150, 150, 100); border-radius: 2px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(150, 150, 150, 150); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }"
    );

    ButtonNewProfile = new QPushButton(this);
    ButtonNewProfile->setProperty("ButtonStyle", "dialog");
    ButtonNewProfile->setText("New Profile");
    ButtonNewProfile->setMinimumSize( QSize( 10, 30 ) );

    ButtonLoad = new QPushButton(QIcon(":/icons/upload"), "", this);
    ButtonLoad->setProperty("ButtonStyle", "dialog");
    ButtonLoad->setIconSize( QSize( 20, 20 ) );
    ButtonLoad->setFixedSize( QSize( 30, 30 ) );
    ButtonLoad->setToolTip("Load profile from file");

    ButtonSave = new QPushButton(QIcon(":/icons/downloads"), "", this);
    ButtonSave->setProperty("ButtonStyle", "dialog");
    ButtonSave->setIconSize( QSize( 20, 20 ) );
    ButtonSave->setFixedSize( QSize( 30, 30 ) );
    ButtonSave->setToolTip("Save profile to file");

    auto formWidget = new QWidget(this);
    auto formLayout = new QGridLayout(formWidget);
    formLayout->setContentsMargins(0, 0, 0, 0);

    formLayout->addWidget( label_UserInfo,      0, 0, 1, 2 );
    formLayout->addWidget( label_User,          1, 0, 1, 1 );
    formLayout->addWidget( lineEdit_User,       1, 1, 1, 1 );
    formLayout->addWidget( label_Password,      2, 0, 1, 1 );
    formLayout->addWidget( lineEdit_Password,   2, 1, 1, 1 );

    formLayout->addWidget( label_ServerDetails, 4, 0, 1, 2 );
    formLayout->addWidget( label_Project,       5, 0, 1, 1 );
    formLayout->addWidget( lineEdit_Project,    5, 1, 1, 1 );
    formLayout->addWidget( label_ProjectDir,    6, 0, 1, 1 );
    formLayout->addWidget( lineEdit_ProjectDir, 6, 1, 1, 1 );
    formLayout->addWidget( label_Host,          7, 0, 1, 1 );
    formLayout->addWidget( lineEdit_Host,       7, 1, 1, 1 );
    formLayout->addWidget( label_Port,          8, 0, 1, 1 );
    formLayout->addWidget( lineEdit_Port,       8, 1, 1, 1 );
    formLayout->addWidget( label_Endpoint,      9, 0, 1, 1 );
    formLayout->addWidget( lineEdit_Endpoint,   9, 1, 1, 1 );

    formLayout->setRowMinimumHeight(10, 14);

    formLayout->addWidget( ButtonClear,        11, 0, 1, 1 );
    formLayout->addWidget( ButtonConnect,      11, 1, 1, 1 );
    formLayout->setRowStretch(12, 1);

    auto separatorLine = new QFrame(this);
    separatorLine->setFrameShape(QFrame::VLine);
    separatorLine->setFrameShadow(QFrame::Sunken);
    separatorLine->setStyleSheet("QFrame { color: rgba(100, 100, 100, 50); background-color: rgba(100, 100, 100, 50); }");

    auto buttonsLayout = new QHBoxLayout();
    buttonsLayout->addWidget( ButtonNewProfile );
    buttonsLayout->addWidget( ButtonLoad );
    buttonsLayout->addWidget( ButtonSave );
    buttonsLayout->setSpacing( 5 );
    buttonsLayout->setContentsMargins( 0, 0, 0, 0 );

    auto buttonsWidget = new QWidget( this );
    buttonsWidget->setLayout( buttonsLayout );

    gridLayout = new QGridLayout( this );
    gridLayout->addWidget( formWidget,       0, 0, 3, 2 );
    gridLayout->addWidget( separatorLine,    0, 2, 3, 1 );
    gridLayout->addWidget( label_Profiles,   0, 3, 1, 1 );
    gridLayout->addWidget( listWidget,       1, 3, 1, 1 );
    gridLayout->addWidget( buttonsWidget,    2, 3, 1, 1 );
    
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
    listWidget->clear();
    listProjects = GlobalClient->storage->ListProjects();
    for (auto& profile : listProjects) {
        auto* item = new QListWidgetItem(profile.GetProject());
        item->setData(Qt::UserRole, profile.GetUsername());
        item->setData(Qt::UserRole + 1, profile.GetHost());
        listWidget->addItem(item);
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

    auto* newProfile = new AuthProfile(lineEdit_Project->text(), lineEdit_User->text(),
                                      lineEdit_Password->text(), lineEdit_Host->text(),
                                      lineEdit_Port->text(), lineEdit_Endpoint->text(), projectDir);

    if (GlobalClient->storage->ExistsProject(lineEdit_Project->text()))
        GlobalClient->storage->UpdateProject(*newProfile);
    else
        GlobalClient->storage->AddProject(*newProfile);

    return newProfile;
}

void DialogConnect::itemRemove()
{
    auto* item = listWidget->currentItem();
    if (!item)
        return;

    GlobalClient->storage->RemoveProject(item->text());
    delete listWidget->takeItem(listWidget->row(item));
    loadProjects();
}

void DialogConnect::itemSelected()
{
    auto* item = listWidget->currentItem();
    if (!item)
        return;

    const QString project = item->text();
    isNewProject = false;
    projectDirTouched = true;

    auto it = std::find_if(listProjects.begin(), listProjects.end(),
                          [&project](AuthProfile& p) { return p.GetProject() == project; });
    if (it == listProjects.end())
        return;

    lineEdit_Project->setText(it->GetProject());
    lineEdit_ProjectDir->setText(it->GetProjectDir());
    lineEdit_Host->setText(it->GetHost());
    lineEdit_Port->setText(it->GetPort());
    lineEdit_Endpoint->setText(it->GetEndpoint());
    lineEdit_User->setText(it->GetUsername());
    lineEdit_Password->setText(it->GetPassword());
}

void DialogConnect::handleContextMenu(const QPoint &pos ) const
{
    QPoint globalPos = listWidget->mapToGlobal( pos );
    menuContex->exec( globalPos );
}

bool DialogConnect::checkValidInput() const
{
    const auto checkEmpty = [](QLineEdit* edit, const QString& msg) {
        if (edit->text().isEmpty()) {
            MessageError(msg);
            return false;
        }
        return true;
    };

    if (!checkEmpty(lineEdit_Project, "Project is empty")) return false;
    if (!checkEmpty(lineEdit_Host, "Host is empty")) return false;
    if (!checkEmpty(lineEdit_Port, "Port is empty")) return false;
    if (!checkEmpty(lineEdit_Endpoint, "Endpoint is empty")) return false;
    if (!checkEmpty(lineEdit_User, "Username is empty")) return false;
    if (!checkEmpty(lineEdit_Password, "Password is empty")) return false;

    if (!IsValidURI(lineEdit_Endpoint->text())) {
        MessageError("Endpoint is invalid URI (Example: /api/qwe)");
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
    lineEdit_Host->clear();
    lineEdit_Port->clear();
    lineEdit_Endpoint->clear();
    listWidget->clearSelection();
    lineEdit_Project->setFocus();
}

void DialogConnect::onButton_Clear()
{
    isNewProject = true;
    projectDirTouched = false;
    clearFields();
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
            lineEdit_Host->setText(json["host"].toString());
            lineEdit_Port->setText(json["port"].toString());
            lineEdit_Endpoint->setText(json["endpoint"].toString());

            AuthProfile loadedProfile(project, json["username"].toString(), json["password"].toString(),
                                     json["host"].toString(), json["port"].toString(),
                                     json["endpoint"].toString(), projectDirFinal);

            if (isNewProject)
                GlobalClient->storage->AddProject(loadedProfile);
            else
                GlobalClient->storage->UpdateProject(loadedProfile);

            loadProjects();

            for (int i = 0; i < listWidget->count(); ++i) {
                if (listWidget->item(i)->text() == project) {
                    listWidget->setCurrentRow(i);
                    itemSelected();
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

    QJsonObject json;
    json["project"] = projectName;
    json["host"] = lineEdit_Host->text().trimmed();
    json["port"] = lineEdit_Port->text().trimmed();
    json["endpoint"] = lineEdit_Endpoint->text().trimmed();
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