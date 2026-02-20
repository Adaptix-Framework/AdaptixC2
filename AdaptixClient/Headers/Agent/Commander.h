#ifndef ADAPTIXCLIENT_COMMANDER_H
#define ADAPTIXCLIENT_COMMANDER_H

#include <QJsonArray>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDir>
#include <QJsonObject>
#include <QJSValue>

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
};

struct CommandsGroup
{
    QString        groupName;
    QString        filepath;
    QList<Command> commands;
    QJSEngine*     engine;
};

struct ServerCommandsGroup
{
    QString       scriptName;
    QString       description;
    bool          enabled = true;
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
};



class Commander : public QObject
{
Q_OBJECT

    QString agentType;
    QString listenerType;
    QString error;

    CommandsGroup                       mainCommandsGroup;
    QMap<QString, ServerCommandsGroup>  serverGroups;
    QVector<CommandsGroup>              clientGroups;

    QString         ProcessPreHook(QJSEngine *engine, const Command &command, const QString &agentId, const QString &cmdline, const QJsonObject &jsonObj, QStringList args);
    CommanderResult ProcessCommand(const Command &command, const QString &commandName, QStringList args, QJsonObject jsonObj);
    CommanderResult ProcessInputForGroup(const CommandsGroup &group, const QString &commandName, QStringList args, const QString &agentId, const QString &cmdline);
    CommanderResult ProcessHelp(QStringList commandParts);
    QString         GenerateCommandHelp(const Command &command, const QString &parentCommand = "");

public:
    explicit Commander();
    ~Commander() override;

    void SetAgentType(const QString &type);
    void SetMainCommands(const CommandsGroup &group);

    void AddServerGroup(const QString &scriptName, const QString &description, const CommandsGroup &group);
    void RemoveServerGroup(const QString &scriptName);
    void SetServerGroupEnabled(const QString &scriptName, bool enabled);
    void SetServerGroupEngine(const QString &scriptName, QJSEngine* engine);
    bool IsServerGroupEnabled(const QString &scriptName) const;
    QStringList GetServerGroupNames() const;
    ServerCommandsGroup GetServerGroup(const QString &scriptName) const;

    void AddClientGroup(const CommandsGroup &group);
    void RemoveClientGroup(const QString &filepath);

    QString GetError();
    QStringList GetCommands();
    CommanderResult ProcessInput(QString agentId, QString cmdline);

Q_SIGNALS:
    void commandsUpdated();
};

#endif
