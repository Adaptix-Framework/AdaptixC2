#ifndef SCREENSHOTSWIDGET_H
#define SCREENSHOTSWIDGET_H

#include <main.h>

class ImageFrame : public QWidget
{
Q_OBJECT
    QLabel*      label;
    QScrollArea* scrollArea;
    QPixmap      originalPixmap;
    bool         ctrlPressed;
    double       scaleFactor;

public:
    explicit ImageFrame(QWidget *parent = 0);

    QPixmap pixmap() const;
    void clear();

protected:
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* e) override;

public slots:
    void resizeImage() const;
    void setPixmap(const QPixmap&);
};



class ScreenshotsWidget : public QWidget
{
Q_OBJECT
    QWidget*      mainWidget     = nullptr;
    QTableWidget* tableWidget    = nullptr;
    QGridLayout*  mainGridLayout = nullptr;
    QSplitter*    splitter       = nullptr;
    ImageFrame*   imageFrame     = nullptr;

    void createUI();

public:
    ScreenshotsWidget(QWidget* w);
    ~ScreenshotsWidget() override;

     void Clear() const;
     void AddScreenshotItem(const ScreenData &newScreen) const;
     void EditScreenshotItem(const QString &screenId, const QString &note) const;
     void RemoveScreenshotItem(const QString &screenId) const;

public slots:
    void onTableItemSelection(const QModelIndex &current, const QModelIndex &previous) const;
    void handleScreenshotsMenu(const QPoint &pos);
    void actionDelete() const;
    void actionNote() const;
    void actionDownload() const;
};

#endif