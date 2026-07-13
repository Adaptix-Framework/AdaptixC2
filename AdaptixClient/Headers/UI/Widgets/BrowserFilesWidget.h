#ifndef ADAPTIXCLIENT_BROWSERFILESWIDGET_H
#define ADAPTIXCLIENT_BROWSERFILESWIDGET_H

#include <main.h>
#include <Utils/FileSystem.h>
#include <UI/Widgets/AbstractDock.h>
#include <oclero/qlementine/widgets/LoadingSpinner.hpp>

class Agent;
class FileBrowserTreeItem;
class AdaptixWidget;

class BrowserFileData
{
public:
    bool    Stored = false;
    int     Type = TYPE_DIR;
    QString Fullpath;
    QString Name;
    QString Size;
    QString Modified;
    QString Mode;
    QString User;
    QString Group;
    QString Status;
    QVector<BrowserFileData*> Files;

    FileBrowserTreeItem* TreeItem = nullptr;

    BrowserFileData() = default;
    ~BrowserFileData() = default;

    void SetType(int type);
    void SetStored(bool stored);
    void CreateBrowserFileData(const QString &path, int os);
};

class FileBrowserTreeItem : public QTreeWidgetItem
{
public:
    QString Fullpath;

    explicit FileBrowserTreeItem(const BrowserFileData* d)
    {
        Fullpath = d->Fullpath;
        setText(0, d->Name);
    }
};

class BrowserFilesWidget : public DockTab
{
    QGridLayout*        mainGridLayout    = nullptr;
    QGridLayout*        listGridLayout    = nullptr;
    QTreeWidget*        treeBrowserWidget = nullptr;
    QWidget*            listBrowserWidget = nullptr;
    QTableView*         tableView         = nullptr;
    QStandardItemModel* tableModel        = nullptr;
    QLabel*             statusLabel       = nullptr;
    oclero::qlementine::LoadingSpinner* loadingSpinner = nullptr;
    QSplitter*          splitter          = nullptr;
    QLineEdit*          inputPath         = nullptr;
    QPushButton*        buttonParent      = nullptr;
    QPushButton*        buttonReload      = nullptr;
    QPushButton*        buttonUpload      = nullptr;
    QPushButton*        buttonDisks       = nullptr;
    QPushButton*        buttonList        = nullptr;
    QFrame*             line_1            = nullptr;
    QFrame*             line_2            = nullptr;

    Agent*  agent = nullptr;
    QString currentPath;
    QMap<QString, BrowserFileData*> browserStore;

    bool isWindows() const;
    QString normalizePath(const QString& path) const;
    QString joinPath(const QString& dir, const QString& name) const;

    BrowserFileData* getBrowserStore(const QString& path) const;
    void removeStoreEntry(const QString& path);
    void removeStoreSubtree(const QString& fullpath);
    void clearBrowserStore();

    void createUI();
    void setStoredFileData(const QString& path, BrowserFileData* fileData);
    void updateFileData(BrowserFileData* currentFileData, const QString& path, const QJsonArray& jsonArray);
    void tableShowItems(const QVector<BrowserFileData*>& files) const;
    void cdBrowser(const QString& path);
    void stopLoading() const;

    BrowserFileData* getFileData(const QString& path);

public:
    BrowserFilesWidget(const AdaptixWidget* w, Agent* a);
    ~BrowserFilesWidget() override;

    void clearAgent();

    void SetDisksWin(qint64 time, int msgType, const QString& message, const QString& data);
    void AddFiles(qint64 time, int msgType, const QString& message, const QString& path, const QString& data);
    void SetStatus(qint64 time, int msgType, const QString& message) const;

public Q_SLOTS:
    void onDisks() const;
    void onList() const;
    void onParent();
    void onReload() const;
    void onUpload() const;
    void handleTableDoubleClicked(const QModelIndex& index);
    void handleTreeDoubleClicked(QTreeWidgetItem* item, int column);
    void handleTableMenu(const QPoint& pos);
};

#endif
