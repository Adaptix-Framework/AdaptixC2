#ifndef AXCOMMANDWRAPPERS_H
#define AXCOMMANDWRAPPERS_H

#include <QJSEngine>
#include <QObject>
#include <QJSValue>
#include <Agent/Commander.h>

class AxCommandWrappers : public QObject {
Q_OBJECT
    Command command;

public:
    explicit AxCommandWrappers(const QString &name, const QString &description, const QString &example, const QString &message, QObject* parent = nullptr);

    Command getCommand() const;

    Q_INVOKABLE void addSubCommands(const QJSValue& array);

    Q_INVOKABLE void addArgBool(const QString &flag, const QString &description = "");
    Q_INVOKABLE void addArgBool(const QString &flag, const QString &description, const QJSValue &value);

    Q_INVOKABLE void addArgInt(const QString &name, bool required = false, const QString &description = "");
    Q_INVOKABLE void addArgInt(const QString &name, const QString &description, const QJSValue &value);
    Q_INVOKABLE void addArgFlagInt(const QString &flag, const QString &name, bool required = false, const QString &description = "");
    Q_INVOKABLE void addArgFlagInt(const QString &flag, const QString &name, const QString &description, const QJSValue &value);

    Q_INVOKABLE void addArgString(const QString &name, bool required = false, const QString &description = "");
    Q_INVOKABLE void addArgString(const QString &name, const QString &description, const QJSValue &value);
    Q_INVOKABLE void addArgFlagString(const QString &flag, const QString &name, bool required = false, const QString &description = "");
    Q_INVOKABLE void addArgFlagString(const QString &flag, const QString &name, const QString &description, const QJSValue &value);

    Q_INVOKABLE void addArgFile(const QString &name, bool required = false, const QString &description = "");
    Q_INVOKABLE void addArgFlagFile(const QString &flag, const QString &name, bool required = false, const QString &description = "");

    Q_INVOKABLE void setPreHook(const QJSValue& handler);

signals:
    void scriptError(const QString &msg);
};





class AxCommandGroupWrapper : public QObject {
Q_OBJECT
    QObject*       parent;
    QString        name;
    QList<Command> commands;
    QJSEngine*     engine;

public:
    explicit AxCommandGroupWrapper(QJSEngine* engine, QObject* parent = nullptr);

    void SetParams(const QString &name, const QJSValue& array);
    QString        getName() const;
    QList<Command> getCommands() const;
    QJSEngine*     getEngine() const;

    Q_INVOKABLE void add(const QJSValue& array);

signals:
    void scriptError(const QString &msg);
};

#endif
