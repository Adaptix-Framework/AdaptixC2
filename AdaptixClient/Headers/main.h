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
#include <QTableView>
#include <QStandardItemModel>
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

#define FRAMEWORK_VERSION "Adaptix Framework v2.0"
#define SMALL_VERSION     "v2.0"

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
#define TYPE_LOG_BATCH           0x16

#define TYPE_CHAT_MESSAGE  0x18
#define TYPE_CHAT_EDIT     0x19
#define TYPE_CHAT_DELETE   0x1a
#define TYPE_CHAT_REACTION 0x1b
#define TYPE_CHAT_TODO     0x1c

#define TYPE_PLUGIN_SERVICE_DATA  0x1d
#define TYPE_PLUGIN_AGENT_DATA    0x1e
#define TYPE_PLUGIN_LISTENER_DATA 0x1f

#define TYPE_REG_LISTENER 0x21
#define TYPE_REG_AGENT    0x22
#define TYPE_REG_SERVICE  0x23

#define TYPE_LISTENER_START 0x31
#define TYPE_LISTENER_EDIT  0x32
#define TYPE_LISTENER_STOP  0x33

#define TYPE_AGENT_NEW    0x41
#define TYPE_AGENT_UPDATE 0x42
#define TYPE_AGENT_REMOVE 0x43
#define TYPE_AGENT_TICK   0x44
#define TYPE_AGENT_LINK   0x45

#define TYPE_AGENT_TASK_SYNC   0x49
#define TYPE_AGENT_TASK_UPDATE 0x4a
#define TYPE_AGENT_TASK_SEND   0x4b
#define TYPE_AGENT_TASK_REMOVE 0x4c
#define TYPE_AGENT_TASK_HOOK   0x4d

#define TYPE_TRANSFER_CREATE  0x51
#define TYPE_TRANSFER_UPDATE  0x52
#define TYPE_TRANSFER_DELETE  0x53
#define TYPE_TRANSFER_ACTUAL  0x54
#define TYPE_TRANSFER_SET_TAG 0x55

#define TYPE_TUNNEL_CREATE 0x57
#define TYPE_TUNNEL_EDIT   0x58
#define TYPE_TUNNEL_DELETE 0x59
#define TYPE_TUNNEL_ACCEPT 0x5a

#define TYPE_SCREEN_CREATE 0x5b
#define TYPE_SCREEN_UPDATE 0x5c
#define TYPE_SCREEN_DELETE 0x5d

#define TYPE_BROWSER_DISKS   0x61
#define TYPE_BROWSER_FILES   0x62
#define TYPE_BROWSER_STATUS  0x63
#define TYPE_BROWSER_PROCESS 0x64

#define TYPE_AGENT_CONSOLE_LOCAL     0x67
#define TYPE_AGENT_CONSOLE_ERROR     0x68
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

#define TYPE_AXSCRIPT_COMMANDS 0x91
#define TYPE_AXSCRIPT_LIST     0x92
#define TYPE_EVENT_HANDLERS    0x93

#define TYPE_GROUP_CREATE   0xa1
#define TYPE_GROUP_RENAME   0xa2
#define TYPE_GROUP_DELETE   0xa3
#define TYPE_GROUP_MEMBERS  0xa4
#define TYPE_GROUP_REPARENT 0xa5

#define TYPE_PAYLOAD_CREATE  0xb1
#define TYPE_PAYLOAD_UPDATE  0xb2
#define TYPE_PAYLOAD_DELETE  0xb3
#define TYPE_PAYLOAD_EDIT    0xb4
#define TYPE_PAYLOAD_SET_TAG 0xb5

//////////

#define TRANSFER_DOWNLOAD 1
#define TRANSFER_UPLOAD   2

#define TRANSFER_STATE_RUNNING  0x1
#define TRANSFER_STATE_STOPPED  0x2
#define TRANSFER_STATE_FINISHED 0x3
#define TRANSFER_STATE_CANCELED 0x4

#define LOG_STATUS_DEBUG   0
#define LOG_STATUS_INFO    1
#define LOG_STATUS_SUCCESS 2
#define LOG_STATUS_WARN    3
#define LOG_STATUS_ERROR   4

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

typedef struct AxScriptPolicy {
    bool fileRead  = true;
    bool fileWrite = false;
    bool process   = false;
    bool sandboxFs = true;
} AxScriptPolicy;

typedef struct DockLayoutSettings {
    QString layout = QStringLiteral("split_v2");
    QMap<QString, QString> openIn;
    QStringList startup;
} DockLayoutSettings;

typedef struct SettingsData {
    QString MainTheme;
    QString FontFamily;
    int     FontSize;
    QString GraphVersion;
    bool    GraphAutoHideInactive;
    bool    GraphAutoHideNoChilds;
    int     RemoteTerminalBufferSize;
    int     PageSize;
    int     ToolbarPosition; // 0 = top, 1 = bottom, 2 = left, 3 = right
    DockLayoutSettings DockLayout;

    bool ConsoleTime;
    int  ConsoleBufferSize;
    bool ConsoleNoWrap;
    bool ConsoleAutoScroll;
    bool ConsoleShowBackground;
    bool ConsoleUseAppTheme;
    QString ConsoleBgImagePath;
    int     ConsoleBgDimming;
    QString ConsoleTheme;
    bool ConsoleAutoLoadEarlier;
    int  ConsolePageSize;

    bool   SessionsTableColumns[17];
    int    SessionsColumnOrder[17];
    int    SessionsViewMode;
    bool   SessionsAutoHideInactive;
    bool   SessionsCompactMode;
    bool   CheckHealth;
    double HealthCoaf;
    int    HealthOffset;
    double DeadLightnessShift;

    bool TasksTableColumns[11];
    int  TasksViewMode;
    bool TasksInProcessOnly;
    bool TasksCompactMode;

    bool TargetsTableColumns[10];
    int  TargetsViewMode;
    bool TargetsCompactMode;

    bool CredentialsTableColumns[10];
    int  CredentialsViewMode;
    bool CredentialsCompactMode;

    bool FilesTableColumns[11];
    bool FilesCompactMode;

    bool PayloadsTableColumns[14];

    bool TabBlinkEnabled;
    QMap<QString, bool> BlinkWidgets;  // className -> enabled

    AxScriptPolicy ScriptServer;
    AxScriptPolicy ScriptLocal;
    AxScriptPolicy ScriptEditor;
    AxScriptPolicy ScriptEditorAction;
    QString        ScriptSandboxDir;
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
    QString Date;
    qint64  DateTimestamp = 0;
    QString Status;
    QString Tags;
    QString Data;
} ListenerData;

typedef struct AgentData
{
    qint64  Id = 0;
    QString Name;
    QString Listener;
    bool    Async;
    QString ExternalIP;
    QString InternalIP;
    int     GmtOffset;
    int     ACP;
    int     OemCP;
    uint    KillDate;
    uint    WorkingTime;
    int     Sleep;
    int     Jitter;
    QString Pid;
    QString Tid;
    QString Arch;
    bool    Elevated;
    QString Process;
    int     Os;
    QString OsDesc;
    QString Domain;
    QString Computer;
    QString Username;
    QString Impersonated;
    QString Tags;
    QString Mark;
    QString Color;
    qint64  LastTick;
    QString Date;
    qint64  DateTimestamp = 0;
} AgentData;

inline QString formatAgentUserHost(const AgentData& d)
{
    if (d.Username.isEmpty() && d.Computer.isEmpty())
        return QString();

    QString user = d.Username;
    if (d.Elevated)                 user = "* " + user;
    if (!d.Impersonated.isEmpty())  user += " [" + d.Impersonated + "]";

    if (d.Domain.isEmpty() || d.Computer == d.Domain)
        return QString("%1 @ %2").arg(user).arg(d.Computer);
    return QString("%1 @ %2.%3").arg(user).arg(d.Computer).arg(d.Domain);
}

#define TRANSFER_KIND_FILE   0
#define TRANSFER_KIND_MEMORY 1

typedef struct TransferData
{
    int     TransferType;
    qint64  FileId = 0;
    qint64  AgentId = 0;
    QString AgentName;
    QString User;
    QString Computer;
    QString Filename;
    qint64  TotalSize;
    qint64  Progress;
    int     State;
    QString Date;
    qint64  DateTimestamp = 0;
    QString Tag;
    bool    Cancellable = true;
    int     Kind = TRANSFER_KIND_FILE;
    QString ArtifactName;
    QString ArtifactType;
} TransferData;

typedef struct SyncEntryData
{
    QString id;
    int     direction;
    QString filename;
    QString localPath;
    qint64  timestamp;
    qint64  totalSize;
    qint64  progress;
    double  speed;
    int     state;
} SyncEntryData;

typedef struct PayloadData
{
    qint64    PayloadId = 0;
    QString   Name;
    QString   AgentType;
    QString   Artifact;
    QString   Arch;
    QStringList Listeners;
    qint64    Size = 0;
    QString   Sha1;
    QString   Sha256;
    QString   Md5;
    QString   Creator;
    qint64    Created = 0;
    bool      Hidden = false;
    QString   Filename;
    QString   BuildId;
    QString   Watermark;
    QString   ConfigJson;
    QString   Description;
    QString   Tag;
    QString   Uid;
    QString   Color;
    bool      Missing = false;
} PayloadData;

typedef struct ScreenData
{
    qint64     ScreenId = 0;
    qint64     AgentId = 0;
    QString    User;
    QString    Computer;
    QString    Date;
    qint64     DateTimestamp = 0;
    QString    Note;
    QByteArray Content;
} ScreenData;

typedef struct CredentialData
{
    qint64  CredId = 0;
    QString Username;
    QString Password;
    QString Realm;
    QString Type;
    QString Tag;
    QString Date;
    qint64  DateTimestamp = 0;
    QString Storage;
    qint64  AgentId = 0;
    QString Host;
} CredentialData;

typedef struct TargetData
{
    qint64      TargetId = 0;
    QString     Computer;
    QString     Domain;
    QString     Address;
    QString     Tag;
    QIcon       OsIcon;
    int         Os;
    QString     OsDesc;
    QString     Date;
    qint64      DateTimestamp = 0;
    QString     Info;
    bool        Alive;
    QList<qint64> Agents;
} TargetData;

typedef struct TunnelData
{
    qint64  TunnelId = 0;
    qint64  AgentId  = 0;
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
    qint64  DateTimestamp = 0;
    qint64  BytesSent = 0;
    qint64  BytesRecv = 0;
    bool    Active = true;
} TunnelData;

typedef struct TaskData
{
    qint64  TaskId = 0;
    int     TaskType;
    qint64  AgentId = 0;
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
    qint64  ParentAgentId = 0;
    qint64  ChildAgentId = 0;
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


inline qint64 parseI64(const QJsonObject& obj, const QString& key) {
    return obj.value(key).toVariant().toLongLong();
}

inline qint64 parseI64(const QJsonValue& v) {
    return v.toVariant().toLongLong();
}

inline QJsonValue toJsonI64(qint64 v) {
    return QJsonValue::fromVariant(QVariant::fromValue(v));
}

#endif //ADAPTIXCLIENT_MAIN_H
