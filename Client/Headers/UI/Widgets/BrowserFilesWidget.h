#ifndef ADAPTIXCLIENT_BROWSERFILESWIDGET_H
#define ADAPTIXCLIENT_BROWSERFILESWIDGET_H

#include <main.h>
#include <Agent/Agent.h>

typedef struct BrowserFileData
{
    QString Path;
    QString Type;
    QString Modified;
    QVector<BrowserFileData> Files;
} BrowserFileData;

class BrowserFilesWidget : public QWidget
{
    QGridLayout*  mainGridLayout    = nullptr;
    QGridLayout*  listGridLayout    = nullptr;
    QTreeWidget*  treeBrowserWidget = nullptr;
    QWidget*      listBrowserWidget = nullptr;
    QTableWidget* tableWidget       = nullptr;
    QLabel*       statusLabel       = nullptr;
    QSplitter*    splitter          = nullptr;
    QLineEdit*    pathInput         = nullptr;
    QPushButton*  buttonHomeDir     = nullptr;
    QPushButton*  buttonReload      = nullptr;
    QPushButton*  buttonUpload      = nullptr;
    QPushButton*  buttonDisks       = nullptr;
    QPushButton*  buttonCd          = nullptr;
    QFrame*       line_1            = nullptr;
    QFrame*       line_2            = nullptr;

    Agent* agent;

    void TreeAddData(BrowserFileData data);
    void createUI();

public:
    BrowserFilesWidget(Agent* a);
    ~BrowserFilesWidget();

    void SetDisks(qint64 time, int msgType, QString message, QString data);

public slots:
    void onDisks();
};

#endif //ADAPTIXCLIENT_BROWSERFILESWIDGET_H