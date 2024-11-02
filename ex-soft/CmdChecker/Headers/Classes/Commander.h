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
    bool    valid;
};

struct Command
{
    QString name;
    QString description;
    QString example;
    QList<Argument> args;
    QList<Command> subcommands;
};

class Commander
{
    QList<Command> commands;
    QString        error;
    bool           valid = false;

public:
    Commander(QByteArray data);
    ~Commander();

    bool     IsValid();
    QString  GetError();
    Argument parseArgument(QString argString);

    QString parseInput(QString input);
    QString createJson(Command command, QStringList args);
    QString help(QStringList commandParts);
    QStringList getCommandList();
};

#endif // COMMANDER_H
