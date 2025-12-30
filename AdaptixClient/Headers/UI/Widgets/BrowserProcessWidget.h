#ifndef ADAPTIXCLIENT_BROWSERPROCESSWIDGET_H
#define ADAPTIXCLIENT_BROWSERPROCESSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>

class Agent;
class AdaptixWidget;

typedef struct BrowserProcessData {
    int     pid;
    int     ppid;
    int     sessId;
    QString arch;
    QString context;
    QString process;
} BrowserProcessDataWin;

typedef struct BrowserProcessDataUnix {
    int     pid;
    int     ppid;
    QString tty;
    QString context;
    QString process;
} BrowserProcessDataUnix;

class BrowserProcessWidget : public DockTab
{
    QGridLayout*  mainGridLayout    = nullptr;
    QGridLayout*  listGridLayout    = nullptr;
    QTreeWidget*  treeBrowserWidget = nullptr;
    QWidget*      listBrowserWidget = nullptr;
    QTableWidget* tableWidget       = nullptr;
    QLabel*       statusLabel       = nullptr;
    QSplitter*    splitter          = nullptr;
    QLineEdit*    inputFilter       = nullptr;
    QPushButton*  buttonReload      = nullptr;
    QFrame*       line_1            = nullptr;

    Agent*  agent;

    void createUI();
    void setTableProcessDataWin(const QMap<int, BrowserProcessDataWin>& processMap) const;
    void setTableProcessDataUnix(const QMap<int, BrowserProcessDataUnix>& processMap) const;
    void setTreeProcessDataWin(QMap<int, BrowserProcessDataWin> processMap) const;
    static void addProcessToTreeWin(QTreeWidgetItem* parent, int parentPID, QMap<int, BrowserProcessDataWin> processMap, QMap<int, QTreeWidgetItem*> *nodeMap);
    void setTreeProcessDataUnix(QMap<int, BrowserProcessDataUnix> processMap) const;
    static void addProcessToTreeUnix(QTreeWidgetItem* parent, int parentPID, QMap<int, BrowserProcessDataUnix> processMap, QMap<int, QTreeWidgetItem*> *nodeMap);

    void filterTreeWidget(const QString &filterText) const;
    void filterTableWidget(const QString &filterText) const;

public:
    BrowserProcessWidget(AdaptixWidget* w, Agent* a);
    ~BrowserProcessWidget() override;

    void SetProcess(int msgType, const QString &data) const;
    void SetStatus(qint64 time, int msgType, const QString &message) const;

public Q_SLOTS:
    void onReload() const;
    void onFilter(const QString &text) const;
    void actionCopyPid() const;
    void handleTableMenu(const QPoint &pos);
    void onTableSelect() const;
    void onTreeSelect() const;
};

#endif
