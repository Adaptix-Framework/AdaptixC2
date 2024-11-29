#include <UI/Widgets/BrowserFilesWidget.h>
#include <Utils/FileSystem.h>

BrowserFilesWidget::BrowserFilesWidget(Agent* a)
{
    agent = a;
    this->createUI();

    connect( buttonDisks, &QPushButton::clicked, this, &BrowserFilesWidget::onDisks);
}

BrowserFilesWidget::~BrowserFilesWidget() = default;

void BrowserFilesWidget::createUI()
{
    buttonReload = new QPushButton(QIcon(":/icons/reload"), "", this);
    buttonReload->setIconSize( QSize( 24,24 ));
    buttonReload->setFixedSize(37, 28);
    buttonReload->setToolTip("Reload");

    buttonHomeDir = new QPushButton( QIcon(":/icons/folder"), "", this);
    buttonHomeDir->setIconSize( QSize( 24,24 ));
    buttonHomeDir->setFixedSize(37, 28);
    buttonHomeDir->setToolTip("Up folder");

    pathInput = new QLineEdit(this);

    buttonCd = new QPushButton(QIcon(":/icons/arrow_right"), "", this);
    buttonCd->setIconSize( QSize( 24,24 ));
    buttonCd->setFixedSize(37, 28);

    line_1 = new QFrame(this);
    line_1->setFrameShape(QFrame::VLine);
    line_1->setMinimumHeight(25);

    buttonDisks = new QPushButton(QIcon(":/icons/storage"), "", this);
    buttonDisks->setIconSize( QSize( 24,24 ));
    buttonDisks->setFixedSize(37, 28);
    buttonDisks->setToolTip("Disks list");

    buttonUpload = new QPushButton(QIcon(":/icons/upload"), "", this);
    buttonUpload->setIconSize( QSize( 24,24 ));
    buttonUpload->setFixedSize(37, 28);
    buttonUpload->setToolTip("Upload File");

    line_2 = new QFrame(this);
    line_2->setFrameShape(QFrame::VLine);
    line_2->setMinimumHeight(25);

    statusLabel = new QLabel(this);
    statusLabel->setText("Status: ");

    tableWidget = new QTableWidget(this );
    tableWidget->setColumnCount(3);
    tableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    tableWidget->setAutoFillBackground( false );
    tableWidget->setShowGrid( false );
    tableWidget->setSortingEnabled( false );
    tableWidget->setWordWrap( true );
    tableWidget->setCornerButtonEnabled( true );
    tableWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableWidget->setFocusPolicy( Qt::NoFocus );
    tableWidget->setAlternatingRowColors( true );
    tableWidget->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableWidget->horizontalHeader()->setCascadingSectionResizes( true );
    tableWidget->horizontalHeader()->setHighlightSections( false );
    tableWidget->verticalHeader()->setVisible( false );

    tableWidget->setHorizontalHeaderItem( 0, new QTableWidgetItem( "Name" ) );
    tableWidget->setHorizontalHeaderItem( 1, new QTableWidgetItem( "Size" ) );
    tableWidget->setHorizontalHeaderItem( 2, new QTableWidgetItem( "Last Modified" ) );

    listGridLayout = new QGridLayout(this);
    listGridLayout->setContentsMargins(5, 4, 1, 1);
    listGridLayout->setVerticalSpacing(4);
    listGridLayout->setHorizontalSpacing(8);

    listGridLayout->addWidget( buttonReload,  0, 0,  1, 1  );
    listGridLayout->addWidget( buttonHomeDir, 0, 1,  1, 1  );
    listGridLayout->addWidget( pathInput,     0, 2,  1, 5  );
    listGridLayout->addWidget( buttonCd,      0, 7,  1, 1  );
    listGridLayout->addWidget( line_1,        0, 8,  1, 1  );
    listGridLayout->addWidget( buttonUpload,  0, 9,  1, 1  );
    listGridLayout->addWidget( buttonDisks,   0, 10, 1, 1  );
    listGridLayout->addWidget( line_2,        0, 11, 1, 1  );
    listGridLayout->addWidget( statusLabel,   0, 12, 1, 1  );
    listGridLayout->addWidget( tableWidget,   1, 0,  1, 13 );

    listBrowserWidget = new QWidget(this);
    listBrowserWidget->setLayout(listGridLayout);

    treeBrowserWidget = new QTreeWidget();
    treeBrowserWidget->setSortingEnabled(false);
    treeBrowserWidget->headerItem()->setText( 0, "Directory Tree" );

    splitter = new QSplitter( this );
    splitter->setOrientation( Qt::Horizontal );
    splitter->addWidget( treeBrowserWidget );
    splitter->addWidget( listBrowserWidget );
    splitter->setSizes( QList<int>() << 1 << 250 );

    mainGridLayout = new QGridLayout(this );
    mainGridLayout->setContentsMargins(0, 0, 0, 0 );
    mainGridLayout->addWidget(splitter, 0, 0, 1, 1 );

    this->setLayout(mainGridLayout);
}

void BrowserFilesWidget::SetDisks(qint64 time, int msgType, QString message, QString data)
{
    QString sTime  = UnixTimestampGlobalToStringLocal(time);
    QString status = "";
    if( msgType == CONSOLE_OUT_LOCAL_ERROR || msgType == CONSOLE_OUT_ERROR ) {
        status = TextColorHtml(message, COLOR_Berry) + " >> " + sTime;
    }
    else {
        status = TextColorHtml(message, COLOR_NeonGreen) + " >> " + sTime;
    }

    statusLabel->setText(status);

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data.toUtf8());
    if (!jsonDoc.isArray())
        return;

    QJsonArray jsonArray = jsonDoc.array();
    for (const QJsonValue& value : jsonArray) {
        QJsonObject jsonObject = value.toObject();
        BrowserFileData fileData;
        fileData.Path     = jsonObject["b_name"].toString();
        fileData.Type     = "Disk";
        fileData.Modified = jsonObject["b_type"].toString();
        this->TreeAddData(fileData);
    }
}

void BrowserFilesWidget::TreeAddData(BrowserFileData data)
{
    QString rootPath = GetRootPathWindows(data.Path);
    QTreeWidgetItem* rootItem = nullptr;
    for (int i = 0; i < treeBrowserWidget->topLevelItemCount(); ++i) {
        auto item = treeBrowserWidget->topLevelItem(i);
        if (item->text(0) == rootPath) {
            rootItem = item;
            break;
        }
    }

    if (!rootItem) {
        rootItem = new QTreeWidgetItem(treeBrowserWidget);
        rootItem->setText(0, rootPath);
        rootItem->setIcon(0, QIcon(":/icons/ssd"));
        treeBrowserWidget->addTopLevelItem(rootItem);
    }
}

/// SLOTS

void BrowserFilesWidget::onDisks()
{
    QString status = agent->BrowserDisks();
    statusLabel->setText(status);
}