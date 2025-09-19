#include <Agent/Agent.h>
#include <UI/Widgets/DownloadsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Dialogs/DialogDownloader.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Utils/NonBlockingDialogs.h>


DownloadsWidget::DownloadsWidget(AdaptixWidget* w)
{
    this->adaptixWidget = w;
    this->createUI();

    connect( tableWidget, &QTableWidget::customContextMenuRequested, this, &DownloadsWidget::handleDownloadsMenu );
}

void DownloadsWidget::createUI()
{
    tableWidget = new QTableWidget(this );
    tableWidget->setColumnCount(10 );
    tableWidget->setContextMenuPolicy( Qt::CustomContextMenu );
    tableWidget->setAutoFillBackground( false );
    tableWidget->setShowGrid( false );
    tableWidget->setSortingEnabled( true );
    tableWidget->setWordWrap( true );
    tableWidget->setCornerButtonEnabled( true );
    tableWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
    tableWidget->setSelectionMode( QAbstractItemView::SingleSelection );
    tableWidget->setFocusPolicy( Qt::NoFocus );
    tableWidget->setAlternatingRowColors( true );
    tableWidget->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    tableWidget->horizontalHeader()->setCascadingSectionResizes( true );
    tableWidget->horizontalHeader()->setHighlightSections( false );
    tableWidget->verticalHeader()->setVisible( false );

    tableWidget->setHorizontalHeaderItem( 0, new QTableWidgetItem( "File ID" ) );
    tableWidget->setHorizontalHeaderItem( 1, new QTableWidgetItem( "Agent Type" ) );
    tableWidget->setHorizontalHeaderItem( 2, new QTableWidgetItem( "Agent ID" ) );
    tableWidget->setHorizontalHeaderItem( 3, new QTableWidgetItem( "User" ) );
    tableWidget->setHorizontalHeaderItem( 4, new QTableWidgetItem( "Computer" ) );
    tableWidget->setHorizontalHeaderItem( 5, new QTableWidgetItem( "File" ) );
    tableWidget->setHorizontalHeaderItem( 6, new QTableWidgetItem( "Date" ) );
    tableWidget->setHorizontalHeaderItem( 7, new QTableWidgetItem( "Size" ) );
    tableWidget->setHorizontalHeaderItem( 8, new QTableWidgetItem( "Received" ) );
    tableWidget->setHorizontalHeaderItem( 9, new QTableWidgetItem( "Progress" ) );
    tableWidget->hideColumn( 0 );

    mainGridLayout = new QGridLayout(this );
    mainGridLayout->setContentsMargins(0, 0, 0, 0 );
    mainGridLayout->addWidget(tableWidget, 0, 0, 1, 4 );
}

DownloadsWidget::~DownloadsWidget() = default;

void DownloadsWidget::Clear() const
{
    adaptixWidget->Downloads.clear();
    for (int index = tableWidget->rowCount(); index > 0; index-- )
        tableWidget->removeRow(index -1 );
}

void DownloadsWidget::AddDownloadItem(const DownloadData &newDownload )
{
    if ( adaptixWidget->Downloads.contains(newDownload.FileId) )
        return;

    if( tableWidget->rowCount() < 1 )
        tableWidget->setRowCount( 1 );
    else
        tableWidget->setRowCount( tableWidget->rowCount() + 1 );

    auto item_FileID    = new QTableWidgetItem( newDownload.FileId );
    auto item_AgentName = new QTableWidgetItem( newDownload.AgentName );
    auto item_AgentID   = new QTableWidgetItem( newDownload.AgentId );
    auto item_User      = new QTableWidgetItem( newDownload.User );
    auto item_Computer  = new QTableWidgetItem( newDownload.Computer );
    auto item_File      = new QTableWidgetItem( newDownload.Filename );
    auto item_Date      = new QTableWidgetItem( newDownload.Date );
    auto item_Size      = new QTableWidgetItem( BytesToFormat(newDownload.TotalSize) );
    auto item_Received  = new QTableWidgetItem( BytesToFormat(newDownload.RecvSize) );

    item_FileID->setTextAlignment( Qt::AlignCenter );
    item_FileID->setFlags( item_FileID->flags() ^ Qt::ItemIsEditable );

    item_AgentName->setTextAlignment( Qt::AlignCenter );
    item_AgentName->setFlags( item_AgentName->flags() ^ Qt::ItemIsEditable );

    item_AgentID->setTextAlignment( Qt::AlignCenter );
    item_AgentID->setFlags( item_AgentID->flags() ^ Qt::ItemIsEditable );

    item_User->setTextAlignment( Qt::AlignCenter );
    item_User->setFlags( item_User->flags() ^ Qt::ItemIsEditable );

    item_Computer->setTextAlignment( Qt::AlignCenter );
    item_Computer->setFlags( item_Computer->flags() ^ Qt::ItemIsEditable );

    item_File->setTextAlignment( Qt::AlignCenter );
    item_File->setFlags( item_File->flags() ^ Qt::ItemIsEditable );
    item_File->setToolTip(item_File->text());

    item_Date->setTextAlignment( Qt::AlignCenter );
    item_Date->setFlags( item_Date->flags() ^ Qt::ItemIsEditable );

    item_Size->setTextAlignment( Qt::AlignCenter );
    item_Size->setFlags( item_Size->flags() ^ Qt::ItemIsEditable );

    item_Received->setTextAlignment( Qt::AlignCenter );
    item_Received->setFlags( item_Received->flags() ^ Qt::ItemIsEditable );

    QProgressBar *pgbar = new QProgressBar(this);
    pgbar->setMinimum(0);
    pgbar->setMaximum(newDownload.TotalSize);
    pgbar->setValue(newDownload.RecvSize);
    int hSize = tableWidget->rowHeight(tableWidget->rowCount()-1);
    pgbar->setMinimumSize(QSize(200,hSize));
    pgbar->setMaximumSize(QSize(200,hSize));

    bool isSortingEnabled = tableWidget->isSortingEnabled();
    tableWidget->setSortingEnabled( false );
    tableWidget->setItem( tableWidget->rowCount() - 1, 0, item_FileID );
    tableWidget->setItem( tableWidget->rowCount() - 1, 1, item_AgentName );
    tableWidget->setItem( tableWidget->rowCount() - 1, 2, item_AgentID );
    tableWidget->setItem( tableWidget->rowCount() - 1, 3, item_User );
    tableWidget->setItem( tableWidget->rowCount() - 1, 4, item_Computer );
    tableWidget->setItem( tableWidget->rowCount() - 1, 5, item_File );
    tableWidget->setItem( tableWidget->rowCount() - 1, 6, item_Date );
    tableWidget->setItem( tableWidget->rowCount() - 1, 7, item_Size );
    tableWidget->setItem( tableWidget->rowCount() - 1, 8, item_Received );
    tableWidget->setCellWidget( tableWidget->rowCount() - 1, 9, pgbar );
    tableWidget->setSortingEnabled( isSortingEnabled );

    if( newDownload.State == DOWNLOAD_STATE_STOPPED )
        pgbar->setEnabled(false);

    tableWidget->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 4, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 6, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 7, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 8, QHeaderView::ResizeToContents );
    tableWidget->horizontalHeader()->setSectionResizeMode( 9, QHeaderView::ResizeToContents );

    tableWidget->verticalHeader()->setSectionResizeMode(tableWidget->rowCount() - 1, QHeaderView::ResizeToContents);

    adaptixWidget->Downloads[newDownload.FileId] = newDownload;
}

void DownloadsWidget::EditDownloadItem(const QString &fileId, const int recvSize, const int state) const
{
    adaptixWidget->Downloads[fileId].RecvSize = recvSize;
    adaptixWidget->Downloads[fileId].State    = state;

    for (int row = 0; row < tableWidget->rowCount(); ++row) {
        QTableWidgetItem *item = tableWidget->item(row, 0);
        if ( item && item->text() == fileId ) {
            tableWidget->item(row, 8)->setText(BytesToFormat(recvSize) );

            QProgressBar *pgbar = static_cast<QProgressBar *>(tableWidget->cellWidget(row, 9));
            pgbar->setValue(recvSize);

            if( state == DOWNLOAD_STATE_STOPPED) {
                pgbar->setEnabled(false);
            }
            else if(state == DOWNLOAD_STATE_RUNNING) {
                pgbar->setEnabled(true);
            }
            else if (state == DOWNLOAD_STATE_FINISHED) {
                adaptixWidget->Downloads[fileId].RecvSize = adaptixWidget->Downloads[fileId].TotalSize;

                tableWidget->item( row, 8 )->setText("");

                auto item_Status = new QTableWidgetItem( "Finished" );
                item_Status->setTextAlignment( Qt::AlignCenter );
                tableWidget->removeCellWidget(row,9);
                tableWidget->setItem(row, 9, item_Status);
                tableWidget->item(row, 9)->setForeground(QColor(COLOR_NeonGreen));
            }
            else if (state == DOWNLOAD_STATE_CANCELED) {
                tableWidget->removeCellWidget(row,9);
                tableWidget->removeRow( row );
                adaptixWidget->Downloads.remove(fileId);
            }

            break;
        }
    }
}

void DownloadsWidget::RemoveDownloadItem(const QString &fileId) const
{
    adaptixWidget->Downloads.remove(fileId);

    for ( int row = 0; row < tableWidget->rowCount(); row++ ) {
        if ( tableWidget->item( row, 0 )->text() == fileId ) {
            tableWidget->removeRow( row );
            break;
        }
    }
}

/// SLOTS

void DownloadsWidget::handleDownloadsMenu(const QPoint &pos )
{
    if ( !tableWidget->itemAt(pos) )
        return;

    QVector<DataMenuDownload> files;

    DataMenuDownload data = {};
    data.agentId = tableWidget->item( tableWidget->currentRow(), 2 )->text();
    data.fileId  = tableWidget->item( tableWidget->currentRow(), 0 )->text();
    data.path    = tableWidget->item( tableWidget->currentRow(), 5 )->text();

    auto ctxMenu = QMenu();
    auto Received = tableWidget->item( tableWidget->currentRow(), 8 )->text();
    if(Received.compare("") == 0) {
        ctxMenu.addAction("Sync file to client", this, &DownloadsWidget::actionSync);

        auto syncMenu = new QMenu("Sync as ...", &ctxMenu);
        syncMenu->addAction("Curl command", this, &DownloadsWidget::actionSyncCurl);
        syncMenu->addAction("Wget command", this, &DownloadsWidget::actionSyncWget);
        ctxMenu.addMenu(syncMenu);
        ctxMenu.addSeparator();

        data.state = "finished";
        files.append(data);
        int menuCount = adaptixWidget->ScriptManager->AddMenuDownload(&ctxMenu, "DownloadFinished", files);
        if (menuCount > 0)
            ctxMenu.addSeparator();

        ctxMenu.addAction("Delete file", this, &DownloadsWidget::actionDelete );
    }
    else {
        if( tableWidget->cellWidget( tableWidget->currentRow(), 9)->isEnabled() ) {
            data.state = "running";
            files.append(data);
        }
        else {
            data.state = "stopped";
            files.append(data);
        }
        adaptixWidget->ScriptManager->AddMenuDownload(&ctxMenu, "DownloadRunning", files);
    }

    ctxMenu.exec(tableWidget->horizontalHeader()->viewport()->mapToGlobal(pos) );
}

void DownloadsWidget::actionSync() const
{
    if( tableWidget->item( tableWidget->currentRow(), 8 )->text() != "" )
        return;

    QString fileId   = tableWidget->item( tableWidget->currentRow(), 0 )->text();
    QString filePath = tableWidget->item( tableWidget->currentRow(), 5 )->text();

    QString message = QString();
    bool ok = false;
    bool result = HttpReqGetOTP("download", fileId, *adaptixWidget->GetProfile(), &message, &ok);
    if( !result ) {
        MessageError("Response timeout");
        return;
    }
    if ( !ok ) {
        MessageError(message);
        return;
    }
    QString otp = message;

    QStringList pathParts = filePath.split("\\", Qt::SkipEmptyParts);
    QString fileName =  pathParts[pathParts.size()-1];
    pathParts = fileName.split("/", Qt::SkipEmptyParts);
    fileName = pathParts[pathParts.size()-1];

    NonBlockingDialogs::getSaveFileName(const_cast<DownloadsWidget*>(this), "Save File", fileName, "All Files (*.*)",
        [this, otp](const QString& savedPath) {
            if (savedPath.isEmpty())
                return;

            QString sUrl = adaptixWidget->GetProfile()->GetURL() + "/otp/download/sync";

            DialogDownloader dialog(sUrl, otp, savedPath);
            dialog.exec();
        });
}

void DownloadsWidget::actionSyncCurl() const
{
    if( tableWidget->item( tableWidget->currentRow(), 8 )->text() != "" )
        return;

    QString fileId   = tableWidget->item( tableWidget->currentRow(), 0 )->text();
    QString filePath = tableWidget->item( tableWidget->currentRow(), 5 )->text();

    QString message = QString();
    bool ok = false;
    bool result = HttpReqGetOTP("download", fileId, *adaptixWidget->GetProfile(), &message, &ok);
    if( !result ) {
        MessageError("Response timeout");
        return;
    }
    if ( !ok ) {
        MessageError(message);
        return;
    }
    QString otp = message;

    QStringList pathParts = filePath.split("\\", Qt::SkipEmptyParts);
    QString fileName =  pathParts[pathParts.size()-1];
    pathParts = fileName.split("/", Qt::SkipEmptyParts);
    fileName = pathParts[pathParts.size()-1];

    QString sUrl = adaptixWidget->GetProfile()->GetURL() + "/otp/download/sync";

    QString command = QString("curl -k %1 -H 'OTP: %2' -o %3").arg(sUrl).arg(otp).arg(fileName);

    QInputDialog inputDialog;
    inputDialog.setWindowTitle("Sync file as curl");
    inputDialog.setLabelText("Curl command:");
    inputDialog.setTextEchoMode(QLineEdit::Normal);
    inputDialog.setTextValue(command);
    inputDialog.setFixedSize(700,60);
    inputDialog.move(QGuiApplication::primaryScreen()->geometry().center() - inputDialog.geometry().center());
    inputDialog.exec();
}

void DownloadsWidget::actionSyncWget() const
{
    if( tableWidget->item( tableWidget->currentRow(), 8 )->text() != "" )
        return;

    QString fileId   = tableWidget->item( tableWidget->currentRow(), 0 )->text();
    QString filePath = tableWidget->item( tableWidget->currentRow(), 5 )->text();

    QString message = QString();
    bool ok = false;
    bool result = HttpReqGetOTP("download", fileId, *adaptixWidget->GetProfile(), &message, &ok);
    if( !result ) {
        MessageError("Response timeout");
        return;
    }
    if ( !ok ) {
        MessageError(message);
        return;
    }
    QString otp = message;

    QStringList pathParts = filePath.split("\\", Qt::SkipEmptyParts);
    QString fileName =  pathParts[pathParts.size()-1];
    pathParts = fileName.split("/", Qt::SkipEmptyParts);
    fileName = pathParts[pathParts.size()-1];

    QString sUrl = adaptixWidget->GetProfile()->GetURL() + "/otp/download/sync";

    QString command = QString("wget --no-check-certificate %1 --header='OTP: %2' -O %3").arg(sUrl).arg(otp).arg(fileName);

    QInputDialog inputDialog;
    inputDialog.setWindowTitle("Sync file as curl");
    inputDialog.setLabelText("Curl command:");
    inputDialog.setTextEchoMode(QLineEdit::Normal);
    inputDialog.setTextValue(command);
    inputDialog.setFixedSize(700,60);
    inputDialog.move(QGuiApplication::primaryScreen()->geometry().center() - inputDialog.geometry().center());
    inputDialog.exec();
}

void DownloadsWidget::actionDelete() const
{
    if( tableWidget->item( tableWidget->currentRow(), 8 )->text() == "" ) {

        QString fileId = tableWidget->item(tableWidget->currentRow(), 0)->text();
        QString message = QString();
        bool ok = false;
        bool result = HttpReqDownloadAction("delete", fileId, *(adaptixWidget->GetProfile()), &message, &ok);
        if (!result) {
            MessageError("Response timeout");
            return;
        }

        if (!ok) {
            MessageError(message);
        }
    }
}
