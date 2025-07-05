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
    bool            is_post_hook;
    QJSValue        post_hook;
};

struct CommandsGroup
{
    QString        groupName;
    QList<Command> commands;
    QJSEngine*     engine;
};

struct CommanderResult
{
    bool        error;
    bool        output;
    QString     message;
    QJsonObject data;
    bool        hooked;
};

class Commander
{
    QString agentType;
    QString listenerType;
    QString error;

    CommandsGroup regCommandsGroup;
    QVector<CommandsGroup> axCommandsGroup;

    void            ProcessPreHook(QJSEngine *engine, const QString &agentId, const Command &command, const QString &cmdline, QStringList args);
    CommanderResult ProcessCommand(Command command, QStringList args, QJsonObject jsonObj);
    CommanderResult ProcessHelp(QStringList commandParts);

public:
    explicit Commander();
    ~Commander();

    void AddRegCommands(const CommandsGroup &group);
    void AddAxCommands(const QString &groupName, const QList<Command> &axCommands, QJSEngine *engine);

    QString GetError();
    QStringList GetCommands();
    CommanderResult ProcessInput(QString agentId, QString cmdline);



    void RemoveAxCommands(const QString &filepath);
};

#endif //ADAPTIXCLIENT_COMMANDER_H
