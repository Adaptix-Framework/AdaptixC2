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

    CommandsGroup regCommandsGroup;
    QVector<CommandsGroup> axCommandsGroup;

    QString         ProcessPreHook(QJSEngine *engine, const Command &command, const QString &agentId, const QString &cmdline, const QJsonObject &jsonObj, QStringList args);
    CommanderResult ProcessCommand(const Command &command, const QString &commandName, QStringList args, QJsonObject jsonObj);
    CommanderResult ProcessHelp(QStringList commandParts);
    QString         GenerateCommandHelp(const Command &command, const QString &parentCommand = "");

public:
    explicit Commander();
    ~Commander() override;

    void AddRegCommands(const CommandsGroup &group);
    void AddAxCommands(const CommandsGroup &group);
    void RemoveAxCommands(const QString &filepath);

    QString GetError();
    QStringList GetCommands();
    CommanderResult ProcessInput(QString agentId, QString cmdline);

Q_SIGNALS:
    void commandsUpdated();
};

#endif
