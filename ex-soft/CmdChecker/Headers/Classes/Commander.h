#ifndef COMMANDER_H
#define COMMANDER_H

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
};

struct ExtCommand
{
    QString           name;
    QString           exec;
    QString           message;
    QString           description;
    QString           example;
    QList<Argument>   args;
    QList<ExtCommand> subcommands;
};

struct ExtModule
{
    QString extName;
    QList<ExtCommand> extCommands;
};


struct CommanderResult
{
    bool    output;
    QString message;
    bool    error;
};

class Commander
{
    QList<Command> commands;
    QMap<QString, ExtModule> extModules;
    QString error;

    Argument        ParseArgument(QString argString);
    CommanderResult ProcessCommand(Command command, QStringList args);
    CommanderResult ProcessHelp(QStringList commandParts);

public:
    explicit Commander();
    ~Commander();

    bool AddRegCommands(QByteArray jsonData);
    bool AddExtCommands(QString filepath, QString extName, QList<QJsonObject> extCommands);
    QString     GetError();
    QStringList GetCommands();
    QStringList GetExtCommands();
    CommanderResult ProcessInput(QString input);
};

#endif // COMMANDER_H
