#ifndef ADAPTIXCLIENT_DOWNLOADSWIDGET_H
#define ADAPTIXCLIENT_DOWNLOADSWIDGET_H

#include <main.h>

class AdaptixWidget;

class DownloadsWidget : public QWidget
{
    AdaptixWidget* adaptixWidget  = nullptr;
    QTableWidget*  tableWidget    = nullptr;
    QGridLayout*   mainGridLayout = nullptr;

    void createUI();

public:
    DownloadsWidget(AdaptixWidget* w);
    ~DownloadsWidget() override;

    void Clear() const;
    void AddDownloadItem(const DownloadData &newDownload);
    void EditDownloadItem(const QString &fileId, int recvSize, int state) const;
    void RemoveDownloadItem(const QString &fileId) const;

public slots:
    void handleDownloadsMenu(const QPoint &pos );
    void actionSync() const;
    void actionSyncCurl() const;
    void actionSyncWget() const;
    void actionDelete() const;
    void actionResume() const;
    void actionPause() const;
    void actionCancel() const;
};

#endif