#ifndef ADAPTIXCLIENT_DOWNLOADSWIDGET_H
#define ADAPTIXCLIENT_DOWNLOADSWIDGET_H

#include <main.h>

class DownloadsWidget : public QWidget
{
    QWidget*      mainWidget     = nullptr;
    QTableWidget* tableWidget    = nullptr;
    QGridLayout*  mainGridLayout = nullptr;

    void createUI();

public:
    DownloadsWidget(QWidget* w);
    ~DownloadsWidget();

    void AddDownloadItem(DownloadData newDownload);
    void EditDownloadItem(QString fileId, int recvSize, int state);

public slots:
    void handleDownloadsMenu(const QPoint &pos );
};

#endif //ADAPTIXCLIENT_DOWNLOADSWIDGET_H