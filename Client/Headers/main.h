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

#define TYPE_SYNC_START  11
#define TYPE_SYNC_FINISH 12

#define TYPE_CLIENT_CONNECT    21
#define TYPE_CLIENT_DISCONNECT 22

#define TYPE_LISTENER_NEW   31
#define TYPE_LISTENER_START 32
#define TYPE_LISTENER_STOP  33
#define TYPE_LISTENER_EDIT  34

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

#endif //ADAPTIXCLIENT_MAIN_H
