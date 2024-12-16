#include <UI/Dialogs/DialogExtender.h>

DialogExtender::DialogExtender()
{
    this->createUI();

    connect( buttonClose,  &QPushButton::clicked, this, &DialogExtender::close);
    connect( table, &QTableWidget::customContextMenuRequested, this, &DialogExtender::handleMenu );
}

DialogExtender::~DialogExtender()  = default;

void DialogExtender::createUI()
{
    this->setWindowTitle("Extender");
    this->resize(1200, 600);

    table = new QTableWidget( this );
    table->setColumnCount( 4 );
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

    table->setHorizontalHeaderItem( 0, new QTableWidgetItem( "Type" ) );
    table->setHorizontalHeaderItem( 1, new QTableWidgetItem( "Path" ) );
    table->setHorizontalHeaderItem( 2, new QTableWidgetItem( "Description" ) );
    table->setHorizontalHeaderItem( 3, new QTableWidgetItem( "Status" ) );

    hSpacer     = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    buttonClose = new QPushButton("Close", this);

    layout = new QGridLayout(this);
    layout->setContentsMargins( 0, 0,  0, 0);
    layout->addWidget( table, 0, 0, 1, 2);
    layout->addItem( hSpacer, 1, 0, 1, 1);
    layout->addWidget( buttonClose, 1, 1, 1, 1);

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
    QString filePath = QFileDialog::getOpenFileName( nullptr, "Select file", QDir::homePath());
    if ( filePath.isEmpty())
        return;
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