#include <UI/Dialogs/DialogExtender.h>

DialogExtender::DialogExtender(Extender* e)
{
    extender = e;

    this->createUI();

    connect( buttonClose, &QPushButton::clicked, this, &DialogExtender::close);
    connect( table, &QTableWidget::customContextMenuRequested, this, &DialogExtender::handleMenu );
}

DialogExtender::~DialogExtender()  = default;

void DialogExtender::createUI()
{
    this->setWindowTitle("Extender");
    this->resize(1200, 600);

    table = new QTableWidget( this );
    table->setColumnCount( 3 );
    table->setContextMenuPolicy( Qt::CustomContextMenu );
    table->setAutoFillBackground( false );
    table->setShowGrid( false );
    table->setSortingEnabled( true );
    table->setWordWrap( true );
    table->setCornerButtonEnabled( false );
    table->setSelectionBehavior( QAbstractItemView::SelectRows );
    table->setSelectionMode( QAbstractItemView::SingleSelection );
    table->setFocusPolicy( Qt::NoFocus );
    table->setAlternatingRowColors( true );
    table->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    table->horizontalHeader()->setCascadingSectionResizes( true );
    table->horizontalHeader()->setHighlightSections( false );
    table->verticalHeader()->setVisible( false );

    table->setHorizontalHeaderItem( 0, new QTableWidgetItem( "Path" ) );
    table->setHorizontalHeaderItem( 1, new QTableWidgetItem( "Description" ) );
    table->setHorizontalHeaderItem( 2, new QTableWidgetItem( "Status" ) );
    table->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );

    auto hSpacer1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto hSpacer2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    buttonClose = new QPushButton("Close", this);
    buttonClose->setFixedWidth(180);

    layout = new QGridLayout(this);
    layout->setContentsMargins( 4, 4,  4, 4);
    layout->addWidget( table, 0, 0, 1, 3);
    layout->addItem( hSpacer1, 1, 0, 1, 1);
    layout->addWidget( buttonClose, 1, 1, 1, 1);
    layout->addItem( hSpacer2, 1, 2, 1, 1);

    this->setLayout(layout);
}

void DialogExtender::handleMenu(const QPoint &pos ) const
{
    QMenu menu = QMenu();

    menu.addAction("Add new", this, &DialogExtender::onActionAdd );
    menu.addSeparator();
    menu.addAction("Enable", this, &DialogExtender::onActionEnable );
    menu.addAction("Disable", this, &DialogExtender::onActionDisable );
    menu.addSeparator();
    menu.addAction("Remove", this, &DialogExtender::onActionRemove );

    QPoint globalPos = table->mapToGlobal( pos );
    menu.exec(globalPos );
}

void DialogExtender::onActionAdd()
{
    QString filePath = QFileDialog::getOpenFileName( nullptr, "Select file", QDir::homePath(), "*.json");
    if ( filePath.isEmpty())
        return;

    extender->LoadFromFile(filePath);
}

void DialogExtender::onActionEnable()
{

}

void DialogExtender::onActionDisable()
{

}

void DialogExtender::onActionRemove()
{

}