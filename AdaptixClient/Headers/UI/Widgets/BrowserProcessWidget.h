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
    void setTableProcessData(QMap<int, BrowserProcessData> processMap) const;
    void setTreeProcessData(QMap<int, BrowserProcessData> processMap) const;

    static void addProcessToTree(QTreeWidgetItem* parent, int parentPID, QMap<int, BrowserProcessData> processMap, QMap<int, QTreeWidgetItem*> *nodeMap);
    void filterTreeWidget(const QString &filterText) const;
    void filterTableWidget(const QString &filterText) const;

public:
    BrowserProcessWidget(Agent* a);
    ~BrowserProcessWidget() override;

    void SetProcess(int msgType, const QString &data) const;
    void SetStatus(qint64 time, int msgType, const QString &message) const;

public slots:
    void onReload() const;
    void onFilter(const QString &text) const;
    void actionCopyPid() const;
    void handleTableMenu(const QPoint &pos);
    void onTableSelect() const;
    void onTreeSelect() const;
};

#endif //ADAPTIXCLIENT_BROWSERPROCESSWIDGET_H
