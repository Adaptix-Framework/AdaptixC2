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
                emit consoleError("Item at index " + QString::number(i) + " is not an Command");
            }
        }
    }
    else {
        QObject* obj = value.toQObject();
        if (auto* commandWrapper = qobject_cast<AxCommandWrappers*>(obj)) {
            command.subcommands.append(commandWrapper->getCommand());
        }
        else {
            emit consoleError("Item is not an Command");
        }
    }

}

void AxCommandWrappers::addArgBool(const QString &flag, const QString &description)
{
    Argument arg = { "BOOL", "", false, true, flag, description, false, QVariant() };
    command.args.append(arg);
}

void AxCommandWrappers::addArgBool(const QString &flag, const QString &description, const QJSValue &value)
{
    Argument arg = { "BOOL", "", true, true, flag, description, false, QVariant() };

    if ( !value.isUndefined() && !value.isNull() ) {
        arg.defaultUsed = true;
        arg.defaultValue = value.toVariant();
    }

    command.args.append(arg);
}

void AxCommandWrappers::addArgInt(const QString &name, const bool required, const QString &description)
{
    Argument arg = { "INT", name, required, false, "", description, false, QVariant() };
    command.args.append(arg);
}

void AxCommandWrappers::addArgInt(const QString &name, const QString &description, const QJSValue &value)
{
    Argument arg = { "INT", name, true, false, "", description, false, QVariant() };

    if ( !value.isUndefined() && !value.isNull() ) {
        arg.defaultUsed = true;
        arg.defaultValue = value.toVariant();
    }

    command.args.append(arg);
}

void AxCommandWrappers::addArgFlagInt(const QString &flag, const QString &name, bool required, const QString &description)
{
    Argument arg = { "INT", name, required, true, flag, description, false, QVariant() };
    command.args.append(arg);
}

void AxCommandWrappers::addArgFlaInt(const QString &flag, const QString &name, const QString &description, const QJSValue &value)
{
    Argument arg = { "INT", name, true, true, flag, description, false, QVariant() };

    if ( !value.isUndefined() && !value.isNull() ) {
        arg.defaultUsed = true;
        arg.defaultValue = value.toVariant();
    }

    command.args.append(arg);
}

void AxCommandWrappers::addArgString(const QString &name, bool required, const QString &description)
{
    Argument arg = { "STRING", name, required, false, "", description, false, QVariant() };
    command.args.append(arg);
}

void AxCommandWrappers::addArgString(const QString &name, const QString &description, const QJSValue &value)
{
    Argument arg = { "STRING", name, true, false, "", description, false, QVariant() };

    if ( !value.isUndefined() && !value.isNull() ) {
        arg.defaultUsed = true;
        arg.defaultValue = value.toVariant();
    }

    command.args.append(arg);
}

void AxCommandWrappers::addArgFlagString(const QString &flag, const QString &name, bool required, const QString &description)
{
    Argument arg = { "STRING", name, required, true, flag, description, false, QVariant() };
    command.args.append(arg);
}

void AxCommandWrappers::addArgFlagString(const QString &flag, const QString &name, const QString &description, const QJSValue &value)
{
    Argument arg = { "STRING", name, true, true, flag, description, false, QVariant() };

    if ( !value.isUndefined() && !value.isNull() ) {
        arg.defaultUsed = true;
        arg.defaultValue = value.toVariant();
    }

    command.args.append(arg);
}

void AxCommandWrappers::addArgFile(const QString &name, bool required, const QString &description)
{
    Argument arg = { "STRING", name, required, false, "", description, false, QVariant() };
    command.args.append(arg);
}

void AxCommandWrappers::addArgFlagFile(const QString &flag, const QString &name, bool required, const QString &description)
{
    Argument arg = { "STRING", name, required, true, flag, description, false, QVariant() };
    command.args.append(arg);
}

void AxCommandWrappers::setPreHook(const QJSValue &handler)
{
    if (!handler.isCallable()) {
        emit consoleError("handler is not function");
        return;
    }

    command.is_pre_hook = true;
    command.pre_hook = handler;
}


AxCommandGroupWrapper::AxCommandGroupWrapper(const QString &name, const QJSValue& array, QJSEngine* engine, QObject* parent) : QObject(parent), name(name)
{
    this->parent = parent;

    if (array.isUndefined() || array.isNull() || !array.isArray()) {
        emit consoleError("array is not the Command[]");
        return;
    }

    const int length = array.property("length").toInt();
    for (int i = 0; i < length; ++i) {
        QJSValue item = array.property(i);

        QObject* obj = item.toQObject();
        if (auto* commandWrapper = qobject_cast<AxCommandWrappers*>(obj)) {
            this->commands.append(commandWrapper->getCommand());
        } else {
            emit consoleError("Item at index " + QString::number(i) + " is not an Command");
        }
    }

    this->engine = engine;
}

QString AxCommandGroupWrapper::getName() const { return name; }

QList<Command> AxCommandGroupWrapper::getCommands() const { return commands; }

QJSEngine* AxCommandGroupWrapper::getEngine() const { return this->engine; }

void AxCommandGroupWrapper::add(const QJSValue &value) {
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
                emit consoleError("Item at index " + QString::number(i) + " is not an Command");
            }
        }
    }
    else {
        QObject* obj = value.toQObject();
        if (auto* commandWrapper = qobject_cast<AxCommandWrappers*>(obj)) {
            this->commands.append(commandWrapper->getCommand());
        } else {
            emit consoleError("Item is not an Command");
        }
    }
}
