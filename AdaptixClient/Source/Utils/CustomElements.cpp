#include <Utils/CustomElements.h>

SpinTable::SpinTable(int rows, int columns, QWidget* parent)
{
    this->setParent(parent);

    buttonAdd   = new QPushButton("Add");
    buttonClear = new QPushButton("Clear");

    table = new QTableWidget(rows, columns, this);
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
        if (table->rowCount() < 1 )
            table->setRowCount(1 );
        else
            table->setRowCount(table->rowCount() + 1 );

        table->setItem(table->rowCount() - 1, 0, new QTableWidgetItem() );
        table->selectRow(table->rowCount() - 1 );
    } );

    QObject::connect(buttonClear, &QPushButton::clicked, this, [&]()
    {
        table->setRowCount(0);
    } );
}



FileSelector::FileSelector(QWidget* parent) : QWidget(parent)
{
    input = new QLineEdit(this);
    input->setReadOnly(true);

    button = new QPushButton(this);
    button->setIcon(QIcon::fromTheme("folder"));

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(input);
    layout->addWidget(button);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout);

    connect(button, &QPushButton::clicked, this, [&]()
    {
        QString selectedFile = QFileDialog::getOpenFileName(this, "Select a file");
        if (selectedFile.isEmpty())
            return;

        QString filePath = selectedFile;
        input->setText(filePath);

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return;

        QByteArray fileData = file.readAll();
        file.close();

        content = QString::fromUtf8(fileData.toBase64());
    } );
}
