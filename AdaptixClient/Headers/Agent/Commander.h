#ifndef ADAPTIXCLIENT_COMMANDER_H
#define ADAPTIXCLIENT_COMMANDER_H

#include <main.h>

struct Argument
{
    QString type;
    QString name;
    bool    required;
    bool    flag;
    QString mark;
    QString description;
    QString defaultValue;
    bool    valid;
};

struct Command
{
    QString         name;
    QString         message;
    QString         description;
    QString         example;
    QList<Argument> args;
    QList<Command>  subcommands;
    QString         exec;
    QString         extPath;
};

struct ExtModule
{
    QString extName;
    QList<Command> extCommands;
};

struct CommanderResult
{
    bool    output;
    QString message;
    bool    error;
};

class BofPacker
{
public:
    QByteArray data;
    void Pack(const QString &type, const QJsonValue &jsonValue);
    QString Build() const;
};

class Commander
{
    QList<Command> commands;
    QMap<QString, ExtModule> extModules;
    QString error;

    Command         ParseCommand(QJsonObject jsonObject);
    Argument        ParseArgument(const QString &argString);
    CommanderResult ProcessCommand(AgentData agentData, Command command, QStringList args);

    static QString  ProcessExecExtension(const AgentData &agentData, const QString &filepath, QString ExecString, QList<Argument> args, QJsonObject jsonObj);
    CommanderResult ProcessHelp(QStringList commandParts);

public:
    explicit Commander();
    ~Commander();

    bool AddRegCommands(const QByteArray &jsonData);
    bool AddExtCommands(const QString &filepath, const QString &extName, QList<QJsonObject> extCommands);
    void RemoveExtCommands(const QString &filepath);
    QString GetError();
    QStringList GetCommands();
    CommanderResult ProcessInput(AgentData agentData, QString input);
};

#endif //ADAPTIXCLIENT_COMMANDER_H
