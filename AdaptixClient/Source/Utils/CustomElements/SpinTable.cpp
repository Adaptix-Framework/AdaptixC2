#include <Utils/CustomElements/SpinTable.h>

SpinTable::SpinTable(int rows, int columns, QWidget* parent) : QWidget(parent)
{
    buttonAdd = new QPushButton("Add");

    buttonClear = new QPushButton("Clear");

    tableModel = new QStandardItemModel(rows, columns, this);

    table = new QTableView(this);
    table->setModel(tableModel);
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

    layout = new QGridLayout( this );
    layout->addWidget(table, 0, 0, 1, 2);
    layout->addWidget(buttonAdd, 1, 0, 1, 1);
    layout->addWidget(buttonClear, 1, 1, 1, 1);

    this->setLayout(layout);

    QObject::connect(buttonAdd, &QPushButton::clicked, this, [&]()
    {
        if (tableModel->rowCount() < 1 )
            tableModel->setRowCount(1 );
        else
            tableModel->setRowCount(tableModel->rowCount() + 1 );

        tableModel->setItem(tableModel->rowCount() - 1, 0, new QStandardItem() );
        table->selectRow(tableModel->rowCount() - 1 );
    } );

    QObject::connect(buttonClear, &QPushButton::clicked, this, [&](){ tableModel->setRowCount(0); } );
}
