#include <Client/AxScript/AxCommandWrappers.h>

AxCommandWrappers::AxCommandWrappers(const QString &name, const QString &description, const QString &example, const QString &message, QObject* parent) : QObject(parent)
{
    command = {};
    command.name        = name;
    command.description = description;
    command.example     = example;
    command.message     = message;
}

Command AxCommandWrappers::getCommand() const { return this->command; }

void AxCommandWrappers::addSubCommands(const QJSValue& value)
{
    if (value.isUndefined() || value.isNull())
        return;

    if (value.isArray()) {
        const int length = value.property("length").toInt();
        for (int i = 0; i < length; ++i) {
            QJSValue item = value.property(i);

            QObject* obj = item.toQObject();
            if (auto* commandWrapper = qobject_cast<AxCommandWrappers*>(obj)) {
                command.subcommands.append(commandWrapper->getCommand());
            }
            else {
                Q_EMIT scriptError("Item at index " + QString::number(i) + " is not an Command");
            }
        }
    }
    else {
        QObject* obj = value.toQObject();
        if (auto* commandWrapper = qobject_cast<AxCommandWrappers*>(obj)) {
            command.subcommands.append(commandWrapper->getCommand());
        }
        else {
            Q_EMIT scriptError("Item is not an Command");
        }
    }

}

void AxCommandWrappers::addArgBool(const QString &flag, const QJSValue &arg2, const QJSValue &arg3)
{
    Argument arg = { "BOOL", "", false, true, flag, "", false, QVariant() };

    if (arg2.isString()) {
        arg.description = arg2.toString();

        if (!arg3.isUndefined() && !arg3.isNull()) {
            arg.defaultUsed = true;
            arg.defaultValue = arg3.toVariant();
        }
    }

    command.args.append(arg);
}

void AxCommandWrappers::addArgInt(const QString &name, const QJSValue &arg2, const QJSValue &arg3)
{
    Argument arg = { "INT", name, false, false, "", "", false, QVariant() };

    if (arg2.isBool()) {
        arg.required = arg2.toBool();
        arg.description = arg3.isString() ? arg3.toString() : "";
    } else if (arg2.isString()) {
        arg.required = true;
        arg.description = arg2.toString();

        if (!arg3.isUndefined() && !arg3.isNull()) {
            arg.defaultUsed = true;
            arg.defaultValue = arg3.toVariant();
        }
    }

    command.args.append(arg);
}

void AxCommandWrappers::addArgFlagInt(const QString &flag, const QString &name, const QJSValue &arg3, const QJSValue &arg4)
{
    Argument arg = { "INT", name, false, true, flag, "", false, QVariant() };

    if (arg3.isBool()) {
        arg.required = arg3.toBool();
        arg.description = arg4.isString() ? arg4.toString() : "";
    } else if (arg3.isString()) {
        arg.required = true;
        arg.description = arg3.toString();

        if (!arg4.isUndefined() && !arg4.isNull()) {
            arg.defaultUsed = true;
            arg.defaultValue = arg4.toVariant();
        }
    }

    command.args.append(arg);
}

void AxCommandWrappers::addArgString(const QString &name, const QJSValue &arg2, const QJSValue &arg3)
{
    Argument arg = { "STRING", name, true, false, "", "", false, QVariant() };

    if (arg2.isBool()) {
        arg.required = arg2.toBool();
        arg.description = arg3.isString() ? arg3.toString() : "";
    }
    else if (arg2.isString()) {
        arg.description = arg2.toString();

        if (!arg3.isUndefined() && !arg3.isNull()) {
            arg.defaultUsed = true;
            arg.defaultValue = arg3.toVariant();
        }
    }

    command.args.append(arg);
}

void AxCommandWrappers::addArgFlagString(const QString &flag, const QString &name, const QJSValue &arg3, const QJSValue &arg4)
{
    Argument arg = { "STRING", name, false, true, flag, "", false, QVariant() };

    if (arg3.isBool()) {
        arg.required = arg3.toBool();
        arg.description = arg4.isString() ? arg4.toString() : "";
    } else if (arg3.isString()) {
        arg.required = true;
        arg.description = arg3.toString();

        if (!arg4.isUndefined() && !arg4.isNull()) {
            arg.defaultUsed = true;
            arg.defaultValue = arg4.toVariant();
        }
    }

    command.args.append(arg);
}

void AxCommandWrappers::addArgFile(const QString &name, const bool required, const QString &description)
{
    Argument arg = { "FILE", name, required, false, "", description, false, QVariant() };
    command.args.append(arg);
}

void AxCommandWrappers::addArgFlagFile(const QString &flag, const QString &name, const bool required, const QString &description)
{
    Argument arg = { "FILE", name, required, true, flag, description, false, QVariant() };
    command.args.append(arg);
}

void AxCommandWrappers::setPreHook(const QJSValue &handler)
{
    if (!handler.isCallable()) {
        Q_EMIT scriptError("handler is not function");
        return;
    }

    command.is_pre_hook = true;
    command.pre_hook = handler;
}

AxCommandGroupWrapper::AxCommandGroupWrapper(QJSEngine* engine, QObject* parent) : QObject(parent), parent(parent), engine(engine) {}

void AxCommandGroupWrapper::SetParams(const QString &name, const QJSValue &array)
{
    this->name = name;

    if (array.isUndefined() || array.isNull() || !array.isArray()) {
        Q_EMIT scriptError("array is not the Command[]");
        return;
    }

    const int length = array.property("length").toInt();
    for (int i = 0; i < length; ++i) {
        QJSValue item = array.property(i);

        QObject* obj = item.toQObject();
        if (auto* commandWrapper = qobject_cast<AxCommandWrappers*>(obj)) {
            this->commands.append(commandWrapper->getCommand());
        } else {
            Q_EMIT scriptError("Item at index " + QString::number(i) + " is not an Command");
        }
    }
}

QString AxCommandGroupWrapper::getName() const { return name; }

QList<Command> AxCommandGroupWrapper::getCommands() const { return commands; }

QJSEngine* AxCommandGroupWrapper::getEngine() const { return this->engine; }

void AxCommandGroupWrapper::add(const QJSValue &value)
{
    if (value.isUndefined() || value.isNull())
        return;

    if (value.isArray()) {
        const int length = value.property("length").toInt();
        for (int i = 0; i < length; ++i) {
            QJSValue item = value.property(i);

            QObject* obj = item.toQObject();
            if (auto* commandWrapper = qobject_cast<AxCommandWrappers*>(obj)) {
                this->commands.append(commandWrapper->getCommand());
            } else {
                Q_EMIT scriptError("Item at index " + QString::number(i) + " is not an Command");
            }
        }
    }
    else {
        QObject* obj = value.toQObject();
        if (auto* commandWrapper = qobject_cast<AxCommandWrappers*>(obj)) {
            this->commands.append(commandWrapper->getCommand());
        } else {
            Q_EMIT scriptError("Item is not an Command");
        }
    }
}
