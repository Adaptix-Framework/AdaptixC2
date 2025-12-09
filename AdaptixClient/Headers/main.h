#ifndef ADAPTIXCLIENT_MAIN_H
#define ADAPTIXCLIENT_MAIN_H

#include <QApplication>
#include <QMap>
#include <QVector>
#include <QShortcut>
#include <QGridLayout>
#include <QWidget>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QPushButton>
#include <QMenu>
#include <QRegularExpression>
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
#include <QListWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QtWebSockets/QWebSocket>
#include <QTimer>
#include <QCompleter>
#include <QFileDialog>
#include <QInputDialog>
#include <QTreeWidget>
#include <QClipboard>
#include <QSplashScreen>
#include <QStyledItemDelegate>
#include <QStringListModel>
#include <QSpinBox>
#include <QColorDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QPainter>
#include <QScrollBar>
#include <QDateTimeEdit>
#include <QButtonGroup>
#include <QTcpServer>
#include <QQueue>
#include <QMutex>

#include <Utils/Logs.h>
#include <Utils/FileSystem.h>
#include <Utils/Convert.h>

#define FRAMEWORK_VERSION "Adaptix Framework v1.0"
#define SMALL_VERSION "v1.0"

///////////

#define OS_UNKNOWN 0
#define OS_WINDOWS 1
#define OS_LINUX   2
#define OS_MAC     3

//////////

#define EVENT_CLIENT_CONNECT    1
#define EVENT_CLIENT_DISCONNECT 2
#define EVENT_LISTENER_START    3
#define EVENT_LISTENER_STOP     4
#define EVENT_AGENT_NEW         5
#define EVENT_TUNNEL_START      6
#define EVENT_TUNNEL_STOP       7

/////////

#define TYPE_SYNC_START  0x11
#define TYPE_SYNC_FINISH 0x12

#define SP_TYPE_EVENT            0x13
#define TYPE_SYNC_BATCH          0x14
#define TYPE_SYNC_CATEGORY_BATCH 0x15

#define TYPE_CHAT_MESSAGE 0x18

#define TYPE_LISTENER_REG   0x31
#define TYPE_LISTENER_START 0x32
#define TYPE_LISTENER_STOP  0x33
#define TYPE_LISTENER_EDIT  0x34

#define TYPE_AGENT_REG         0x41
#define TYPE_AGENT_NEW         0x42
#define TYPE_AGENT_TICK        0x43
#define TYPE_AGENT_UPDATE      0x44
#define TYPE_AGENT_LINK        0x45
#define TYPE_AGENT_REMOVE      0x46

#define TYPE_AGENT_TASK_SYNC   0x49
#define TYPE_AGENT_TASK_UPDATE 0x4a
#define TYPE_AGENT_TASK_SEND   0x4b
#define TYPE_AGENT_TASK_REMOVE 0x4c
#define TYPE_AGENT_TASK_HOOK   0x4d

#define TYPE_DOWNLOAD_CREATE 0x51
#define TYPE_DOWNLOAD_UPDATE 0x52
#define TYPE_DOWNLOAD_DELETE 0x53

#define TYPE_TUNNEL_CREATE 0x57
#define TYPE_TUNNEL_EDIT   0x58
#define TYPE_TUNNEL_DELETE 0x59

#define TYPE_SCREEN_CREATE 0x5b
#define TYPE_SCREEN_UPDATE 0x5c
#define TYPE_SCREEN_DELETE 0x5d

#define TYPE_BROWSER_DISKS   0x61
#define TYPE_BROWSER_FILES   0x62
#define TYPE_BROWSER_STATUS  0x63
#define TYPE_BROWSER_PROCESS 0x64

#define TYPE_AGENT_CONSOLE_OUT       0x69
#define TYPE_AGENT_CONSOLE_TASK_SYNC 0x6a
#define TYPE_AGENT_CONSOLE_TASK_UPD  0x6b

#define TYPE_PIVOT_CREATE 0x71
#define TYPE_PIVOT_DELETE 0x72

#define TYPE_CREDS_CREATE  0x81
#define TYPE_CREDS_EDIT    0x82
#define TYPE_CREDS_DELETE  0x83
#define TYPE_CREDS_SET_TAG 0x84

#define TYPE_TARGETS_CREATE  0x87
#define TYPE_TARGETS_EDIT    0x88
#define TYPE_TARGETS_DELETE  0x89
#define TYPE_TARGETS_SET_TAG 0x8a

//////////

#define DOWNLOAD_STATE_RUNNING  0x1
#define DOWNLOAD_STATE_STOPPED  0x2
#define DOWNLOAD_STATE_FINISHED 0x3
#define DOWNLOAD_STATE_CANCELED 0x4

/////////

#define COLOR_Black           "#000000"     /// black
#define COLOR_NeonGreen       "#39FF14"     /// green
#define COLOR_KellyGreen      "#4CBB17"     /// green
#define COLOR_Green           "#008000"     /// green
#define COLOR_Berry           "#A01641"     /// red
#define COLOR_ChiliPepper     "#E32227"     /// red
#define COLOR_BrightOrange    "#FFA500"     /// orange
#define COLOR_PastelYellow    "#FDFD96"     /// yellow
#define COLOR_Yellow          "#FFFF00"     /// yellow
#define COLOR_BabyBlue        "#89CFF0"     /// blue
#define COLOR_Purple          "#800080"     /// purple
#define COLOR_DarkBrownishRed "#4A403D"     /// gray-red
#define COLOR_LightGray       "#A0A0A0"     /// gray
#define COLOR_Gray            "#808080"     /// gray
#define COLOR_SaturGray       "#606060"     /// gray
#define COLOR_ConsoleWhite    "#E0E0E0"     /// white
#define COLOR_White           "#FFFFFF"     /// white

//////////

class AxContainerWrapper;

//////////

typedef struct SettingsData {
    QString MainTheme;
    QString FontFamily;
    int     FontSize;
    QString GraphVersion;
    int     RemoteTerminalBufferSize;

    bool ConsoleTime;
    int  ConsoleBufferSize;
    bool ConsoleNoWrap;
    bool ConsoleAutoScroll;

    bool   SessionsTableColumns[15];
    bool   CheckHealth;
    double HealthCoaf;
    int    HealthOffset;

    bool TasksTableColumns[11];

    bool TabBlinkEnabled;
    QMap<QString, bool> BlinkWidgets;  // className -> enabled
} SettingsData;

typedef struct AxUI
{
    AxContainerWrapper* container;
    QWidget*            widget;
    int                 height;
    int                 width;
} AxUI;

/// Object Data

typedef struct ListenerData
{
    QString Name;
    QString ListenerRegName;
    QString ListenerProtocol;
    QString ListenerType;
    QString BindHost;
    QString BindPort;
    QString AgentAddresses;
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
    uint        KillDate;
    uint        WorkingTime;
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
    QString     Impersonated;
    QString     Tags;
    QString     Mark;
    QString     Color;
    int         LastTick;
} AgentData;

typedef struct DownloadData
{
    QString FileId;
    QString AgentId;
    QString AgentName;
    QString User;
    QString Computer;
    QString Filename;
    int     TotalSize;
    int     RecvSize;
    int     State;
    QString Date;
} DownloadData;

typedef struct ScreenData
{
    QString    ScreenId;
    QString    User;
    QString    Computer;
    QString    Date;
    QString    Note;
    QByteArray Content;
} ScreenData;

typedef struct CredentialData
{
    QString CredId;
    QString Username;
    QString Password;
    QString Realm;
    QString Type;
    QString Tag;
    QString Date;
    QString Storage;
    QString AgentId;
    QString Host;
} CredentialData;

typedef struct TargetData
{
    QString     TargetId;
    QString     Computer;
    QString     Domain;
    QString     Address;
    QString     Tag;
    QIcon       OsIcon;
    int         Os;
    QString     OsDesc;
    QString     Date;
    QString     Info;
    bool        Alive;
    QStringList Agents;
} TargetData;

typedef struct TunnelData
{
    QString TunnelId;
    QString AgentId;
    QString Computer;
    QString Username;
    QString Process;
    QString Type;
    QString Info;
    QString Interface;
    QString Port;
    QString Client;
    QString Fport;
    QString Fhost;
} TunnelData;

typedef struct TaskData
{
    QString TaskId;
    int     TaskType;
    QString AgentId;
    QString Client;
    QString User;
    QString Computer;
    qint64  StartTime;
    qint64  FinishTime;
    QString CommandLine;
    int     MessageType;
    QString Status;
    QString Message;
    QString Output;
    bool    Completed;
} TaskData;

typedef struct PivotData
{
    QString PivotId;
    QString PivotName;
    QString ParentAgentId;
    QString ChildAgentId;
} PivotData;

typedef struct ExtensionFile
{
    QString Name;
    QString FilePath;
    QString Code;
    QString Description;
    QString Message;
    bool    Enabled;
    bool    NoSave;
    bool    Valid;

    QMap<QString, QVector<QJsonObject> > ExCommands;
} ExtensionFile;

#endif //ADAPTIXCLIENT_MAIN_H
