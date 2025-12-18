#include <UI/Dialogs/DialogConnect.h>
#include <Client/AuthProfile.h>
#include <Client/Storage.h>
#include <MainAdaptix.h>

DialogConnect::DialogConnect()
{
    this->createUI();

    this->loadProjects();

    connect( tableWidget, &QTableWidget::itemPressed, this, &DialogConnect::itemSelected );
    connect( tableWidget, &QTableWidget::customContextMenuRequested, this, &DialogConnect::handleContextMenu );

    connect( lineEdit_Project,  &QLineEdit::returnPressed, this, &DialogConnect::onButton_Connect );
    connect( lineEdit_Host,     &QLineEdit::returnPressed, this, &DialogConnect::onButton_Connect );
    connect( lineEdit_Port,     &QLineEdit::returnPressed, this, &DialogConnect::onButton_Connect );
    connect( lineEdit_Endpoint, &QLineEdit::returnPressed, this, &DialogConnect::onButton_Connect );
    connect( lineEdit_User,     &QLineEdit::returnPressed, this, &DialogConnect::onButton_Connect );
    connect( lineEdit_Password, &QLineEdit::returnPressed, this, &DialogConnect::onButton_Connect );
    connect( ButtonConnect,     &QPushButton::clicked,     this, &DialogConnect::onButton_Connect );
    connect( ButtonClear,       &QPushButton::clicked,     this, &DialogConnect::onButton_Clear );

    if (lineEdit_Project) {
        connect(lineEdit_Project, &QLineEdit::textChanged,
                this, &DialogConnect::onProjectNameChanged);
    }
    if (lineEdit_ProjectDir) {
        connect(lineEdit_ProjectDir, &QLineEdit::textEdited,
                this, &DialogConnect::onProjectDirEdited);

        auto action = lineEdit_ProjectDir->addAction(QIcon(":/icons/folder"),
                                                     QLineEdit::TrailingPosition);
        connect(action, &QAction::triggered,
                this, &DialogConnect::onSelectProjectDir);
    }
}

DialogConnect::~DialogConnect() = default;

void DialogConnect::createUI()
{
    resize( 800, 370 );
    setMinimumSize( QSize( 800, 370 ) );
    setMaximumSize( QSize( 800, 370 ) );

    this->setWindowTitle("Connect");
    this->setProperty("Main", "base");

    label_UserInfo = new QLabel( this );
    label_UserInfo->setAlignment(Qt::AlignCenter);
    label_UserInfo->setText( "User information" );

    label_User = new QLabel( this );
    label_User->setText( "User:" );

    label_Password = new QLabel( this );
    label_Password->setText( "Password:" );

    label_ServerDetails = new QLabel( this );
    label_ServerDetails->setAlignment(Qt::AlignCenter);
    label_ServerDetails->setText( "Server details" );

    label_Project = new QLabel( this );
    label_Project->setText( "Project:" );

    label_ProjectDir = new QLabel( this );
    label_ProjectDir->setText( "Project directory:" );

    label_Host = new QLabel( this );
    label_Host->setText( "Host:" );

    label_Port = new QLabel( this );
    label_Port->setText( "Port:" );

    label_Endpoint = new QLabel( this );
    label_Endpoint->setText( "Endpoint:" );


    lineEdit_User = new QLineEdit( this );

    lineEdit_Password = new QLineEdit( this );
    lineEdit_Password->setEchoMode( QLineEdit::Password );

    lineEdit_Project = new QLineEdit( this );

    lineEdit_ProjectDir = new QLineEdit( this );

    lineEdit_Host = new QLineEdit( this );

    lineEdit_Port = new QLineEdit( this );
    lineEdit_Port->setValidator( new QRegularExpressionValidator( QRegularExpression( "[0-9]*" ), lineEdit_Port ) );

    lineEdit_Endpoint = new QLineEdit( this );


    ButtonConnect = new QPushButton(this);
    ButtonConnect->setProperty("ButtonStyle", "dialog");
    ButtonConnect->setText("Connect");
    ButtonConnect->setFocus();

    ButtonClear = new QPushButton(this);
    ButtonClear->setProperty("ButtonStyle", "dialog");
    ButtonClear->setText("Clear");


    menuContex = new QMenu( this );
    menuContex->addAction( "Remove", this, &DialogConnect::itemRemove );


    tableWidget = new QTableWidget(this);
    tableWidget->setColumnCount( 3 );
    tableWidget->setMinimumSize(QSize( 440, 0) );
    tableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    tableWidget->addAction( menuContex->menuAction() );
    tableWidget->setAutoFillBackground( false );
    tableWidget->setShowGrid( false );
    tableWidget->setSortingEnabled( true );
    tableWidget->setWordWrap( true );
    tableWidget->setCornerButtonEnabled( false );
    tableWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableWidget->setSelectionMode( QAbstractItemView::SingleSelection );
    tableWidget->setFocusPolicy( Qt::NoFocus );
    tableWidget->setAlternatingRowColors( true );
    tableWidget->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableWidget->horizontalHeader()->setCascadingSectionResizes( true );
    tableWidget->horizontalHeader()->setHighlightSections( false );
    tableWidget->verticalHeader()->setVisible( false );

    tableWidget->setHorizontalHeaderItem( 0, new QTableWidgetItem( "Username" ) );
    tableWidget->setHorizontalHeaderItem( 1, new QTableWidgetItem( "Project" ) );
    tableWidget->setHorizontalHeaderItem( 2, new QTableWidgetItem( "Host" ) );

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

    formLayout->addWidget( ButtonClear,        10, 0, 1, 1 );
    formLayout->addWidget( ButtonConnect,      10, 1, 1, 1 );
    formLayout->setRowStretch(11, 1);

    gridLayout = new QGridLayout( this );
    gridLayout->addWidget( formWidget,  0, 0, 11, 2 );
    gridLayout->addWidget( tableWidget, 0, 2, 11, 1 );
}

void DialogConnect::loadProjects()
{
    this->listProjects = GlobalClient->storage->ListProjects();
    for ( auto profile : this->listProjects ) {
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

        auto item_Username = new QTableWidgetItem();
        auto item_Project  = new QTableWidgetItem();
        auto item_Host     = new QTableWidgetItem();

        item_Username->setText( profile.GetUsername() );
        item_Username->setFlags( item_Username->flags() ^ Qt::ItemIsEditable );

        item_Project->setText( profile.GetProject() );
        item_Project->setFlags( item_Project->flags() ^ Qt::ItemIsEditable );

        item_Host->setText( profile.GetHost() );
        item_Host->setFlags( item_Host->flags() ^ Qt::ItemIsEditable );

        tableWidget->setItem( tableWidget->rowCount() - 1, 0, item_Username );
        tableWidget->setItem( tableWidget->rowCount() - 1, 1, item_Project );
        tableWidget->setItem( tableWidget->rowCount() - 1, 2, item_Host );
    }
}

AuthProfile* DialogConnect::StartDialog()
{
    this->toConnect = false;
    this->exec();
    if( this->toConnect ) {
        QString projectDir = lineEdit_ProjectDir ? lineEdit_ProjectDir->text().trimmed() : QString();
        if (projectDir.isEmpty()) {
            QDir home(QDir::homePath());
            QString basePath = home.filePath("AdaptixProjects");
            QDir baseDir(basePath);
            projectDir = baseDir.filePath(lineEdit_Project->text().trimmed());
        }

        AuthProfile* newProfile = new AuthProfile(
                    lineEdit_Project->text(),
                    lineEdit_User->text(),
                    lineEdit_Password->text(),
                    lineEdit_Host->text(),
                    lineEdit_Port->text(),
                    lineEdit_Endpoint->text(),
                    projectDir);

        if( GlobalClient->storage->ExistsProject(lineEdit_Project->text()) ) {
            GlobalClient->storage->UpdateProject(*newProfile);
        } else {
            GlobalClient->storage->AddProject(*newProfile);
        }

        return newProfile;
    }
    return NULL;
}

void DialogConnect::itemRemove() const
{
    if( tableWidget->selectedItems().count() == 0 )
        return;

    QString Project = tableWidget->item(tableWidget->currentRow(), 1)->text();
    GlobalClient->storage->RemoveProject(Project);
    tableWidget->removeRow(tableWidget->currentRow());
}

void DialogConnect::itemSelected()
{
    QString project = tableWidget->item(tableWidget->currentRow(), 1)->text();
    this->isNewProject = false;
    this->projectDirTouched = true;
    for ( auto profile : this->listProjects ) {
        if ( profile.GetProject() == project ) {
            lineEdit_Project->setText( profile.GetProject() );
            if (lineEdit_ProjectDir)
                lineEdit_ProjectDir->setText( profile.GetProjectDir() );
            lineEdit_Host->setText( profile.GetHost() );
            lineEdit_Port->setText( profile.GetPort() );
            lineEdit_Endpoint->setText( profile.GetEndpoint() );
            lineEdit_User->setText( profile.GetUsername() );
            lineEdit_Password->setText( profile.GetPassword() );
        }
    }
}

void DialogConnect::handleContextMenu(const QPoint &pos ) const
{
    QPoint globalPos = tableWidget->mapToGlobal( pos );
    menuContex->exec( globalPos );
}

bool DialogConnect::checkValidInput() const
{
    if ( lineEdit_Project->text().isEmpty() ) {
        MessageError("Project is empty");
        return false;
    }
    if ( lineEdit_Host->text().isEmpty() ) {
        MessageError("Host is empty");
        return false;
    }
    if ( lineEdit_Port->text().isEmpty()  ) {
        MessageError("Port is empty");
        return false;
    }
    if ( lineEdit_Endpoint->text().isEmpty()  ) {
        MessageError("Endpoint is empty");
        return false;
    }
    if ( !IsValidURI(lineEdit_Endpoint->text()) ) {
        MessageError("Endpoint is invalid URI (Example: /api/qwe)");
        return false;
    }
    if ( lineEdit_User->text().isEmpty() ) {
        MessageError("Username is empty");
        return false;
    }
    if ( lineEdit_Password->text().isEmpty() ) {
        MessageError("Password is empty");
        return false;
    }

    if( GlobalClient->storage->ExistsProject(lineEdit_Project->text()) && isNewProject ) {
        MessageError("Project already exists");
        return false;
    }

    return true;
}

void DialogConnect::onButton_Connect()
{
    if(this->checkValidInput() ) {
        this->toConnect = true;
        close();
    }
}

void DialogConnect::onButton_Clear()
{
    if (lineEdit_User)        lineEdit_User->clear();
    if (lineEdit_Password)    lineEdit_Password->clear();
    if (lineEdit_Project)     lineEdit_Project->clear();
    if (lineEdit_ProjectDir)  lineEdit_ProjectDir->clear();
    if (lineEdit_Host)        lineEdit_Host->clear();
    if (lineEdit_Port)        lineEdit_Port->clear();
    if (lineEdit_Endpoint)    lineEdit_Endpoint->clear();

    if (tableWidget)
        tableWidget->clearSelection();

    isNewProject = true;
    projectDirTouched = false;

    if (lineEdit_Project)
        lineEdit_Project->setFocus();
}

void DialogConnect::onProjectNameChanged(const QString &text)
{
    if (!lineEdit_ProjectDir)
        return;

    // Не трогаем путь, если пользователь уже его правил вручную
    if (projectDirTouched)
        return;

    const QString name = text.trimmed();
    if (name.isEmpty()) {
        lineEdit_ProjectDir->clear();
        return;
    }

    QDir home(QDir::homePath());
    QString basePath = home.filePath("AdaptixProjects");
    QDir baseDir(basePath);
    const QString newDir = baseDir.filePath(name);

    lineEdit_ProjectDir->setText(newDir);
}

void DialogConnect::onProjectDirEdited(const QString &)
{
    projectDirTouched = true;
}

void DialogConnect::onSelectProjectDir()
{
    if (!lineEdit_ProjectDir)
        return;

    QString current = lineEdit_ProjectDir->text().trimmed();
    if (current.isEmpty()) {
        QDir home(QDir::homePath());
        QString basePath = home.filePath("AdaptixProjects");
        QDir baseDir(basePath);
        current = baseDir.filePath(lineEdit_Project ? lineEdit_Project->text().trimmed() : QString());
    }

    QString dir = QFileDialog::getExistingDirectory(this, "Select project directory", current);
    if (dir.isEmpty())
        return;

    lineEdit_ProjectDir->setText(dir);
    projectDirTouched = true;
}
