#ifndef ADAPTIXCLIENT_BROWSERFILESWIDGET_H
#define ADAPTIXCLIENT_BROWSERFILESWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>

class Agent;
class FileBrowserTreeItem;
class AdaptixWidget;

class BrowserFileData
{
public:
    bool    Stored = false;
    int     Type;
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
    void CreateBrowserFileData(const QString &path, int os );
};

class FileBrowserTreeItem : public QTreeWidgetItem
{
public:
    BrowserFileData Data;

    explicit FileBrowserTreeItem(const BrowserFileData* d) {
        Data = *d;
        setText(0, Data.Name);
    }
};

class BrowserFilesWidget : public DockTab
{
    QGridLayout*  mainGridLayout    = nullptr;
    QGridLayout*  listGridLayout    = nullptr;
    QTreeWidget*  treeBrowserWidget = nullptr;
    QWidget*      listBrowserWidget = nullptr;
    QTableWidget* tableWidget       = nullptr;
    QLabel*       statusLabel       = nullptr;
    QSplitter*    splitter          = nullptr;
    QLineEdit*    inputPath         = nullptr;
    QPushButton*  buttonParent      = nullptr;
    QPushButton*  buttonReload      = nullptr;
    QPushButton*  buttonUpload      = nullptr;
    QPushButton*  buttonDisks       = nullptr;
    QPushButton*  buttonList        = nullptr;
    QFrame*       line_1            = nullptr;
    QFrame*       line_2            = nullptr;

    Agent* agent;
    QString currentPath;
    QMap<QString, BrowserFileData> browserStore;

    BrowserFileData* getBrowserStore(const QString &path);
    void setBrowserStore(const QString &path, const BrowserFileData &fileData);

    void createUI();
    void setStoredFileData(const QString &path, const BrowserFileData &currenFileData);
    void updateFileData(BrowserFileData* currenFileData, const QString &path, QJsonArray jsonArray);
    void tableShowItems(QVector<BrowserFileData*> files ) const;
    void cdBrowser(const QString &path);

    BrowserFileData  createFileData(const QString &path) const;
    BrowserFileData* getFileData(const QString &path);

public:
    BrowserFilesWidget(AdaptixWidget* w, Agent* a);
    ~BrowserFilesWidget() override;

    void SetDisksWin(qint64 time, int msgType, const QString &message, const QString &data);
    void AddFiles(qint64 time, int msgType, const QString &message, const QString &path, const QString &data);
    void SetStatus(qint64 time, int msgType, const QString &message) const;

public Q_SLOTS:
    void onDisks() const;
    void onList() const;
    void onParent();
    void onReload() const;
    void onUpload() const;
    void handleTableDoubleClicked(const QModelIndex &index);
    void handleTreeDoubleClicked(QTreeWidgetItem* item, int column);
    void handleTableMenu(const QPoint &pos);
};

#endif