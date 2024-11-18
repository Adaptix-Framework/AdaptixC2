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
    void RemoveDownloadItem(QString fileId);

public slots:
    void handleDownloadsMenu(const QPoint &pos );
    void actionSync();
    void actionDelete();
    void actionStart();
    void actionStop();
    void actionCancel();
};

#endif //ADAPTIXCLIENT_DOWNLOADSWIDGET_H