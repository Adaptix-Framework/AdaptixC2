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

struct CommanderResult
{
    bool    output;
    QString message;
    bool    error;
};

class Commander
{
    QList<Command> commands;
    QString        error;
    bool           valid = false;

    Argument        ParseArgument(QString argString);
    CommanderResult ProcessCommand(Command command, QStringList args);
    CommanderResult ProcessHelp(QStringList commandParts);

public:
    explicit Commander(const QByteArray& data);
    ~Commander();

    bool            IsValid();
    QString         GetError();
    QStringList     GetCommands();
    CommanderResult ProcessInput(QString input);
};

#endif // COMMANDER_H
