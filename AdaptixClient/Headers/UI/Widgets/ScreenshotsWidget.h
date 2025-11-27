#ifndef SCREENSHOTSWIDGET_H
#define SCREENSHOTSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>

class AdaptixWidget;

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

public Q_SLOTS:
    void resizeImage() const;
    void setPixmap(const QPixmap&);
};



class ScreenshotsWidget : public DockTab
{
Q_OBJECT
    AdaptixWidget* adaptixWidget  = nullptr;
    QTableWidget*  tableWidget    = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QSplitter*     splitter       = nullptr;
    ImageFrame*    imageFrame     = nullptr;

    void createUI();

public:
    ScreenshotsWidget(AdaptixWidget* w);
    ~ScreenshotsWidget() override;

    void SetUpdatesEnabled(const bool enabled);

    void Clear() const;
    void AddScreenshotItem(const ScreenData &newScreen) const;
    void EditScreenshotItem(const QString &screenId, const QString &note) const;
    void RemoveScreenshotItem(const QString &screenId) const;

public Q_SLOTS:
    void onTableItemSelection(const QModelIndex &current, const QModelIndex &previous) const;
    void handleScreenshotsMenu(const QPoint &pos);
    void actionDelete() const;
    void actionNote() const;
    void actionDownload() const;
};

#endif