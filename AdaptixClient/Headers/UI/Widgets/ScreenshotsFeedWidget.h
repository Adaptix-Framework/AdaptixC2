#ifndef SCREENSHOTSFEEEDWIDGET_H
#define SCREENSHOTSFEEEDWIDGET_H

#include <Utils/CustomElements/ListFeed.h>
#include <UI/Widgets/AbstractDock.h>
#include <Client/PagedTableHelper.h>

#include <main.h>

#include <QNetworkAccessManager>
#include <QListView>
#include <QStackedWidget>
#include <QNetworkReply>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWheelEvent>

class AdaptixWidget;

enum ScreensFeedBlock {
    SCF_Id    = 0,
    SCF_Main  = 1,
    SCF_Tags  = 2,
    SCF_Right = 3,
    SCF_Count = 4
};





class ImageFrame : public QWidget
{
Q_OBJECT
    QLabel*      label;
    QScrollArea* scrollArea;
    QPixmap      originalPixmap;
    bool         ctrlPressed;
    double       scaleFactor;

public:
    explicit ImageFrame(QWidget *parent = nullptr);
    ~ImageFrame() override = default;

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





class ScreenshotGridDelegate : public QStyledItemDelegate
{
Q_OBJECT
    int m_thumbSize = 200;
public:
    explicit ScreenshotGridDelegate(QObject* parent = nullptr);
    void setThumbSize(int size);
    int thumbSize() const;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};





class ScreenshotsFeedWidget : public ListFeedWidget
{
Q_OBJECT
    friend class ScreenshotGridDelegate;
    AdaptixWidget*  m_adaptixWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* m_dockWidget = nullptr;
    FeedListModel*  feedModel       = nullptr;

    PagedTableHelper* pageHelper = nullptr;
    int               m_offset   = 0;
    QString           m_sortCol   = "Date";
    QString           m_sortOrder = "desc";

    QNetworkAccessManager*  imageNam  = nullptr;
    QNetworkReply*          imageReply = nullptr;
    ImageFrame*             imageFrame = nullptr;
    QSplitter*              m_feedSplitter = nullptr;
    QStackedWidget*         m_viewStack    = nullptr;
    QStackedWidget*         stackedView = nullptr;
    QListView*              gridView    = nullptr;
    ScreenshotGridDelegate* gridDelegate = nullptr;
    QHash<qint64, QPixmap>  thumbnailCache;

    void loadCurrentPage();
    void fetchImage(qint64 screenId);
    void cancelImageFetch();
    void showImage(qint64 screenId);
    void loadGridThumbnails();
    void fetchThumbnail(qint64 screenId);

public:
    explicit ScreenshotsFeedWidget(AdaptixWidget* w);
    ~ScreenshotsFeedWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock();

    void SetUpdatesEnabled(bool enabled);
    void Clear();
    void AddScreenshotItem(const ScreenData& newScreen);
    void EditScreenshotItem(qint64 screenId, const QString& note);
    void RemoveScreenshotItem(qint64 screenId);

    qint64 getSelectedScreenId() const;

public Q_SLOTS:
    void handleFeedMenu(const QPoint& pos);
    void actionNote();
    void actionDownload();
    void actionDelete();

private Q_SLOTS:
    void onPageReady(const QJsonObject& response);
    void onPageError(const QString& message);
};

#endif
