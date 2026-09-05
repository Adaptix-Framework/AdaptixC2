#ifndef ADAPTIXCLIENT_COMMANDER_H
#define ADAPTIXCLIENT_COMMANDER_H

#include <QJsonArray>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>
#include <QJSValue>
#include <QMap>
#include <QVector>

struct Argument
{
    QString  type;
    QString  name;
    bool     required;
    bool     flag;
    QString  mark;
    QString  description;
    bool     defaultUsed;
    QVariant defaultValue;
};

struct Command
{
    QString         name;
    QString         message;
    QString         description;
    QString         example;
    QList<Argument> args;
    QList<Command>  subcommands;
    bool            is_pre_hook;
    QJSValue        pre_hook;
    bool            is_post_hook;
    QJSValue        post_hook;
    bool            is_handler;
    QJSValue        handler;
    bool            destructive;
};

struct CommandsGroup
{
    QString        groupName;
    QString        filepath;
    QList<Command> commands;
    QJSEngine*     engine = nullptr;
};

struct MainCommandsGroup
{
    QString       groupId;
    QString       description;
    bool          enabled = true;
    bool          defaultEnabled = true;
    CommandsGroup group;
};

struct ServerCommandsGroup
{
    QString       scriptName;
    QString       description;
    bool          enabled = true;
    bool          defaultEnabled = true;
    CommandsGroup group;
};

struct AxExecutor
{
    bool     isSet;
    QString  engineName;
    QJSValue executor;
};

struct CommanderResult
{
    bool        error;
    bool        output;
    QString     message;
    QJsonObject data;
    bool        is_pre_hook;
    AxExecutor  post_hook;
    AxExecutor  handler;
    bool        styledHelp = false;
};

inline constexpr QChar kHelpInactiveMarker = QChar(0x1e);

class Commander : public QObject
{
Q_OBJECT

    QString agentType;
    QString listenerType;
    QString error;

    QVector<MainCommandsGroup>          mainGroups;
    QMap<QString, ServerCommandsGroup>  serverGroups;
    QVector<CommandsGroup>              clientGroups;

    QString         ProcessPreHook(QJSEngine *engine, const Command &command, qint64 agentId, const QString &cmdline, const QJsonObject &jsonObj, QStringList args);
    CommanderResult ProcessCommand(const Command &command, const QString &commandName, QStringList args, QJsonObject jsonObj);
    CommanderResult ProcessInputForGroup(const CommandsGroup &group, const QString &commandName, QStringList args, qint64 agentId, const QString &cmdline);
    CommanderResult ProcessHelp(QStringList commandParts);
    QString         GenerateCommandHelp(const Command &command, const QString &parentCommand = "");

    void appendHelpCommandLines(QTextStream &output, const QList<Command> &commands, int totalWidth, bool inactive) const;
    bool findCommand(const QString &commandName, Command *out, QString *groupIdOut = nullptr, bool *enabledOut = nullptr) const;

public:
    explicit Commander();
    ~Commander() override;

    void SetAgentType(const QString &type);
    QString AgentType() const { return agentType; }

    void SetMainCommands(const CommandsGroup &group);
    void ClearMainGroups();
    void AddMainGroup(const CommandsGroup &group, const QString &description = QString(), bool defaultEnabled = true);

    void AddServerGroup(const QString &scriptName, const QString &description, const CommandsGroup &group, bool defaultEnabled = true);
    void RemoveServerGroup(const QString &scriptName);
    void SetServerGroupEnabled(const QString &scriptName, bool enabled);
    void SetServerGroupEngine(const QString &scriptName, QJSEngine* engine);
    bool IsServerGroupEnabled(const QString &scriptName) const;
    QStringList GetServerGroupNames() const;
    ServerCommandsGroup GetServerGroup(const QString &scriptName) const;

    void AddClientGroup(const CommandsGroup &group);
    void RemoveClientGroup(const QString &filepath);

    void SetGroupEnabled(const QString &groupId, bool enabled);
    bool IsGroupEnabled(const QString &groupId) const;
    void ApplyGroupEnabledMap(const QMap<QString, bool> &overrides);
    QMap<QString, bool> GetGroupEnabledOverrides() const;
    QStringList GetGroupIds() const;
    QList<QPair<QString, bool>> GetGroupsStatus() const; // id → enabled

    Commander* Clone(QObject *parent = nullptr) const;

    QString GetError();
    QStringList GetCommands();
    QStringList GetHelpCatalog();
    CommanderResult ProcessInput(qint64 agentId, QString cmdline);

Q_SIGNALS:
    void commandsUpdated();
};

#endif
