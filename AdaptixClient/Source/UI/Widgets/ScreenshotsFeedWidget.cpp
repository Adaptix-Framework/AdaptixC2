#include <UI/Widgets/ScreenshotsFeedWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Utils/NonBlockingDialogs.h>
#include <Utils/FontManager.h>

#include <oclero/qlementine/widgets/Menu.hpp>
#include <oclero/qlementine/widgets/SegmentedControl.hpp>

#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QUrlQuery>
#include <QTimer>
#include <QPainter>
#include <QScrollArea>
#include <memory>
#include <QFileDialog>
#include <QLinearGradient>
#include <QSlider>



ImageFrame::ImageFrame(QWidget* parent) : QWidget(parent), label(new QLabel), scrollArea(new QScrollArea(this)), ctrlPressed(false), scaleFactor(1.0)
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
    if (!originalPixmap.isNull())
        label->resize(scaleFactor * originalPixmap.size());
}

void ImageFrame::resizeEvent(QResizeEvent* e) { QWidget::resizeEvent(e); resizeImage(); }
QPixmap ImageFrame::pixmap() const { return originalPixmap; }
void ImageFrame::keyPressEvent(QKeyEvent* e) { if (e->key() == Qt::Key_Control) ctrlPressed = true; QWidget::keyPressEvent(e); }
void ImageFrame::keyReleaseEvent(QKeyEvent* e) { if (e->key() == Qt::Key_Control) ctrlPressed = false; QWidget::keyReleaseEvent(e); }

bool ImageFrame::eventFilter(QObject* obj, QEvent* e)
{
    if (obj == scrollArea->viewport() && e->type() == QEvent::Wheel) {
        auto we = static_cast<QWheelEvent*>(e);
        if (ctrlPressed) {
            const double step = (we->angleDelta().y() > 0) ? 1.05 : (1.0 / 1.05);
            scaleFactor *= step;
            scaleFactor = std::clamp(scaleFactor, 0.2, 2.0);
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





REGISTER_DOCK_WIDGET(ScreenshotsFeedWidget, "Screenshots Feed", true)

static FeedRow screenToFeedRow(const ScreenData& s) {
    FeedRow row;
    row.resize(SCF_Count);
    row.entityId = s.ScreenId;
    row[SCF_Id] = QVariantMap{
        {"id", QString("#%1").arg(s.ScreenId)},
        {"badge", QString("agent: %1").arg(s.AgentId)},
        {"date", QDateTime::fromSecsSinceEpoch(s.DateTimestamp).toString("dd/MM HH:mm:ss")}
    };
    row[SCF_Main] = QVariantMap{
        {"main", QString("%1 @ %2").arg(s.User, s.Computer)},
        {"submain", QString()},
        {"second", QString()}
    };
    QStringList tags;
    if (!s.Note.isEmpty())
        tags << s.Note;
    row[SCF_Tags] = QVariant(tags);
    row[SCF_Right] = QVariantMap{{"main", QString()}, {"second", QString()}, {"status", QString()}, {"statusType", QString()}};
    row.isDead = false;
    return row;
}

static ListFeedDelegate* createScreensDelegate(QObject* parent) {
    auto* d = new ListFeedDelegate(parent);
    d->addBlock(new IdBadgeBlock());
    d->addBlock(new MainBlock());
    d->addBlock(new TagsBlock());
    d->addBlock(new StatusBlock());
    return d;
}





ScreenshotGridDelegate::ScreenshotGridDelegate(QObject* parent) : QStyledItemDelegate(parent) {}
void ScreenshotGridDelegate::setThumbSize(int size) { m_thumbSize = size; }
int ScreenshotGridDelegate::thumbSize() const { return m_thumbSize; }

void ScreenshotGridDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    QRect r = option.rect;
    bool selected = option.state & QStyle::State_Selected;
    bool hover = option.state & QStyle::State_MouseOver;
    QPalette pal = option.palette;

    QColor bg = pal.color(QPalette::AlternateBase);
    if (selected)
        bg = pal.color(QPalette::Highlight).lighter(160);
    else if (hover)
        bg = pal.color(QPalette::Base).lighter(110);
    painter->fillRect(r, bg);

    QColor border = selected ? pal.color(QPalette::Highlight) : pal.color(QPalette::Mid);
    painter->setPen(QPen(border, selected ? 2 : 1));
    painter->drawRect(r.adjusted(0, 0, -1, -1));

    const AppTypography& ty = FontManager::instance().typography();
    const int mainLineH = QFontMetrics(ty.primary).height() + 2;
    const int bodyLineH = QFontMetrics(ty.body).height() + 2;
    const int noteLineH = qMax(bodyLineH, QFontMetrics(ty.caption).height() + 6);
    const int footerH = qMax(44, bodyLineH + noteLineH + 10);
    const int gradH   = qMax(20, mainLineH + 8);

    int imgH = r.height() - footerH;
    if (imgH < 40)
        imgH = qMax(24, r.height() / 2);
    QRect imgRect(r.adjusted(4, 4, -4, -(r.height() - imgH - 4)));
    painter->fillRect(imgRect, pal.color(QPalette::Dark));

    qint64 screenId = index.data(Qt::UserRole).toLongLong();
    QString idText, mainText, note, date;
    auto* fmodel = qobject_cast<const FeedListModel*>(index.model());
    if (!fmodel) {
        auto* proxy = qobject_cast<const QSortFilterProxyModel*>(index.model());
        if (proxy) fmodel = qobject_cast<const FeedListModel*>(proxy->sourceModel());
    }
    if (fmodel && screenId > 0) {
        for (int i = 0; i < fmodel->size(); ++i) {
            if (fmodel->rowAt(i).entityId == screenId) {
                const FeedRow& row = fmodel->rowAt(i);
                idText = row.blockData[SCF_Id].toMap()["id"].toString();
                mainText = row.blockData[SCF_Main].toMap()["main"].toString();
                QStringList tagList = row.blockData[SCF_Tags].toStringList();
                note = tagList.isEmpty() ? QString() : tagList.first();
                date = row.blockData[SCF_Id].toMap()["date"].toString();
                break;
            }
        }
    }

    QPixmap thumb;
    if (option.widget) {
        QObject* obj = const_cast<QWidget*>(option.widget);
        while (obj && !qobject_cast<ScreenshotsFeedWidget*>(obj)) obj = obj->parent();
        auto* sw = qobject_cast<ScreenshotsFeedWidget*>(obj);
        if (sw)
            thumb = sw->thumbnailCache.value(screenId);
    }

    if (!thumb.isNull()) {
        QRect thumbRect(QPoint(0, 0), thumb.size().scaled(imgRect.size(), Qt::KeepAspectRatio));
        thumbRect.moveCenter(imgRect.center());
        painter->drawPixmap(thumbRect, thumb);
    } else {
        painter->setPen(pal.color(QPalette::PlaceholderText));
        painter->setFont(FontManager::instance().typography().caption);
        painter->drawText(imgRect, Qt::AlignCenter, "No preview");
    }

    QLinearGradient grad(imgRect.bottomLeft(), imgRect.bottomLeft() + QPoint(0, -gradH));
    grad.setColorAt(0, QColor(pal.color(QPalette::Base).red(), pal.color(QPalette::Base).green(), pal.color(QPalette::Base).blue(), 200));
    grad.setColorAt(1, QColor(0, 0, 0, 0));
    painter->fillRect(QRect(imgRect.left(), imgRect.bottom() - gradH, imgRect.width(), gradH), grad);

    if (!idText.isEmpty()) {
        QFont idF = ty.primary;
        QFontMetrics idFm(idF);
        int idW = idFm.horizontalAdvance(idText) + 10;
        int idH = idFm.height() + 6;
        QRect idRect(imgRect.left() + 4, imgRect.top() + 4, idW, idH);
        painter->fillRect(idRect, QColor(pal.color(QPalette::Base).red(), pal.color(QPalette::Base).green(), pal.color(QPalette::Base).blue(), 180));
        painter->setFont(idF);
        painter->setPen(pal.color(QPalette::Text));
        painter->drawText(idRect, Qt::AlignCenter, idText);
    }

    painter->setPen(pal.color(QPalette::Text));
    QFont f = ty.primary;
    painter->setFont(f);
    QFontMetrics mainFm(f);
    QRect mainRect(imgRect.left() + 6, imgRect.bottom() - mainLineH - 2, imgRect.width() - 12, mainLineH);
    painter->drawText(mainRect, Qt::AlignLeft | Qt::AlignVCenter, mainFm.elidedText(mainText, Qt::ElideRight, mainRect.width()));

    QRect bottomRect(r.left() + 6, imgRect.bottom() + 4, r.width() - 12, footerH - 8);
    QFont small = ty.body;
    QFontMetrics fm(small);
    painter->setPen(pal.color(QPalette::PlaceholderText));
    painter->setFont(small);
    QRect dateRect(bottomRect.left(), bottomRect.top(), bottomRect.width(), bodyLineH);
    painter->drawText(dateRect, Qt::AlignRight | Qt::AlignVCenter, fm.elidedText(date, Qt::ElideRight, dateRect.width()));

    if (!note.isEmpty()) {
        QFontMetrics noteFm(ty.caption);
        int tw = noteFm.horizontalAdvance(note) + 12;
        QRect noteRect(bottomRect.left(), dateRect.bottom() + 2, qMin(tw, bottomRect.width()), noteLineH);
        painter->setBrush(QColor(pal.color(QPalette::Highlight).red(), pal.color(QPalette::Highlight).green(), pal.color(QPalette::Highlight).blue(), 40));
        painter->setPen(pal.color(QPalette::Highlight));
        painter->setFont(ty.caption);
        painter->drawRoundedRect(noteRect, 3, 3);
        painter->drawText(noteRect, Qt::AlignCenter, noteFm.elidedText(note, Qt::ElideRight, noteRect.width() - 6));
    }
    painter->restore();
}

static int screenshotsCardFooterHeight()
{
    const AppTypography& ty = FontManager::instance().typography();
    const int bodyLineH = QFontMetrics(ty.body).height() + 2;
    const int noteLineH = qMax(bodyLineH, QFontMetrics(ty.caption).height() + 6);
    return qMax(56, bodyLineH + noteLineH + 16);
}

QSize ScreenshotGridDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    const int footerH = screenshotsCardFooterHeight();
    return QSize(m_thumbSize + 8, m_thumbSize + footerH);
}



ScreenshotsFeedWidget::ScreenshotsFeedWidget(AdaptixWidget* w) : ListFeedWidget(w), m_adaptixWidget(w)
{
    feedModel = new FeedListModel(this);
    auto* delegate = createScreensDelegate(this);
    delegate->setFeedModel(feedModel);
    setModel(feedModel);
    setDelegate(delegate);

    enableSearch(true);
    enablePagination(true);
    finalizeSearchWidget();
    enableCompactSwitch(true);
    setBlockGap(12);
    rebuildModelChain();

    gridDelegate = new ScreenshotGridDelegate(this);
    gridView = new QListView(this);
    gridView->setModel(feedModel);
    gridView->setItemDelegate(gridDelegate);
    gridView->setViewMode(QListView::IconMode);
    gridView->setResizeMode(QListView::Adjust);
    gridView->setWrapping(true);
    gridView->setSpacing(8);
    gridView->setMovement(QListView::Static);
    gridView->setUniformItemSizes(true);
    gridView->setSelectionBehavior(QAbstractItemView::SelectItems);
    gridView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    gridView->setContextMenuPolicy(Qt::CustomContextMenu);
    gridView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    gridView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gridView->setGridSize(QSize(208, 256));
    gridDelegate->setThumbSize(260);

    imageFrame = new ImageFrame(this);
    m_feedSplitter = new QSplitter(Qt::Horizontal, this);
    m_feedSplitter->addWidget(treeView());
    m_feedSplitter->addWidget(imageFrame);
    m_feedSplitter->setStretchFactor(0, 3);
    m_feedSplitter->setStretchFactor(1, 7);
    m_feedSplitter->setSizes(QList<int>() << 300 << 700);

    auto* cardPanel = new QWidget(this);
    auto* cardLayout = new QVBoxLayout(cardPanel);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);
    auto* sizeSlider = new QSlider(Qt::Horizontal, cardPanel);
    sizeSlider->setRange(100, 500);
    sizeSlider->setValue(260);
    sizeSlider->setFixedHeight(28);
    sizeSlider->setTickPosition(QSlider::TicksBelow);
    sizeSlider->setTickInterval(50);
    cardLayout->addWidget(gridView, 1);
    cardLayout->addWidget(sizeSlider, 0);

    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(m_feedSplitter);
    m_viewStack->addWidget(cardPanel);

    auto* segment = new oclero::qlementine::SegmentedControl(this);
    segment->addItem("Feed");
    segment->addItem("Card");
    segment->setCurrentIndex(1);
    segment->setFixedHeight(FontManager::instance().typography().segmentHeight);
    connect(segment, &oclero::qlementine::SegmentedControl::currentIndexChanged, this, [segment, this]() {
        int idx = segment->currentIndex();
        m_viewStack->setCurrentIndex(idx);
        if (idx == 1) loadGridThumbnails();
    });
    addToolbarWidgetBefore(segment);

    auto* debounceTimer = new QTimer(this);
    debounceTimer->setSingleShot(true);
    debounceTimer->setInterval(200);
    connect(debounceTimer, &QTimer::timeout, this, [this]() { loadGridThumbnails(); });
    auto applyGridMetrics = [this]() {
        if (!gridDelegate || !gridView)
            return;
        const int thumb = gridDelegate->thumbSize();
        const int footerH = screenshotsCardFooterHeight();
        gridView->setGridSize(QSize(thumb + 8, thumb + footerH));
        gridView->doItemsLayout();
        gridView->viewport()->update();
    };

    connect(sizeSlider, &QSlider::valueChanged, this, [this, debounceTimer, applyGridMetrics](int val) {
        gridDelegate->setThumbSize(val);
        applyGridMetrics();
        debounceTimer->start();
    });
    connect(&FontManager::instance(), &FontManager::typographyChanged, this, applyGridMetrics);
    applyGridMetrics();

    auto* mainGrid = qobject_cast<QGridLayout*>(layout());
    if (mainGrid) {
        mainGrid->addWidget(m_viewStack, 1, 0, 1, -1);
        mainGrid->setRowStretch(0, 0);
        mainGrid->setRowStretch(1, 1);
    }

    m_viewStack->setCurrentIndex(1);
    loadGridThumbnails();

    m_dockWidget = new KDDockWidgets::QtWidgets::DockWidget("ScreenshotsFeed:Dock-" + w->GetProfile()->GetProject(), KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
    m_dockWidget->setTitle("Screenshots");
    m_dockWidget->setIcon(QIcon(":/icons/picture"), KDDockWidgets::IconPlace::TabBar);
    m_dockWidget->setWidget(this);

    auto* pbar = paginationBar();
    if (pbar) {
        pbar->blockSignals(true);
        auto* spin = pbar->findChild<QSpinBox*>();
        if (spin)
            spin->setValue(30);
        pbar->blockSignals(false);

        connect(pbar, &PaginationBar::prevClicked, this, [this]() { m_offset = qMax(0, m_offset - paginationBar()->pageSize()); loadCurrentPage(); });
        connect(pbar, &PaginationBar::nextClicked, this, [this]() { m_offset += paginationBar()->pageSize(); loadCurrentPage(); });
        connect(pbar, &PaginationBar::pageSizeChanged, this, [this](int size) { pageHelper->setPageSize(size); m_offset = 0; loadCurrentPage(); });
    }

    auto* si = searchInput();
    if (si)
        connect(si, &QLineEdit::returnPressed, this, [this]() { m_offset = 0; loadCurrentPage(); });

    pageHelper = new PagedTableHelper(w->GetProfile(), "/screen/list", this);
    if (pbar)
        pageHelper->setPageSize(pbar->pageSize());
    connect(pageHelper, &PagedTableHelper::pageReady, this, &ScreenshotsFeedWidget::onPageReady);
    connect(pageHelper, &PagedTableHelper::errorOccurred, this, &ScreenshotsFeedWidget::onPageError);
    connect(pageHelper, &PagedTableHelper::loadingChanged, this, [this, pbar](bool loading) { if (pbar) pbar->setLoading(loading); });

    imageNam = new QNetworkAccessManager(this);

    connect(treeView(), &QTreeView::customContextMenuRequested, this, &ScreenshotsFeedWidget::handleFeedMenu);
    connect(gridView, &QListView::customContextMenuRequested, this, &ScreenshotsFeedWidget::handleFeedMenu);

    connect(treeView()->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this](const QModelIndex& current, const QModelIndex&) {
        if (!current.isValid()) {
            imageFrame->clear();
            return;
        }
        qint64 sid = proxyModel()->data(current, Qt::UserRole).toLongLong();
        showImage(sid);
    });

    connect(gridView->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& current, const QModelIndex&) {
        if (!current.isValid()) {
            imageFrame->clear();
            return;
        }
        qint64 sid = current.data(Qt::UserRole).toLongLong();
        if (sid)
            showImage(sid);
    });

    connect(gridView, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        if (!index.isValid())
            return;
        qint64 sid = index.data(Qt::UserRole).toLongLong();
        if (!sid)
            return;

        QByteArray imgData;
        {
            QReadLocker locker(&m_adaptixWidget->ScreenshotsLock);
            auto it = m_adaptixWidget->Screenshots.constFind(sid);
            if (it != m_adaptixWidget->Screenshots.constEnd())
                imgData = it->Content;
        }

        auto openViewer = [this, sid](const QByteArray& data) {
            QPixmap pix;
            if (!pix.loadFromData(data))
                return;

            auto* viewer = new QWidget(nullptr, Qt::Window);
            viewer->setWindowTitle(QString("Screenshot #%1").arg(sid));
            viewer->resize(1200, 800);
            auto* layout = new QVBoxLayout(viewer);
            layout->setContentsMargins(0, 0, 0, 0);

            auto* scroll = new QScrollArea(viewer);
            auto* lbl = new QLabel(scroll);
            lbl->setPixmap(pix);
            lbl->setScaledContents(true);
            lbl->resize(pix.size());
            scroll->setWidget(lbl);
            scroll->setAlignment(Qt::AlignCenter);
            layout->addWidget(scroll, 1);

            auto* toolbar = new QWidget(viewer);
            auto* tbLayout = new QHBoxLayout(toolbar);
            tbLayout->setContentsMargins(8, 4, 8, 4);
            auto* btnZoomIn = new QPushButton("+", toolbar);
            auto* btnZoomOut = new QPushButton("-", toolbar);
            auto* btnFit = new QPushButton("Fit", toolbar);
            auto* btnSave = new QPushButton("Save", toolbar);
            btnZoomIn->setFixedSize(28, 24);
            btnZoomOut->setFixedSize(28, 24);
            btnFit->setFixedHeight(FontManager::instance().typography().controlInnerH);
            btnSave->setFixedHeight(FontManager::instance().typography().controlInnerH);
            auto* zoomLabel = new QLabel("100%", toolbar);
            zoomLabel->setFixedWidth(50);
            zoomLabel->setAlignment(Qt::AlignCenter);
            tbLayout->addWidget(btnZoomIn);
            tbLayout->addWidget(btnZoomOut);
            tbLayout->addWidget(zoomLabel);
            tbLayout->addWidget(btnFit);
            tbLayout->addStretch();
            tbLayout->addWidget(btnSave);
            auto* btnClose = new QPushButton("Close", toolbar);
            btnClose->setFixedHeight(FontManager::instance().typography().controlInnerH);
            tbLayout->addWidget(btnClose);
            QObject::connect(btnClose, &QPushButton::clicked, viewer, &QWidget::close);
            layout->addWidget(toolbar);

            auto scale = std::make_shared<double>(1.0);

            auto* filter = new QObject(scroll->viewport());
            filter->installEventFilter(scroll->viewport());
            QObject::connect(viewer, &QObject::destroyed, filter, &QObject::deleteLater);
            QObject::connect(filter, &QObject::destroyed, [filter]() { filter->deleteLater(); });
            scroll->viewport()->installEventFilter(filter);
            QObject::connect(filter, &QObject::destroyed, viewer, [scale, lbl, zoomLabel, pix, filter]() {});

            struct WheelFilter : public QObject {
                std::shared_ptr<double> scale;
                QLabel* lbl;
                QLabel* zoomLabel;
                QPixmap pix;
                bool ctrlPressed = false;
                WheelFilter(std::shared_ptr<double> s, QLabel* l, QLabel* z, QPixmap p, QObject* parent) : QObject(parent), scale(s), lbl(l), zoomLabel(z), pix(p) {}
                bool eventFilter(QObject* obj, QEvent* event) override {
                    if (event->type() == QEvent::KeyPress) {
                        auto* ke = static_cast<QKeyEvent*>(event);
                        if (ke->key() == Qt::Key_Control)
                            ctrlPressed = true;
                    } else if (event->type() == QEvent::KeyRelease) {
                        auto* ke = static_cast<QKeyEvent*>(event);
                        if (ke->key() == Qt::Key_Control)
                            ctrlPressed = false;
                    } else if (event->type() == QEvent::Wheel && ctrlPressed) {
                        auto* we = static_cast<QWheelEvent*>(event);
                        double step = (we->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
                        *scale = qBound(0.1, *scale * step, 5.0);
                        lbl->resize(pix.size() * *scale);
                        zoomLabel->setText(QString("%1%").arg(qRound(*scale * 100)));
                        return true;
                    }
                    return false;
                }
            };
            auto* wheelFilter = new WheelFilter(scale, lbl, zoomLabel, pix, viewer);
            viewer->installEventFilter(wheelFilter);
            scroll->viewport()->installEventFilter(wheelFilter);

            QObject::connect(btnZoomIn, &QPushButton::clicked, viewer, [scale, lbl, zoomLabel, pix]() {
                *scale = qMin(*scale * 1.2, 5.0);
                lbl->resize(pix.size() * *scale);
                zoomLabel->setText(QString("%1%").arg(qRound(*scale * 100)));
            });
            QObject::connect(btnZoomOut, &QPushButton::clicked, viewer, [scale, lbl, zoomLabel, pix]() {
                *scale = qMax(*scale / 1.2, 0.1);
                lbl->resize(pix.size() * *scale);
                zoomLabel->setText(QString("%1%").arg(qRound(*scale * 100)));
            });
            QObject::connect(btnFit, &QPushButton::clicked, viewer, [scroll, lbl, scale, zoomLabel, pix]() {
                QSize vp = scroll->viewport()->size();
                double fitW = (double)vp.width() / pix.width();
                double fitH = (double)vp.height() / pix.height();
                *scale = qMin(fitW, fitH);
                lbl->resize(pix.size() * *scale);
                zoomLabel->setText(QString("%1%").arg(qRound(*scale * 100)));
            });
            QObject::connect(btnSave, &QPushButton::clicked, viewer, [data, sid]() {
                QString defName = QString("screenshot_%1.png").arg(sid);
                QString path = QFileDialog::getSaveFileName(nullptr, "Save Screenshot", defName, "Images (*.png *.jpg *.bmp)");
                if (!path.isEmpty()) {
                    QFile f(path);
                    if (f.open(QIODevice::WriteOnly)) { f.write(data); f.close(); }
                }
            });

            viewer->show();
        };

        if (!imgData.isEmpty()) {
            openViewer(imgData);
        } else {
            auto* profile = m_adaptixWidget->GetProfile();
            if (!profile) return;
            QUrl url(profile->GetURL() + "/screen/image");
            QUrlQuery q;
            q.addQueryItem("screen_id", QString::number(sid));
            url.setQuery(q);
            QNetworkRequest request(url);
            auto ssl = QSslConfiguration::defaultConfiguration();
            ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
            ssl.setProtocol(QSsl::TlsV1_2OrLater);
            request.setSslConfiguration(ssl);
            request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
            QString token = profile->GetAccessToken();
            if (!token.isEmpty())
                request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
            auto* nam = new QNetworkAccessManager(this);
            auto* reply = nam->get(request);
            QObject::connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError>&) { reply->ignoreSslErrors(); });
            QObject::connect(reply, &QNetworkReply::finished, this, [reply, nam, sid, openViewer]() {
                if (reply->error() == QNetworkReply::NoError) {
                    QByteArray data = reply->readAll();
                    openViewer(data);
                }
                reply->deleteLater();
                nam->deleteLater();
            });
        }
    });

    loadCurrentPage();
}

ScreenshotsFeedWidget::~ScreenshotsFeedWidget() { cancelImageFetch(); }

KDDockWidgets::QtWidgets::DockWidget* ScreenshotsFeedWidget::dock() { return m_dockWidget; }

void ScreenshotsFeedWidget::loadCurrentPage()
{
    pageHelper->setParam("q", searchInput() ? searchInput()->text() : QString());
    pageHelper->setParam("sort", m_sortCol);
    pageHelper->setParam("order", m_sortOrder);
    pageHelper->loadPage(m_offset);
}

void ScreenshotsFeedWidget::loadGridThumbnails()
{
    QSize thumbSize(gridDelegate->thumbSize(), gridDelegate->thumbSize() * 3 / 4);
    QList<qint64> toFetch;
    for (int i = 0; i < feedModel->size(); ++i) {
        qint64 sid = feedModel->rowAt(i).entityId;
        if (!sid)
            continue;
        auto it = thumbnailCache.find(sid);
        if (it != thumbnailCache.end() && it->size() == thumbSize)
            continue;

        QByteArray cached;
        {
            QReadLocker locker(&m_adaptixWidget->ScreenshotsLock);
            auto sit = m_adaptixWidget->Screenshots.constFind(sid);
            if (sit != m_adaptixWidget->Screenshots.constEnd())
                cached = sit->Content;
        }
        if (!cached.isEmpty()) {
            QPixmap fullPix;
            if (fullPix.loadFromData(cached))
                thumbnailCache[sid] = fullPix.scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        } else {
            toFetch.append(sid);
        }
    }
    gridView->viewport()->update();
    for (int i = 0; i < qMin(20, toFetch.size()); ++i)
        fetchThumbnail(toFetch.at(i));
}

void ScreenshotsFeedWidget::fetchThumbnail(qint64 screenId)
{
    auto* profile = m_adaptixWidget->GetProfile();
    if (!profile)
        return;

    QUrl url(profile->GetURL() + "/screen/image");
    QUrlQuery q;
    q.addQueryItem("screen_id", QString::number(screenId));
    url.setQuery(q);

    QNetworkRequest request(url);
    auto sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    request.setSslConfiguration(sslConfig);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QString token = profile->GetAccessToken();
    if (!token.isEmpty()) request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());

    auto* reply = imageNam->get(request);
    connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError>&) { reply->ignoreSslErrors(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, screenId]() {
        QByteArray data;
        bool ok = false;
        if (reply->error() == QNetworkReply::NoError) {
            data = reply->readAll();
            ok = reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith("image/");
        }
        reply->deleteLater();
        if (!ok || data.isEmpty()) return;

        {
            QWriteLocker locker(&m_adaptixWidget->ScreenshotsLock);
            auto it = m_adaptixWidget->Screenshots.find(screenId);
            if (it != m_adaptixWidget->Screenshots.end())
                it->Content = data;
        }

        QSize thumbSize(gridDelegate->thumbSize(), gridDelegate->thumbSize() * 3 / 4);
        QPixmap fullPix;
        if (fullPix.loadFromData(data)) {
            thumbnailCache[screenId] = fullPix.scaled(thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QTimer::singleShot(0, this, [this]() { if (gridView) gridView->viewport()->update(); });
        }
    });
}

void ScreenshotsFeedWidget::cancelImageFetch()
{
    if (imageReply) {
        imageReply->disconnect(this);
        imageReply->abort();
        imageReply->deleteLater();
        imageReply = nullptr;
    }
}

void ScreenshotsFeedWidget::fetchImage(qint64 screenId)
{
    cancelImageFetch();
    auto* profile = m_adaptixWidget->GetProfile();
    if (!profile)
        return;

    QUrl url(profile->GetURL() + "/screen/image");
    QUrlQuery q;
    q.addQueryItem("screen_id", QString::number(screenId));
    url.setQuery(q);

    QNetworkRequest request(url);
    auto sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    request.setSslConfiguration(sslConfig);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QString token = profile->GetAccessToken();
    if (!token.isEmpty())
        request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());

    imageReply = imageNam->get(request);
    connect(imageReply, &QNetworkReply::sslErrors, this, [this](const QList<QSslError>&) { if (imageReply) imageReply->ignoreSslErrors(); });
    connect(imageReply, &QNetworkReply::finished, this, [this, screenId]() {
        if (!imageReply)
            return;

        QByteArray data;
        bool ok = false;
        if (imageReply->error() == QNetworkReply::NoError) {
            data = imageReply->readAll();
            ok = imageReply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith("image/");
        }
        imageReply->deleteLater();
        imageReply = nullptr;
        if (ok && !data.isEmpty()) {
            {
                QWriteLocker locker(&m_adaptixWidget->ScreenshotsLock);
                auto it = m_adaptixWidget->Screenshots.find(screenId);
                if (it != m_adaptixWidget->Screenshots.end())
                    it->Content = data;
            }
            QPixmap pix;
            if (pix.loadFromData(data))
                imageFrame->setPixmap(pix);
        }
    });
}

void ScreenshotsFeedWidget::showImage(qint64 screenId)
{
    QByteArray cached;
    {
        QReadLocker locker(&m_adaptixWidget->ScreenshotsLock);
        auto it = m_adaptixWidget->Screenshots.constFind(screenId);
        if (it != m_adaptixWidget->Screenshots.constEnd())
            cached = it->Content;
    }
    if (!cached.isEmpty()) {
        cancelImageFetch();
        QPixmap pix;
        if (pix.loadFromData(cached))
            imageFrame->setPixmap(pix);
        else
            imageFrame->clear();
        return;
    }
    imageFrame->clear();
    fetchImage(screenId);
}

void ScreenshotsFeedWidget::SetUpdatesEnabled(bool enabled) { treeView()->setUpdatesEnabled(enabled); }

void ScreenshotsFeedWidget::Clear()
{
    cancelImageFetch();
    {
        QWriteLocker locker(&m_adaptixWidget->ScreenshotsLock);
        m_adaptixWidget->Screenshots.clear();
    }
    if (feedModel)
        feedModel->clear();
    imageFrame->clear();
    m_offset = 0;
}

void ScreenshotsFeedWidget::AddScreenshotItem(const ScreenData& newScreen)
{
    {
        QWriteLocker locker(&m_adaptixWidget->ScreenshotsLock);
        if (m_adaptixWidget->Screenshots.contains(newScreen.ScreenId))
            return;
        m_adaptixWidget->Screenshots[newScreen.ScreenId] = newScreen;
    }

    if (feedModel && m_offset == 0) {
        for (int i = 0; i < feedModel->size(); ++i) {
            if (feedModel->rowAt(i).entityId == newScreen.ScreenId) {
                feedModel->updateRow(i, screenToFeedRow(newScreen));
                return;
            }
        }
        feedModel->insertRow(0, screenToFeedRow(newScreen));
    } else if (feedModel) {
        m_offset = 0;
        loadCurrentPage();
    }
}

void ScreenshotsFeedWidget::EditScreenshotItem(qint64 screenId, const QString& note)
{
    {
        QWriteLocker locker(&m_adaptixWidget->ScreenshotsLock);
        if (!m_adaptixWidget->Screenshots.contains(screenId))
            return;
        m_adaptixWidget->Screenshots[screenId].Note = note;
    }

    if (!feedModel)
        return;

    for (int i = 0; i < feedModel->size(); ++i) {
        const FeedRow& r = feedModel->rowAt(i);
        if (r.entityId == screenId) {
            FeedRow newRow = r;
            newRow[SCF_Main] = QVariantMap{{"main", r.blockData[SCF_Main].toMap()["main"].toString()}, {"submain", note}, {"second", QString()}};
            QStringList tags; if (!note.isEmpty()) tags << note;
            newRow[SCF_Tags] = QVariant(tags);
            feedModel->updateRow(i, newRow);
            return;
        }
    }
}

void ScreenshotsFeedWidget::RemoveScreenshotItem(qint64 screenId)
{
    {
        QWriteLocker locker(&m_adaptixWidget->ScreenshotsLock);
        m_adaptixWidget->Screenshots.remove(screenId);
    }

    if (!feedModel)
        return;
    for (int i = 0; i < feedModel->size(); ++i) {
        if (feedModel->rowAt(i).entityId == screenId) {
            feedModel->removeRow(i);
            return;
        }
    }
}

qint64 ScreenshotsFeedWidget::getSelectedScreenId() const
{
    QModelIndex idx;
    if (m_viewStack->currentIndex() == 0)
        idx = treeView()->currentIndex();
    else
        idx = gridView->currentIndex();
    if (!idx.isValid())
        return 0;
    return proxyModel()->data(idx, Qt::UserRole).toLongLong();
}

void ScreenshotsFeedWidget::onPageReady(const QJsonObject& response)
{
    QJsonArray items = response["items"].toArray();
    QList<ScreenData> page;
    page.reserve(items.size());
    for (const QJsonValue& v : items) {
        QJsonObject obj = v.toObject();
        ScreenData s;
        s.ScreenId      = parseI64(obj, "s_screen_id");
        s.AgentId       = parseI64(obj, "s_agent_id");
        s.User          = obj["s_user"].toString();
        s.Computer      = obj["s_computer"].toString();
        s.Note          = obj["s_note"].toString();
        s.DateTimestamp = parseI64(obj, "s_date");
        s.Date          = UnixTimestampGlobalToStringLocal(s.DateTimestamp);
        page.append(s);
        {
            QWriteLocker locker(&m_adaptixWidget->ScreenshotsLock);
            if (!m_adaptixWidget->Screenshots.contains(s.ScreenId))
                m_adaptixWidget->Screenshots[s.ScreenId] = s;
        }
    }
    m_offset = response["offset"].toInt();
    int total = response["total"].toInt();
    feedModel->clear();
    for (const auto& s : page) {
        feedModel->addRow(screenToFeedRow(s));
    }
    if (paginationBar()) {
        int pageSize = paginationBar()->pageSize();
        int shown = page.size();
        int from = total == 0 ? 0 : m_offset + 1;
        int to = m_offset + shown;
        paginationBar()->setInfo(from, to, total);
        paginationBar()->setPrevEnabled(m_offset > 0);
        paginationBar()->setNextEnabled(m_offset + pageSize < total);
    }
    if (page.isEmpty())
        imageFrame->clear();
    if (m_viewStack && m_viewStack->currentIndex() == 1)
        loadGridThumbnails();
}

void ScreenshotsFeedWidget::onPageError(const QString&)
{
    if (feedModel)
        feedModel->clear();
    imageFrame->clear();
    if (paginationBar()) {
        paginationBar()->setInfo(0, 0, 0);
        paginationBar()->setPrevEnabled(false);
        paginationBar()->setNextEnabled(false);
    }
}

void ScreenshotsFeedWidget::handleFeedMenu(const QPoint& pos)
{
    QAbstractItemView* view = (m_viewStack->currentIndex() == 0)
        ? static_cast<QAbstractItemView*>(treeView())
        : static_cast<QAbstractItemView*>(gridView);

    QModelIndex index = ListFeedWidget::prepareContextMenuSelection(view, pos);
    if (!index.isValid())
        return;

    qint64 screenId = index.data(Qt::UserRole).toLongLong();
    if (!screenId && proxyModel())
        screenId = proxyModel()->data(index, Qt::UserRole).toLongLong();
    if (!screenId)
        return;

    oclero::qlementine::Menu ctxMenu;
    ctxMenu.addAction("Set note", this, &ScreenshotsFeedWidget::actionNote);
    ctxMenu.addAction("Download", this, &ScreenshotsFeedWidget::actionDownload);
    ctxMenu.addAction("Delete",   this, &ScreenshotsFeedWidget::actionDelete);
    ctxMenu.exec(view->viewport()->mapToGlobal(pos));
}

void ScreenshotsFeedWidget::actionNote()
{
    qint64 screenId = getSelectedScreenId();
    if (!screenId)
        return;

    QString note;
    {
        QReadLocker locker(&m_adaptixWidget->ScreenshotsLock);
        auto it = m_adaptixWidget->Screenshots.constFind(screenId);
        if (it != m_adaptixWidget->Screenshots.constEnd())
            note = it->Note;
    }
    bool ok;
    QString newNote = QInputDialog::getText(this, "Set note", "Note:", QLineEdit::Normal, note, &ok);
    if (ok) {
        QList<qint64> ids = {screenId};
        HttpReqScreenSetNoteAsync(ids, newNote, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void ScreenshotsFeedWidget::actionDownload()
{
    qint64 screenId = getSelectedScreenId();
    if (!screenId)
        return;

    QByteArray cached;
    {
        QReadLocker locker(&m_adaptixWidget->ScreenshotsLock);
        auto it = m_adaptixWidget->Screenshots.constFind(screenId);
        if (it != m_adaptixWidget->Screenshots.constEnd())
            cached = it->Content;
    }

    auto saveBytes = [this](const QByteArray& bytes) {
        QString baseDir = "screenshot.png";
        if (m_adaptixWidget && m_adaptixWidget->GetProfile())
            baseDir = QDir(m_adaptixWidget->GetProfile()->GetProjectDir()).filePath("screenshot.png");

        NonBlockingDialogs::getSaveFileName(this, "Save", baseDir, "All Files (*.*)",
            [bytes](const QString& filePath) {
                if (filePath.isEmpty())
                    return;

                QFile f(filePath);
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(bytes);
                    f.close();
                }
            });
    };
    if (!cached.isEmpty()) {
        saveBytes(cached);
        return;
    }

    auto* nam = new QNetworkAccessManager(this);
    QUrl url(m_adaptixWidget->GetProfile()->GetURL() + "/screen/image");
    QUrlQuery q;
    q.addQueryItem("screen_id", QString::number(screenId));
    url.setQuery(q);
    QNetworkRequest request(url);
    auto ssl = QSslConfiguration::defaultConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
    ssl.setProtocol(QSsl::TlsV1_2OrLater);
    request.setSslConfiguration(ssl);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    QString token = m_adaptixWidget->GetProfile()->GetAccessToken();
    if (!token.isEmpty()) request.setRawHeader("Authorization", ("Bearer " + token).toUtf8());
    auto* reply = nam->get(request);
    connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError>&) { reply->ignoreSslErrors(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, screenId, saveBytes]() {
        QByteArray data;
        bool ok = false;
        if (reply->error() == QNetworkReply::NoError) {
            data = reply->readAll();
            ok = reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith("image/");
        }
        reply->deleteLater();
        nam->deleteLater();
        if (!ok) {
            MessageError("Failed to fetch image");
            return;
        }
        {
            QWriteLocker l(&m_adaptixWidget->ScreenshotsLock);
            auto it = m_adaptixWidget->Screenshots.find(screenId);
            if (it != m_adaptixWidget->Screenshots.end())
                it->Content = data;
        }
        saveBytes(data);
    });
}

void ScreenshotsFeedWidget::actionDelete()
{
    qint64 screenId = getSelectedScreenId();
    if (!screenId)
        return;

    QList<qint64> ids = {screenId};
    HttpReqScreenRemoveAsync(ids, *(m_adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}
