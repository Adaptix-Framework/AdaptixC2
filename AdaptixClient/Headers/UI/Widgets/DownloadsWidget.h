#ifndef ADAPTIXCLIENT_DOWNLOADSWIDGET_H
#define ADAPTIXCLIENT_DOWNLOADSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>

#include <QSortFilterProxyModel>
#include <QStyledItemDelegate>

class AdaptixWidget;
class ClickableLabel;

enum DownloadsColumns {
    DC_FileId,
    DC_AgentName,
    DC_AgentId,
    DC_User,
    DC_Computer,
    DC_File,
    DC_Date,
    DC_Size,
    DC_Received,
    DC_Progress,
    DC_ColumnCount
};



class DownloadsFilterProxyModel : public QSortFilterProxyModel
{
Q_OBJECT
    QString filter;
    int     stateFilter = -1;
    bool    searchVisible = false;

    bool matchesTerm(const QString &term, const QString &rowData) const {
        if (term.isEmpty())
            return true;
        QRegularExpression re(QRegularExpression::escape(term.trimmed()), QRegularExpression::CaseInsensitiveOption);
        return rowData.contains(re);
    }

    bool evaluateExpression(const QString &expr, const QString &rowData) const {
        QString e = expr.trimmed();
        if (e.isEmpty())
            return true;

        int depth = 0;
        int lastOr = -1;
        for (int i = e.length() - 1; i >= 0; --i) {
            QChar c = e[i];
            if (c == ')') depth++;
            else if (c == '(') depth--;
            else if (depth == 0 && c == '|') {
                lastOr = i;
                break;
            }
        }
        if (lastOr != -1) {
            QString left = e.left(lastOr).trimmed();
            QString right = e.mid(lastOr + 1).trimmed();
            return evaluateExpression(left, rowData) || evaluateExpression(right, rowData);
        }

        depth = 0;
        int lastAnd = -1;
        for (int i = e.length() - 1; i >= 0; --i) {
            QChar c = e[i];
            if (c == ')') depth++;
            else if (c == '(') depth--;
            else if (depth == 0 && c == '&') {
                lastAnd = i;
                break;
            }
        }
        if (lastAnd != -1) {
            QString left = e.left(lastAnd).trimmed();
            QString right = e.mid(lastAnd + 1).trimmed();
            return evaluateExpression(left, rowData) && evaluateExpression(right, rowData);
        }

        if (e.startsWith("^(") && e.endsWith(')')) {
            return !evaluateExpression(e.mid(2, e.length() - 3), rowData);
        }

        if (e.startsWith('(') && e.endsWith(')')) {
            return evaluateExpression(e.mid(1, e.length() - 2), rowData);
        }

        return matchesTerm(e, rowData);
    }

public:
    explicit DownloadsFilterProxyModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {
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

    void setStateFilter(int state) {
        if (stateFilter == state) return;
        stateFilter = state;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override {
        auto model = sourceModel();
        if (!model)
            return true;

        if (!searchVisible)
            return true;

        if (stateFilter > 0) {
            int state = model->index(row, DC_Progress, parent).data(Qt::UserRole).toInt();
            if (state != stateFilter)
                return false;
        }

        if (!filter.isEmpty()) {
            QString rowData;
            rowData += model->index(row, DC_AgentName, parent).data().toString() + " ";
            rowData += model->index(row, DC_AgentId, parent).data().toString() + " ";
            rowData += model->index(row, DC_User, parent).data().toString() + " ";
            rowData += model->index(row, DC_Computer, parent).data().toString() + " ";
            rowData += model->index(row, DC_File, parent).data().toString() + " ";
            rowData += model->index(row, DC_Date, parent).data().toString() + " ";
            if (!evaluateExpression(filter, rowData))
                return false;
        }

        return true;
    }
};



class DownloadsTableModel : public QAbstractTableModel
{
Q_OBJECT
    QVector<DownloadData> downloads;
    QHash<QString, int>   idToRow;

    void rebuildIndex() {
        idToRow.clear();
        for (int i = 0; i < downloads.size(); ++i)
            idToRow[downloads[i].FileId] = i;
    }

public:
    explicit DownloadsTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    int rowCount(const QModelIndex&) const override { return downloads.size(); }
    int columnCount(const QModelIndex&) const override { return DC_ColumnCount; }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() >= downloads.size())
            return {};

        const DownloadData& d = downloads.at(index.row());

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case DC_FileId:    return d.FileId;
                case DC_AgentName: return d.AgentName;
                case DC_AgentId:   return d.AgentId;
                case DC_User:      return d.User;
                case DC_Computer:  return d.Computer;
                case DC_File:      return d.Filename;
                case DC_Date:      return d.Date;
                case DC_Size:      return BytesToFormat(d.TotalSize);
                case DC_Received:  return d.State == DOWNLOAD_STATE_FINISHED ? "" : BytesToFormat(d.RecvSize);
                case DC_Progress:  return d.State == DOWNLOAD_STATE_FINISHED ? "Finished" : QString();
            }
        }

        if (role == Qt::UserRole) {
            switch (index.column()) {
                case DC_Date:     return d.DateTimestamp;
                case DC_Size:     return static_cast<qint64>(d.TotalSize);
                case DC_Received: return static_cast<qint64>(d.RecvSize);
                case DC_Progress: return d.State;
                default:          return data(index, Qt::DisplayRole);
            }
        }

        if (role == Qt::TextAlignmentRole)
            return Qt::AlignCenter;

        if (role == Qt::ToolTipRole && index.column() == DC_File)
            return d.Filename;

        if (role == Qt::ForegroundRole && index.column() == DC_Progress && d.State == DOWNLOAD_STATE_FINISHED)
            return QColor(COLOR_NeonGreen);

        return {};
    }

    QVariant headerData(int section, Qt::Orientation o, int role) const override {
        if (role != Qt::DisplayRole || o != Qt::Horizontal)
            return {};

        static QStringList headers = {
            "File ID", "Agent Type", "Agent ID", "User", "Computer",
            "File", "Date", "Size", "Received", "Progress"
        };

        return headers.value(section);
    }

    void add(const DownloadData& item) {
        if (idToRow.contains(item.FileId))
            return;

        const int row = downloads.size();
        beginInsertRows(QModelIndex(), row, row);
        downloads.append(item);
        idToRow[item.FileId] = row;
        endInsertRows();
    }

    void update(const QString& fileId, int recvSize, int state) {
        auto it = idToRow.find(fileId);
        if (it == idToRow.end())
            return;

        int row = it.value();
        downloads[row].RecvSize = recvSize;
        downloads[row].State = state;

        if (state == DOWNLOAD_STATE_FINISHED)
            downloads[row].RecvSize = downloads[row].TotalSize;

        Q_EMIT dataChanged(index(row, 0), index(row, DC_ColumnCount - 1));
    }

    void remove(const QStringList& fileIds) {
        if (fileIds.isEmpty() || downloads.isEmpty())
            return;

        QList<int> rowsToRemove;
        rowsToRemove.reserve(fileIds.size());

        for (const QString& id : fileIds) {
            auto it = idToRow.find(id);
            if (it != idToRow.end())
                rowsToRemove.append(it.value());
        }

        if (rowsToRemove.isEmpty())
            return;

        std::sort(rowsToRemove.begin(), rowsToRemove.end(), std::greater<int>());

        for (int row : rowsToRemove) {
            beginRemoveRows(QModelIndex(), row, row);
            idToRow.remove(downloads[row].FileId);
            downloads.removeAt(row);
            endRemoveRows();
        }
        rebuildIndex();
    }

    void clear() {
        beginResetModel();
        downloads.clear();
        idToRow.clear();
        endResetModel();
    }

    const DownloadData* getById(const QString& fileId) const {
        auto it = idToRow.find(fileId);
        if (it == idToRow.end())
            return nullptr;
        return &downloads.at(it.value());
    }

    QString getFileIdAt(int row) const {
        if (row < 0 || row >= downloads.size())
            return {};
        return downloads.at(row).FileId;
    }
};



class ProgressBarDelegate : public QStyledItemDelegate
{
Q_OBJECT

public:
    explicit ProgressBarDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        int state = index.data(Qt::UserRole).toInt();

        if (state == DOWNLOAD_STATE_FINISHED) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        qint64 recvSize  = index.sibling(index.row(), DC_Received).data(Qt::UserRole).toLongLong();
        qint64 totalSize = index.sibling(index.row(), DC_Size).data(Qt::UserRole).toLongLong();

        QStyleOptionProgressBar progressBar;
        progressBar.rect = option.rect.adjusted(2, 2, -2, -2);
        progressBar.minimum = 0;
        progressBar.maximum = 100;
        progressBar.progress = totalSize > 0 ? static_cast<int>(recvSize * 100 / totalSize) : 0;
        progressBar.textVisible = false;

        if (state == DOWNLOAD_STATE_STOPPED)
            progressBar.state = QStyle::State_None;
        else
            progressBar.state = QStyle::State_Enabled;

        QApplication::style()->drawControl(QStyle::CE_ProgressBar, &progressBar, painter);
    }
};



class DownloadsWidget : public DockTab
{
Q_OBJECT
    AdaptixWidget* adaptixWidget  = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QTableView*    tableView      = nullptr;
    QShortcut*     shortcutSearch = nullptr;

    DownloadsTableModel*       downloadsModel = nullptr;
    DownloadsFilterProxyModel* proxyModel     = nullptr;

    QWidget*        searchWidget    = nullptr;
    QHBoxLayout*    searchLayout    = nullptr;
    QLineEdit*      inputFilter     = nullptr;
    QCheckBox*      autoSearchCheck = nullptr;
    QComboBox*      stateComboBox   = nullptr;
    ClickableLabel* hideButton      = nullptr;

    void createUI();

public:
    explicit DownloadsWidget(AdaptixWidget* w);
    ~DownloadsWidget() override;

    void SetUpdatesEnabled(bool enabled);

    void Clear() const;
    void AddDownloadItem(const DownloadData &newDownload);
    void EditDownloadItem(const QString &fileId, int recvSize, int state);
    void RemoveDownloadItem(const QStringList &filesId);

    QString getSelectedFileId() const;
    const DownloadData* getSelectedDownload() const;

public Q_SLOTS:
    void toggleSearchPanel() const;
    void onFilterUpdate() const;
    void onStateFilterUpdate() const;
    void handleDownloadsMenu(const QPoint &pos);
    void actionSync();
    void actionSyncCurl();
    void actionSyncWget();
    void actionDelete();
};

#endif