#ifndef ADAPTIXCLIENT_MAIN_H
#define ADAPTIXCLIENT_MAIN_H

#include <QApplication>
#include <QMap>
#include <QVector>
#include <QGridLayout>
#include <QWidget>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QMenu>
#include <QMenuBar>
#include <QDir>
#include <QMessageBox>
#include <QHeaderView>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QFontDatabase>
#include <QNetworkAccessManager>
#include <QSslConfiguration>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkReply>
#include <QMainWindow>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QThread>
#include <QProgressBar>
#include <QComboBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QtWebSockets/QWebSocket>

#include <Utils/Logs.h>
#include <Utils/FileSystem.h>
#include <Utils/Convert.h>

#define FRAMEWORK_VERSION "Adaptix Framework v.0.1-DEV"

//////////

#define TYPE_SYNC_START  0x11
#define TYPE_SYNC_FINISH 0x12

#define TYPE_CLIENT_CONNECT    0x21
#define TYPE_CLIENT_DISCONNECT 0x22

#define TYPE_LISTENER_REG   0x31
#define TYPE_LISTENER_START 0x32
#define TYPE_LISTENER_STOP  0x33
#define TYPE_LISTENER_EDIT  0x34

#define TYPE_AGENT_REG 0x41
#define TYPE_AGENT_NEW 0x42

//////////

#define COLOR_NeonGreen    "#39FF14"     // green
#define COLOR_Berry        "#A01641"     // red
#define COLOR_ChiliPepper  "#E32227"     // red
#define COLOR_BrightOrange "#FFA500"     // orange

//////////

typedef struct ListenerData
{
    QString ListenerName;
    QString ListenerType;
    QString BindHost;
    QString BindPort;
    QString AgentHost;
    QString AgentPort;
    QString Status;
    QString Data;
} ListenerData;

typedef struct AgentData
{
    QString     Id;
    QString     Name;
    QString     Listener;
    bool        Async;
    QString     ExternalIP;
    QString     InternalIP;
    int         GmtOffset;
    int         Sleep;
    int         Jitter;
    QString     Pid;
    QString     Tid;
    QString     Arch;
    bool        Elevated;
    QString     Process;
    int         Os;
    QString     OsDesc;
    QString     Domain;
    QString     Computer;
    QString     Username;
    QStringList Tags;
} AgentData;

#endif //ADAPTIXCLIENT_MAIN_H
