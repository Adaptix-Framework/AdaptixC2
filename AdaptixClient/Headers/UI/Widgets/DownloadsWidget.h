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
    ~DownloadsWidget() override;

    void Clear() const;
    void AddDownloadItem(const DownloadData &newDownload);
    void EditDownloadItem(const QString &fileId, int recvSize, int state) const;
    void RemoveDownloadItem(const QString &fileId) const;

public slots:
    void handleDownloadsMenu(const QPoint &pos );
    void actionSync() const;
    void actionDelete() const;
    void actionStart() const;
    void actionStop() const;
    void actionCancel() const;
};

#endif //ADAPTIXCLIENT_DOWNLOADSWIDGET_H