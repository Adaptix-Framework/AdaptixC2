#ifndef SCREENSHOTSWIDGET_H
#define SCREENSHOTSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>

#include <QSortFilterProxyModel>

class AdaptixWidget;

enum ScreensColumns {
    SCR_ScreenId,
    SCR_User,
    SCR_Computer,
    SCR_Note,
    SCR_Date,
    SCR_ColumnCount
};



class ScreensFilterProxyModel : public QSortFilterProxyModel
{
Q_OBJECT
    QString filter;
    bool    searchVisible = false;

public:
    explicit ScreensFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
        setDynamicSortFilter(true);
        setSortRole(Qt::UserRole);
    };

    void setSearchVisible(bool visible) {
        if (searchVisible == visible) return;
        searchVisible = visible;
        invalidateFilter();
    }

    void setTextFilter(const QString &text) {
        if (filter == text) return;
        filter = text;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
        if (!searchVisible || filter.isEmpty())
            return true;

        auto model = sourceModel();
        if (!model)
            return true;

        QString note = model->index(row, SCR_Note, parent).data().toString();
        return note.contains(filter, Qt::CaseInsensitive);
    }
};



class ScreensTableModel : public QAbstractTableModel
{
Q_OBJECT
    QVector<ScreenData>   screens;
    QHash<QString, int>   idToRow;

    void rebuildIndex() {
        idToRow.clear();
        for (int i = 0; i < screens.size(); ++i)
            idToRow[screens[i].ScreenId] = i;
    }

public:
    explicit ScreensTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex&) const override { return screens.size(); }
    int columnCount(const QModelIndex&) const override { return SCR_ColumnCount; }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= screens.size())
            return {};

        const ScreenData& s = screens.at(index.row());

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case SCR_ScreenId: return s.ScreenId;
                case SCR_User:     return s.User;
                case SCR_Computer: return s.Computer;
                case SCR_Note:     return s.Note;
                case SCR_Date:     return s.Date;
            }
        }

        if (role == Qt::UserRole) {
            switch (index.column()) {
                case SCR_Date: return s.DateTimestamp;
                default:      return data(index, Qt::DisplayRole);
            }
        }

        if (role == Qt::TextAlignmentRole) {
            if (index.column() == SCR_Note)
                return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
            return Qt::AlignCenter;
        }

        return {};
    }

    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        if (role != Qt::DisplayRole || o != Qt::Horizontal)
            return {};

        static QStringList headers = {"ID", "User", "Computer", "Note", "Date"};
        return headers.value(section);
    }

    void add(const ScreenData& item) {
        if (idToRow.contains(item.ScreenId))
            return;

        const int row = screens.size();
        beginInsertRows(QModelIndex(), row, row);
        screens.append(item);
        idToRow[item.ScreenId] = row;
        endInsertRows();
    }

    void update(const QString& screenId, const QString& note) {
        auto it = idToRow.find(screenId);
        if (it == idToRow.end())
            return;

        int row = it.value();
        screens[row].Note = note;
        Q_EMIT dataChanged(index(row, SCR_Note), index(row, SCR_Note));
    }

    void remove(const QString& screenId) {
        auto it = idToRow.find(screenId);
        if (it == idToRow.end())
            return;

        int row = it.value();
        beginRemoveRows(QModelIndex(), row, row);
        screens.removeAt(row);
        endRemoveRows();
        rebuildIndex();
    }

    void clear() {
        beginResetModel();
        screens.clear();
        idToRow.clear();
        endResetModel();
    }

    const ScreenData* getById(const QString& screenId) const {
        auto it = idToRow.find(screenId);
        if (it == idToRow.end())
            return nullptr;
        return &screens.at(it.value());
    }

    QString getScreenIdAt(int row) const {
        if (row < 0 || row >= screens.size())
            return {};
        return screens.at(row).ScreenId;
    }

    QStringList getSelectedIds(const QModelIndexList& selectedRows) const {
        QStringList ids;
        for (const QModelIndex& idx : selectedRows) {
            if (idx.row() >= 0 && idx.row() < screens.size())
                ids.append(screens.at(idx.row()).ScreenId);
        }
        return ids;
    }
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



class ClickableLabel;

class ScreenshotsWidget : public DockTab
{
Q_OBJECT
    AdaptixWidget* adaptixWidget  = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QTableView*    tableView      = nullptr;
    QSplitter*     splitter       = nullptr;
    ImageFrame*    imageFrame     = nullptr;
    QShortcut*     shortcutSearch = nullptr;

    ScreensTableModel*       screensModel = nullptr;
    ScreensFilterProxyModel* proxyModel   = nullptr;

    QWidget*        searchWidget = nullptr;
    QHBoxLayout*    searchLayout = nullptr;
    QLineEdit*      inputFilter  = nullptr;
    ClickableLabel* hideButton   = nullptr;

    void createUI();

public:
    explicit ScreenshotsWidget(AdaptixWidget* w);
    ~ScreenshotsWidget() override;

    void SetUpdatesEnabled(bool enabled);

    void Clear() const;
    void AddScreenshotItem(const ScreenData &newScreen);
    void EditScreenshotItem(const QString &screenId, const QString &note);
    void RemoveScreenshotItem(const QString &screenId);

    QString getSelectedScreenId() const;
    const ScreenData* getSelectedScreen() const;
    QStringList getSelectedScreenIds() const;

public Q_SLOTS:
    void toggleSearchPanel() const;
    void onFilterUpdate() const;
    void onTableItemSelection(const QModelIndex &current, const QModelIndex &previous);
    void handleScreenshotsMenu(const QPoint &pos);
    void actionDelete();
    void actionNote();
    void actionDownload();
};

#endif