#include <UI/Widgets/ScreenshotsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>

ImageFrame::ImageFrame(QWidget* parent)
    : QWidget(parent),
      label(new QLabel),
      scrollArea(new QScrollArea(this)),
      ctrlPressed(false),
      scaleFactor(1.0)
{
    setFocusPolicy(Qt::StrongFocus);

    label->setBackgroundRole(QPalette::Base);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    label->setScaledContents(true);

    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(label);
    scrollArea->viewport()->installEventFilter(this);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(scrollArea);
    setLayout(layout);
}

void ImageFrame::setPixmap(const QPixmap& pix)
{
    originalPixmap = pix;
    label->setPixmap(originalPixmap);
    scaleFactor = 1.0;
    resizeImage();
}

void ImageFrame::resizeImage() const
{
    if (!originalPixmap.isNull()) {
        label->resize(scaleFactor * originalPixmap.size());
    }
}

void ImageFrame::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    resizeImage();
}

QPixmap ImageFrame::pixmap() const
{
    return originalPixmap;
}

void ImageFrame::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Control)
        ctrlPressed = true;
    QWidget::keyPressEvent(e);
}

void ImageFrame::keyReleaseEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Control)
        ctrlPressed = false;
    QWidget::keyReleaseEvent(e);
}

bool ImageFrame::eventFilter(QObject* obj, QEvent* e)
{
    if (obj == scrollArea->viewport() && e->type() == QEvent::Wheel) {
        auto we = static_cast<QWheelEvent*>(e);
        if (ctrlPressed) {
            const double step = (we->angleDelta().y() > 0) ? 1.1 : 0.9;
            scaleFactor *= step;
            scaleFactor = std::clamp(scaleFactor, 0.3, 4.0);
            resizeImage();
            return true;
        }
    }
    return QWidget::eventFilter(obj, e);
}

void ImageFrame::clear()
{
    originalPixmap = QPixmap();
    label->setPixmap(QPixmap());
    scaleFactor = 1.0;
    resizeImage();
}





ScreenshotsWidget::ScreenshotsWidget(QWidget* w)
{
    this->mainWidget = w;
    this->createUI();

    connect(tableWidget, &QTableWidget::customContextMenuRequested, this, &ScreenshotsWidget::handleScreenshotsMenu);
    connect(tableWidget->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &ScreenshotsWidget::onTableItemSelection);
    connect(tableWidget, &QTableWidget::itemSelectionChanged, this, [this](){tableWidget->setFocus();});
    connect(splitter, &QSplitter::splitterMoved, imageFrame, &ImageFrame::resizeImage);
}

ScreenshotsWidget::~ScreenshotsWidget() = default;

void ScreenshotsWidget::createUI()
{
    tableWidget = new QTableWidget(this );
    tableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    tableWidget->setAutoFillBackground( false );
    tableWidget->setShowGrid( false );
    tableWidget->setSortingEnabled( true );
    tableWidget->setWordWrap( true );
    tableWidget->setCornerButtonEnabled( true );
    tableWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableWidget->setFocusPolicy( Qt::NoFocus );
    tableWidget->setAlternatingRowColors( true );
    tableWidget->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableWidget->horizontalHeader()->setCascadingSectionResizes( true );
    tableWidget->horizontalHeader()->setHighlightSections( false );
    tableWidget->verticalHeader()->setVisible( false );
    tableWidget->setColumnCount(5);

    tableWidget->setHorizontalHeaderItem( 0, new QTableWidgetItem( "ID" ) );
    tableWidget->setHorizontalHeaderItem( 1, new QTableWidgetItem( "User" ) );
    tableWidget->setHorizontalHeaderItem( 2, new QTableWidgetItem( "Computer" ) );
    tableWidget->setHorizontalHeaderItem( 3, new QTableWidgetItem( "Note" ) );
    tableWidget->setHorizontalHeaderItem( 4, new QTableWidgetItem( "Date" ) );
    tableWidget->hideColumn( 0 );

    imageFrame = new ImageFrame(this);

    splitter = new QSplitter(this);
    splitter->setOrientation(Qt::Horizontal);
    splitter->addWidget(tableWidget);
    splitter->addWidget(imageFrame);
    splitter->setSizes(QList<int>() << 80 << 200);

    mainGridLayout = new QGridLayout(this );
    mainGridLayout->setContentsMargins(0, 0, 0, 0 );
    mainGridLayout->addWidget(splitter, 0, 0, 1, 1 );
}

void ScreenshotsWidget::Clear() const
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if (!adaptixWidget)
        return;

    adaptixWidget->Screenshots.clear();

    QSignalBlocker blocker(tableWidget->selectionModel());

    for (int index = tableWidget->rowCount(); index > 0; index-- )
        tableWidget->removeRow(index -1 );

    imageFrame->clear();

}

void ScreenshotsWidget::AddScreenshotItem(const ScreenData &newScreen) const
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>(mainWidget);
    if(adaptixWidget->Screenshots.contains(newScreen.ScreenId))
        return;

    if( tableWidget->rowCount() < 1 )
        tableWidget->setRowCount( 1 );
    else
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

    auto item_ScreenId = new QTableWidgetItem( newScreen.ScreenId );
    auto item_User     = new QTableWidgetItem( newScreen.User );
    auto item_Computer = new QTableWidgetItem( newScreen.Computer );
    auto item_Note     = new QTableWidgetItem( newScreen.Note );
    auto item_Date     = new QTableWidgetItem( newScreen.Date );

    item_ScreenId->setTextAlignment( Qt::AlignCenter );
    item_ScreenId->setFlags( item_ScreenId->flags() ^ Qt::ItemIsEditable );

    item_User->setTextAlignment( Qt::AlignCenter );
    item_User->setFlags( item_User->flags() ^ Qt::ItemIsEditable );

    item_Computer->setTextAlignment( Qt::AlignCenter );
    item_Computer->setFlags( item_Computer->flags() ^ Qt::ItemIsEditable );

    item_Note->setTextAlignment( Qt::AlignLeft | Qt::AlignVCenter );
    item_Note->setFlags( item_Note->flags() ^ Qt::ItemIsEditable );

    item_Date->setTextAlignment( Qt::AlignCenter );
    item_Date->setFlags( item_Date->flags() ^ Qt::ItemIsEditable );

    bool isSortingEnabled = tableWidget->isSortingEnabled();
    tableWidget->setSortingEnabled( false );
    tableWidget->setItem( tableWidget->rowCount() - 1, 0, item_ScreenId );
    tableWidget->setItem( tableWidget->rowCount() - 1, 1, item_User );
    tableWidget->setItem( tableWidget->rowCount() - 1, 2, item_Computer );
    tableWidget->setItem( tableWidget->rowCount() - 1, 3, item_Note );
    tableWidget->setItem( tableWidget->rowCount() - 1, 4, item_Date );
    tableWidget->setSortingEnabled( isSortingEnabled );

    tableWidget->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );

    // tableWidget->setItemDelegate(new PaddingDelegate(tableWidget));
    tableWidget->verticalHeader()->setSectionResizeMode(tableWidget->rowCount() - 1, QHeaderView::ResizeToContents);

    adaptixWidget->Screenshots[newScreen.ScreenId] = newScreen;
}

void ScreenshotsWidget::EditScreenshotItem(const QString &screenId, const QString &note) const
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if (!adaptixWidget)
        return;

    if (!adaptixWidget->Screenshots.contains(screenId))
        return;

    adaptixWidget->Screenshots[screenId].Note = note;

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 0);
        if ( item && item->text() == screenId ) {
            tableWidget->item(row, 3)->setText(note);
            break;
        }
    }
}

void ScreenshotsWidget::RemoveScreenshotItem(const QString &screenId) const
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>(mainWidget);
    if (!adaptixWidget)
        return;

    if (!adaptixWidget->Screenshots.contains(screenId))
        return;

    adaptixWidget->Screenshots.remove(screenId);

    for ( int row = 0; row < tableWidget->rowCount(); row++ ) {
        if ( tableWidget->item( row, 0 )->text() == screenId ) {
            tableWidget->removeRow( row );
            break;
        }
    }

    if (tableWidget->rowCount() == 0)
        imageFrame->clear();
}

/// SLOTS

void ScreenshotsWidget::handleScreenshotsMenu(const QPoint &pos )
{
    if ( !tableWidget->itemAt(pos) )
        return;

    auto ctxMenu = QMenu();
    ctxMenu.addAction("Set note", this, &ScreenshotsWidget::actionNote );
    ctxMenu.addAction("Download", this, &ScreenshotsWidget::actionDownload );
    ctxMenu.addAction("Delete",   this, &ScreenshotsWidget::actionDelete );

    ctxMenu.exec(tableWidget->horizontalHeader()->viewport()->mapToGlobal(pos ) );
}

void ScreenshotsWidget::actionNote() const
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if ( !adaptixWidget )
        return;

    QStringList listId;
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 0)->isSelected() ) {
            QString screenId = tableWidget->item(rowIndex, 0)->text();
            listId.append(screenId);
        }
    }

    if(listId.empty())
        return;

    QString note = "";
    if(listId.size() == 1)
        note = tableWidget->item(tableWidget->currentRow(), 3)->text();

    bool inputOk;
    QString newNote = QInputDialog::getText(nullptr, "Set note", "New note", QLineEdit::Normal,note, &inputOk);
    if ( inputOk ) {
        QString message = QString();
        bool ok = false;
        bool result = HttpReqScreenSetNote(listId, newNote, *(adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("Response timeout");
            return;
        }
    }
}

void ScreenshotsWidget::actionDownload() const
{
    if (tableWidget->selectionModel()->selectedRows().empty())
        return;

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if ( !adaptixWidget )
        return;

    QString screenId = tableWidget->item( tableWidget->currentRow(), 0 )->text();
    if (!adaptixWidget->Screenshots.contains(screenId))
        return;

    ScreenData screenData = adaptixWidget->Screenshots[screenId];

    QString filePath = QFileDialog::getSaveFileName( nullptr, "Save File", "screenshot.png", "All Files (*.*)" );
    if ( filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        MessageError("Failed to open file for writing");
        return;
    }

    file.write( screenData.Content );
    file.close();

    QInputDialog inputDialog;
    inputDialog.setWindowTitle("Sync file");
    inputDialog.setLabelText("File saved to:");
    inputDialog.setTextEchoMode(QLineEdit::Normal);
    inputDialog.setTextValue(filePath);
    inputDialog.adjustSize();
    inputDialog.move(QGuiApplication::primaryScreen()->geometry().center() - inputDialog.geometry().center());
    inputDialog.exec();

}

void ScreenshotsWidget::actionDelete() const
{
    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if ( !adaptixWidget )
        return;

    QStringList listId;
    for( int rowIndex = 0 ; rowIndex < tableWidget->rowCount() ; rowIndex++ ) {
        if ( tableWidget->item(rowIndex, 0)->isSelected() ) {
            QString screenId = tableWidget->item(rowIndex, 0)->text();
            listId.append(screenId);
        }
    }

    if(listId.empty())
        return;

    QString message = QString();
    bool ok = false;
    bool result = HttpReqScreenRemove(listId, *(adaptixWidget->GetProfile()), &message, &ok);
    if( !result ) {
        MessageError("Response timeout");
        return;
    }
}

void ScreenshotsWidget::onTableItemSelection(const QModelIndex &current, const QModelIndex &previous) const
{
    int row = current.row();
    if (row < 0)
        return;

    QString screenId = tableWidget->item(row,0)->text();

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if (!adaptixWidget || !adaptixWidget->Screenshots.contains(screenId) )
        return;

    ScreenData screenData = adaptixWidget->Screenshots[screenId];
    auto image = QPixmap();
    if (image.loadFromData(screenData.Content))
        imageFrame->setPixmap(image);
}