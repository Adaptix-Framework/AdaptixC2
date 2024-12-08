#ifndef ADAPTIXCLIENT_BROWSERPROCESSWIDGET_H
#define ADAPTIXCLIENT_BROWSERPROCESSWIDGET_H

#include <main.h>
#include <Agent/Agent.h>

typedef struct BrowserProcessData {
    int     pid;
    int     ppid;
    int     sessId;
    QString arch;
    QString context;
    QString process;
} BrowserProcessData;

class BrowserProcessWidget : public QWidget
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
    void setTableProcessData(QMap<int, BrowserProcessData> processMap);
    void setTreeProcessData(QMap<int, BrowserProcessData> processMap);
    void addProcessToTree(QTreeWidgetItem* parent, int parentPID, QMap<int, BrowserProcessData> processMap, QMap<int, QTreeWidgetItem*> *nodeMap);
    void filterTreeWidget(QString filterText);
    void filterTableWidget(QString filterText);

public:
    BrowserProcessWidget(Agent* a);
    ~BrowserProcessWidget();

    void SetProcess(int msgType, QString data);
    void SetStatus(qint64 time, int msgType, QString message);

public slots:
    void onReload();
    void onFilter(QString text);
    void actionCopyPid();
    void handleTableMenu(const QPoint &pos);
    void onTableSelect();
    void onTreeSelect();
};

#endif //ADAPTIXCLIENT_BROWSERPROCESSWIDGET_H
