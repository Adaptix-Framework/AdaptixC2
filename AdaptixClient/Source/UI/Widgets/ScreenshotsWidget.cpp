#include <UI/Widgets/ScreenshotsWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Utils/CustomElements.h>
#include <Utils/NonBlockingDialogs.h>

REGISTER_DOCK_WIDGET(ScreenshotsWidget, "Screenshots", true)

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





ScreenshotsWidget::ScreenshotsWidget(AdaptixWidget* w) : DockTab("Screenshots", w->GetProfile()->GetProject(), ":/icons/picture"), adaptixWidget(w)
{
    this->createUI();

    connect(tableView, &QTableView::customContextMenuRequested, this, &ScreenshotsWidget::handleScreenshotsMenu);
    connect(tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection &selected, const QItemSelection &deselected){
        Q_UNUSED(selected)
        Q_UNUSED(deselected)
        tableView->setFocus();
    });
    connect(tableView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &ScreenshotsWidget::onTableItemSelection);
    connect(hideButton,  &ClickableLabel::clicked,  this, &ScreenshotsWidget::toggleSearchPanel);
    connect(inputFilter, &QLineEdit::textChanged,   this, &ScreenshotsWidget::onFilterUpdate);
    connect(inputFilter, &QLineEdit::returnPressed, this, [this]() { proxyModel->setTextFilter(inputFilter->text()); });
    connect(splitter,    &QSplitter::splitterMoved, imageFrame, &ImageFrame::resizeImage);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), this);
    shortcutSearch->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &ScreenshotsWidget::toggleSearchPanel);

    auto shortcutEsc = new QShortcut(QKeySequence(Qt::Key_Escape), inputFilter);
    shortcutEsc->setContext(Qt::WidgetShortcut);
    connect(shortcutEsc, &QShortcut::activated, this, [this]() { searchWidget->setVisible(false); });

    this->dockWidget->setWidget(this);
}

ScreenshotsWidget::~ScreenshotsWidget() = default;

void ScreenshotsWidget::SetUpdatesEnabled(const bool enabled)
{
    tableView->setUpdatesEnabled(enabled);
}

void ScreenshotsWidget::createUI()
{
    auto horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);
    searchWidget->setMaximumHeight(30);

    inputFilter = new QLineEdit(searchWidget);
    inputFilter->setPlaceholderText("filter: (admin | root) & ^(test)");
    inputFilter->setMaximumWidth(300);

    autoSearchCheck = new QCheckBox("auto", searchWidget);
    autoSearchCheck->setChecked(true);
    autoSearchCheck->setToolTip("Auto search on text change. If unchecked, press Enter to search.");

    hideButton = new ClickableLabel("  x  ");
    hideButton->setCursor(Qt::PointingHandCursor);
    hideButton->setStyleSheet("QLabel { color: #888; font-weight: bold; } QLabel:hover { color: #e34234; }");

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 5, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(inputFilter);
    searchLayout->addWidget(autoSearchCheck);
    searchLayout->addSpacing(8);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(horizontalSpacer);

    screensModel = new ScreensTableModel(this);
    proxyModel = new ScreensFilterProxyModel(this);
    proxyModel->setSourceModel(screensModel);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    tableView = new QTableView(this);
    tableView->setModel(proxyModel);
    tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    tableView->setAutoFillBackground(false);
    tableView->setShowGrid(false);
    tableView->setSortingEnabled(true);
    tableView->setWordWrap(true);
    tableView->setCornerButtonEnabled(false);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setFocusPolicy(Qt::NoFocus);
    tableView->setAlternatingRowColors(true);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tableView->horizontalHeader()->setCascadingSectionResizes(true);
    tableView->horizontalHeader()->setHighlightSections(false);
    tableView->verticalHeader()->setVisible(false);

    proxyModel->sort(-1);

    tableView->horizontalHeader()->setSectionResizeMode(SCR_Note, QHeaderView::Stretch);
    tableView->setItemDelegate(new PaddingDelegate(tableView));
    tableView->hideColumn(SCR_ScreenId);

    imageFrame = new ImageFrame(this);

    splitter = new QSplitter(this);
    splitter->setOrientation(Qt::Horizontal);
    splitter->addWidget(tableView);
    splitter->addWidget(imageFrame);
    splitter->setSizes(QList<int>() << 80 << 200);

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->addWidget(searchWidget, 0, 0, 1, 1);
    mainGridLayout->addWidget(splitter, 1, 0, 1, 1);
}

void ScreenshotsWidget::Clear() const
{
    adaptixWidget->Screenshots.clear();

    QSignalBlocker blocker(tableView->selectionModel());
    screensModel->clear();

    imageFrame->clear();
}

void ScreenshotsWidget::AddScreenshotItem(const ScreenData &newScreen)
{
    if (adaptixWidget->Screenshots.contains(newScreen.ScreenId))
        return;

    screensModel->add(newScreen);
    adaptixWidget->Screenshots[newScreen.ScreenId] = newScreen;
}

void ScreenshotsWidget::EditScreenshotItem(const QString &screenId, const QString &note)
{
    if (!adaptixWidget->Screenshots.contains(screenId))
        return;

    adaptixWidget->Screenshots[screenId].Note = note;
    screensModel->update(screenId, note);
}

void ScreenshotsWidget::RemoveScreenshotItem(const QString &screenId)
{
    if (!adaptixWidget->Screenshots.contains(screenId))
        return;

    adaptixWidget->Screenshots.remove(screenId);
    screensModel->remove(screenId);

    if (screensModel->rowCount(QModelIndex()) == 0)
        imageFrame->clear();
}

QString ScreenshotsWidget::getSelectedScreenId() const
{
    QModelIndexList selected = tableView->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return {};

    QModelIndex sourceIndex = proxyModel->mapToSource(selected.first());
    return screensModel->getScreenIdAt(sourceIndex.row());
}

const ScreenData* ScreenshotsWidget::getSelectedScreen() const
{
    QString screenId = getSelectedScreenId();
    if (screenId.isEmpty())
        return nullptr;
    return screensModel->getById(screenId);
}

QStringList ScreenshotsWidget::getSelectedScreenIds() const
{
    QModelIndexList selected = tableView->selectionModel()->selectedRows();
    QStringList ids;
    for (const QModelIndex& idx : selected) {
        QModelIndex sourceIndex = proxyModel->mapToSource(idx);
        QString id = screensModel->getScreenIdAt(sourceIndex.row());
        if (!id.isEmpty())
            ids.append(id);
    }
    return ids;
}

/// SLOTS

void ScreenshotsWidget::toggleSearchPanel() const
{
    if (this->searchWidget->isVisible()) {
        this->searchWidget->setVisible(false);
        proxyModel->setSearchVisible(false);
    }
    else {
        this->searchWidget->setVisible(true);
        proxyModel->setSearchVisible(true);
        inputFilter->setFocus();
    }
}

void ScreenshotsWidget::onFilterUpdate() const
{
    if (autoSearchCheck->isChecked()) {
        proxyModel->setTextFilter(inputFilter->text());
    }
    inputFilter->setFocus();
}

void ScreenshotsWidget::handleScreenshotsMenu(const QPoint &pos)
{
    QModelIndex index = tableView->indexAt(pos);
    if (!index.isValid())
        return;

    auto ctxMenu = QMenu();
    ctxMenu.addAction("Set note", this, &ScreenshotsWidget::actionNote);
    ctxMenu.addAction("Download", this, &ScreenshotsWidget::actionDownload);
    ctxMenu.addAction("Delete",   this, &ScreenshotsWidget::actionDelete);

    ctxMenu.exec(tableView->viewport()->mapToGlobal(pos));
}

void ScreenshotsWidget::actionNote()
{
    QStringList listId = getSelectedScreenIds();
    if (listId.empty())
        return;

    QString note = "";
    if (listId.size() == 1) {
        const ScreenData* screen = getSelectedScreen();
        if (screen)
            note = screen->Note;
    }

    bool inputOk;
    QString newNote = QInputDialog::getText(nullptr, "Set note", "New note", QLineEdit::Normal, note, &inputOk);
    if (inputOk) {
        HttpReqScreenSetNoteAsync(listId, newNote, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
            if (!success)
                MessageError(message.isEmpty() ? "Response timeout" : message);
        });
    }
}

void ScreenshotsWidget::actionDownload()
{
    const ScreenData* screen = getSelectedScreen();
    if (!screen)
        return;

    ScreenData screenData = *screen;

    QString baseDir = QStringLiteral("screenshot.png");
    if (adaptixWidget && adaptixWidget->GetProfile())
        baseDir = QDir(adaptixWidget->GetProfile()->GetProjectDir()).filePath(QStringLiteral("screenshot.png"));

    NonBlockingDialogs::getSaveFileName(this, "Save File", baseDir, "All Files (*.*)",
        [this, screenData](const QString& filePath) {
            if (filePath.isEmpty())
                return;

            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly)) {
                MessageError("Failed to open file for writing");
                return;
            }

            file.write(screenData.Content);
            file.close();

            QInputDialog inputDialog;
            inputDialog.setWindowTitle("Sync file");
            inputDialog.setLabelText("File saved to:");
            inputDialog.setTextEchoMode(QLineEdit::Normal);
            inputDialog.setTextValue(filePath);
            inputDialog.adjustSize();
            inputDialog.move(QGuiApplication::primaryScreen()->geometry().center() - inputDialog.geometry().center());
            inputDialog.exec();
    });
}

void ScreenshotsWidget::actionDelete()
{
    QStringList listId = getSelectedScreenIds();
    if (listId.empty())
        return;

    HttpReqScreenRemoveAsync(listId, *(adaptixWidget->GetProfile()), [](bool success, const QString& message, const QJsonObject&) {
        if (!success)
            MessageError(message.isEmpty() ? "Response timeout" : message);
    });
}

void ScreenshotsWidget::onTableItemSelection(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);

    if (!current.isValid())
        return;

    QModelIndex sourceIndex = proxyModel->mapToSource(current);
    QString screenId = screensModel->getScreenIdAt(sourceIndex.row());

    if (screenId.isEmpty())
        return;

    const ScreenData* screenData = screensModel->getById(screenId);
    if (!screenData)
        return;

    auto image = QPixmap();
    if (image.loadFromData(screenData->Content))
        imageFrame->setPixmap(image);
}