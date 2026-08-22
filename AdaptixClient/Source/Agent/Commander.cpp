#include <Agent/Commander.h>
#include <main.h>

#include <QJSEngine>
#include <QRegularExpression>
#include <QTextStream>

QString serializeParam(const QString &token)
{
    QString result = token;
    result.replace("\\", "\\\\");
    result.replace("\"", "\\\"");
    if (result.contains(' ')) {
        result = "\"" + result + "\"";
    }
    return result;
}

QStringList unserializeParams(const QString &commandline)
{
    QStringList tokens;
    QString token;
    bool inQuotes = false;
    int len = commandline.length();

    for (int i = 0; i < len; ) {
        QChar c = commandline[i];

        if (c.isSpace() && !inQuotes) {
            if (!token.isEmpty()) {
                tokens << token;
                token.clear();
            }
            ++i;
            continue;
        }

        if (c == '"') {
            inQuotes = !inQuotes;
            ++i;
            continue;
        }

        if (c == '\\') {
            int numBS = 0;
            while (i < len && commandline[i] == '\\') {
                ++numBS;
                ++i;
            }
            if (i < len && commandline[i] == '"') {
                token.append(QString(numBS / 2, '\\'));
                if (numBS % 2 == 0) {
                    inQuotes = !inQuotes;
                } else {
                    token.append('"');
                }
                ++i;
            } else {
                token.append(QString(numBS, '\\'));
            }
            continue;
        }

        token.append(c);
        ++i;
    }

    if (!token.isEmpty())
        tokens << token;

    return tokens;
}

static QString inactiveLine(const QString &line)
{
    return QString(kHelpInactiveMarker) + line;
}

Commander::Commander()
{
    mainGroups   = {};
    serverGroups = {};
    clientGroups = {};
}

Commander::~Commander() = default;

void Commander::SetAgentType(const QString &type) { agentType = type; }

void Commander::ClearMainGroups()
{
    mainGroups.clear();
    Q_EMIT commandsUpdated();
}

void Commander::SetMainCommands(const CommandsGroup &group)
{
    mainGroups.clear();
    if (!group.commands.isEmpty() || !group.groupName.isEmpty())
        AddMainGroup(group);
    else
        Q_EMIT commandsUpdated();
}

void Commander::AddMainGroup(const CommandsGroup &group, const QString &description, bool defaultEnabled)
{
    const QString id = group.groupName.isEmpty() ? agentType : group.groupName;
    for (int i = 0; i < mainGroups.size(); ++i) {
        if (mainGroups[i].groupId == id) {
            mainGroups[i].group = group;
            mainGroups[i].group.groupName = id;
            mainGroups[i].description = description;
            mainGroups[i].defaultEnabled = defaultEnabled;
            Q_EMIT commandsUpdated();
            return;
        }
    }
    MainCommandsGroup mg;
    mg.groupId = id;
    mg.description = description;
    mg.enabled = defaultEnabled;
    mg.defaultEnabled = defaultEnabled;
    mg.group = group;
    mg.group.groupName = id;
    mainGroups.append(mg);
    Q_EMIT commandsUpdated();
}

void Commander::AddServerGroup(const QString &scriptName, const QString &description, const CommandsGroup &group, bool defaultEnabled)
{
    ServerCommandsGroup sg;
    sg.scriptName  = scriptName;
    sg.description = description;
    sg.enabled     = defaultEnabled;
    sg.defaultEnabled = defaultEnabled;
    sg.group       = group;
    serverGroups[scriptName] = sg;
    Q_EMIT commandsUpdated();
}

void Commander::RemoveServerGroup(const QString &scriptName)
{
    if (serverGroups.remove(scriptName) > 0)
        Q_EMIT commandsUpdated();
}

void Commander::SetServerGroupEnabled(const QString &scriptName, bool enabled)
{
    SetGroupEnabled(scriptName, enabled);
}

void Commander::SetServerGroupEngine(const QString &scriptName, QJSEngine* engine)
{
    if (!serverGroups.contains(scriptName))
        return;

    if (!engine) {
        for (auto& cmd : serverGroups[scriptName].group.commands) {
            cmd.pre_hook = QJSValue();
            cmd.post_hook = QJSValue();
            cmd.handler = QJSValue();
            cmd.is_pre_hook = false;
            cmd.is_post_hook = false;
            cmd.is_handler = false;
        }
    }
    serverGroups[scriptName].group.engine = engine;
}

bool Commander::IsServerGroupEnabled(const QString &scriptName) const
{
    return IsGroupEnabled(scriptName);
}

QStringList Commander::GetServerGroupNames() const
{
    return serverGroups.keys();
}

ServerCommandsGroup Commander::GetServerGroup(const QString &scriptName) const
{
    return serverGroups.value(scriptName);
}

void Commander::AddClientGroup(const CommandsGroup &group)
{
    for (const auto &existing : clientGroups) {
        if (existing.filepath == group.filepath && existing.groupName == group.groupName)
            return;
    }
    clientGroups.append(group);
    Q_EMIT commandsUpdated();
}

void Commander::RemoveClientGroup(const QString &filepath)
{
    for (int i = 0; i < clientGroups.size(); ++i) {
        if (clientGroups[i].filepath == filepath) {
            clientGroups.removeAt(i);
            i--;
        }
    }
    Q_EMIT commandsUpdated();
}

void Commander::SetGroupEnabled(const QString &groupId, bool enabled)
{
    if (groupId.isEmpty())
        return;

    bool changed = false;
    for (auto &mg : mainGroups) {
        if (mg.groupId == groupId || mg.group.groupName == groupId) {
            if (mg.enabled != enabled) {
                mg.enabled = enabled;
                changed = true;
            }
        }
    }
    if (serverGroups.contains(groupId)) {
        if (serverGroups[groupId].enabled != enabled) {
            serverGroups[groupId].enabled = enabled;
            changed = true;
        }
    } else {
        for (auto it = serverGroups.begin(); it != serverGroups.end(); ++it) {
            if (it->group.groupName == groupId) {
                if (it->enabled != enabled) {
                    it->enabled = enabled;
                    changed = true;
                }
            }
        }
    }
    if (changed)
        Q_EMIT commandsUpdated();
}

bool Commander::IsGroupEnabled(const QString &groupId) const
{
    for (const auto &mg : mainGroups) {
        if (mg.groupId == groupId || mg.group.groupName == groupId)
            return mg.enabled;
    }
    if (serverGroups.contains(groupId))
        return serverGroups[groupId].enabled;
    for (const auto &sg : serverGroups) {
        if (sg.group.groupName == groupId)
            return sg.enabled;
    }
    return true;
}

void Commander::ApplyGroupEnabledMap(const QMap<QString, bool> &overrides)
{
    for (auto &mg : mainGroups)
        mg.enabled = mg.defaultEnabled;
    for (auto it = serverGroups.begin(); it != serverGroups.end(); ++it)
        it->enabled = it->defaultEnabled;

    for (auto it = overrides.begin(); it != overrides.end(); ++it)
        SetGroupEnabled(it.key(), it.value());
}

QMap<QString, bool> Commander::GetGroupEnabledOverrides() const
{
    QMap<QString, bool> m;
    for (const auto &mg : mainGroups) {
        if (mg.enabled != mg.defaultEnabled)
            m[mg.groupId] = mg.enabled;
    }
    for (auto it = serverGroups.begin(); it != serverGroups.end(); ++it) {
        if (it->enabled != it->defaultEnabled)
            m[it.key()] = it->enabled;
    }
    return m;
}

QStringList Commander::GetGroupIds() const
{
    QStringList ids;
    for (const auto &mg : mainGroups)
        ids << mg.groupId;
    for (const auto &k : serverGroups.keys())
        ids << k;
    return ids;
}

QList<QPair<QString, bool>> Commander::GetGroupsStatus() const
{
    QList<QPair<QString, bool>> out;
    for (const auto &mg : mainGroups)
        out.append({mg.groupId, mg.enabled});
    for (auto it = serverGroups.begin(); it != serverGroups.end(); ++it)
        out.append({it.key(), it->enabled});
    return out;
}

Commander* Commander::Clone(QObject *parent) const
{
    auto *c = new Commander();
    if (parent)
        c->setParent(parent);
    c->agentType = agentType;
    c->listenerType = listenerType;
    c->mainGroups = mainGroups;
    c->serverGroups = serverGroups;
    c->clientGroups = clientGroups;
    return c;
}

CommanderResult Commander::ProcessInputForGroup(const CommandsGroup &group, const QString &commandName, QStringList args, qint64 agentId, const QString &cmdline)
{
    for (const Command &command : group.commands) {
        if (command.name != commandName)
            continue;

        QJsonObject jsonObj;
        jsonObj["command"] = command.name;

        if (command.subcommands.isEmpty()) {
            auto cmdResult = ProcessCommand(command, "", args, jsonObj);
            if (!cmdResult.output && command.is_pre_hook && group.engine && command.pre_hook.isCallable()) {
                QString hook_result = ProcessPreHook(group.engine, command, agentId, cmdline, cmdResult.data, args);
                if (hook_result.isEmpty())
                    return CommanderResult{false, false, "", {}, true, {}};
                cmdResult.output = true;
                cmdResult.error = true;
                cmdResult.message = hook_result;
            }
            if (!cmdResult.output && command.is_post_hook && command.post_hook.isCallable())
                cmdResult.post_hook = {true, group.filepath, command.post_hook};
            if (!cmdResult.output && command.is_handler && command.handler.isCallable())
                cmdResult.handler = {true, group.filepath, command.handler};
            return cmdResult;
        }

        if (args.isEmpty())
            return CommanderResult{true, true, "Subcommand must be set" + GenerateCommandHelp(command), {}, false, {}};

        QString subCommandName = args[0];
        args.removeAt(0);

        for (const Command &subcommand : command.subcommands) {
            if (subCommandName != subcommand.name)
                continue;

            jsonObj["subcommand"] = subcommand.name;
            auto cmdResult = ProcessCommand(subcommand, command.name, args, jsonObj);
            if (!cmdResult.output && subcommand.is_pre_hook && group.engine && subcommand.pre_hook.isCallable()) {
                QString hook_result = ProcessPreHook(group.engine, subcommand, agentId, cmdline, cmdResult.data, args);
                if (hook_result.isEmpty())
                    return CommanderResult{false, false, "", {}, true, {}};
                cmdResult.output = true;
                cmdResult.error = true;
                cmdResult.message = hook_result;
            }
            if (!cmdResult.output && subcommand.is_post_hook && subcommand.post_hook.isCallable())
                cmdResult.post_hook = {true, group.filepath, subcommand.post_hook};
            if (!cmdResult.output && subcommand.is_handler && subcommand.handler.isCallable())
                cmdResult.handler = {true, group.filepath, subcommand.handler};
            return cmdResult;
        }
        return CommanderResult{true, true, "Subcommand not found", {}, false, {}};
    }
    return CommanderResult{false, false, "__not_found__", {}, false, {}};
}

CommanderResult Commander::ProcessInput(qint64 agentId, QString cmdline)
{
    QStringList parts = unserializeParams(cmdline);
    if (parts.isEmpty())
        return CommanderResult{false, true, "", {}, false, {}};

    QString commandName = parts[0];
    parts.removeAt(0);

    if (commandName == "help")
        return this->ProcessHelp(parts);

    {
        QString groupId;
        bool enabled = true;
        Command found;
        if (findCommand(commandName, &found, &groupId, &enabled) && !enabled)
            return CommanderResult{true, true, QStringLiteral("Command group '%1' is disabled for this session").arg(groupId), {}, false, {}};
    }

    for (const auto &client_group : clientGroups) {
        auto result = ProcessInputForGroup(client_group, commandName, parts, agentId, cmdline);
        if (result.message != "__not_found__")
            return result;
    }

    for (const auto &server_group : serverGroups) {
        if (!server_group.enabled)
            continue;
        auto result = ProcessInputForGroup(server_group.group, commandName, parts, agentId, cmdline);
        if (result.message != "__not_found__")
            return result;
    }

    for (const auto &mg : mainGroups) {
        if (!mg.enabled)
            continue;
        auto result = ProcessInputForGroup(mg.group, commandName, parts, agentId, cmdline);
        if (result.message != "__not_found__")
            return result;
    }

    return CommanderResult{true, true, "Command not found", {}, false, {}};
}

QString Commander::ProcessPreHook(QJSEngine *engine, const Command &command, qint64 agentId, const QString &cmdline, const QJsonObject &jsonObj, QStringList args)
{
    if (!engine)
        return "Ax Engine is not available";

    QList<QJSValue> jsArgs;
    jsArgs << engine->toScriptValue(agentId);
    jsArgs << engine->toScriptValue(cmdline);
    jsArgs << engine->toScriptValue(jsonObj.toVariantMap());
    for (const QString& arg : args) {
        jsArgs << engine->toScriptValue(arg);
    }

    QJSValue result = command.pre_hook.call(jsArgs);
    if (result.isError()) {
        return  "Error: " + result.property("message").toString();
    }
    return "";
}

QString Commander::GenerateCommandHelp(const Command &command, const QString &parentCommand)
{
    QString result;
    QTextStream output(&result);

    QString fullName = parentCommand.isEmpty() ? command.name : parentCommand + " " + command.name;

    if (!command.subcommands.isEmpty()) {
        output << "\n\n";
        output << "  SubCommands:\n";
        for (const auto &subcmd : command.subcommands) {
            int TotalWidth = 20;
            int cmdWidth = qMin(subcmd.name.size(), TotalWidth);
            QString tab = QString(TotalWidth - cmdWidth, ' ');
            output << "    " + subcmd.name + tab + "  " + subcmd.description + "\n";
        }
    }
    else if (!command.args.isEmpty()) {
        QString usageHelp;
        QTextStream usageStream(&usageHelp);
        usageStream << fullName;

        int maxArgLength = 0;
        for (const auto &arg : command.args) {
            QString fullarg = ((arg.required && !arg.defaultUsed) ? "<" : "[") + arg.mark + (arg.mark.isEmpty() || arg.name.isEmpty() ? "" : " ") + arg.name + ((arg.required && !arg.defaultUsed) ? ">" : "]");
            maxArgLength = qMax(maxArgLength, fullarg.size());
            usageStream << " " + fullarg;
        }

        output << "\n\n";
        output << "  Usage: " + usageHelp + "\n\n";
        output << "  Arguments:\n";

        for (const auto &arg : command.args) {
            QString fullarg = ((arg.required && !arg.defaultUsed) ? "<" : "[") + arg.mark + (arg.mark.isEmpty() || arg.name.isEmpty() ? "" : " ") + arg.name + ((arg.required && !arg.defaultUsed) ? ">" : "]");
            QString padding = QString(maxArgLength - fullarg.size(), ' ');
            output << "    " + fullarg + padding + "  : " + (arg.type + ".").leftJustified(9, ' ') + (arg.defaultUsed ? " (default: '" + arg.defaultValue.toString() + "'). " : " ") + arg.description + "\n";
        }
    }

    return result;
}

CommanderResult Commander::ProcessCommand(const Command &command, const QString &commandName, QStringList args, QJsonObject jsonObj)
{
    QMap<QString, QString> parsedArgsMap;
    QString wideKey = args.isEmpty() ? "" : args[0];

    for (int i = 0; i < args.size(); ++i) {
        QString arg = args[i];

        bool isWideArgs = true;

        for (const Argument& commandArg : command.args) {
            if (commandArg.flag) {
                if (commandArg.type == "BOOL" && commandArg.mark == arg) {
                    parsedArgsMap[commandArg.mark] = "true";
                    wideKey = commandArg.mark;
                    isWideArgs = false;
                    break;
                } else if (commandArg.mark == arg && args.size() > i + 1) {
                    ++i;
                    parsedArgsMap[commandArg.name] = args[i];
                    wideKey = commandArg.name;
                    isWideArgs = false;
                    break;
                }
            } else if (!parsedArgsMap.contains(commandArg.name)) {
                parsedArgsMap[commandArg.name] = arg;
                wideKey = commandArg.name;
                isWideArgs = false;
                break;
            }
        }

        if( isWideArgs ) {
            QString wideStr;
            for(int j = i; j < args.size(); ++j) {
                wideStr += " " + args[j];
            }
            parsedArgsMap[wideKey] += wideStr;
            break;
        }
    }

    for (const Argument& commandArg : command.args) {
        if (parsedArgsMap.contains(commandArg.name) || parsedArgsMap.contains(commandArg.mark)) {
            if (commandArg.type == "STRING") {
                jsonObj[commandArg.name] = parsedArgsMap[commandArg.name];
            }
            else if (commandArg.type == "INT") {
                jsonObj[commandArg.name] = parsedArgsMap[commandArg.name].toInt();
            }
            else if (commandArg.type == "BOOL") {
                jsonObj[commandArg.mark] = parsedArgsMap[commandArg.mark] == "true";
            }
            else if (commandArg.type == "FILE") {
                QString path = parsedArgsMap[commandArg.name].trimmed();

                static const QRegularExpression payloadRe(QStringLiteral("^__payload:#(\\d+)$"), QRegularExpression::CaseInsensitiveOption);
                const QRegularExpressionMatch pm = payloadRe.match(path);
                if (pm.hasMatch()) {
                    const qint64 payloadId = pm.captured(1).toLongLong();
                    if (payloadId <= 0)
                        return CommanderResult{true, true, "Invalid payload id: " + path, {}, false, {}};
                    QJsonObject payloadRef;
                    payloadRef[QStringLiteral("__payload_id")] = toJsonI64(payloadId);
                    jsonObj[commandArg.name] = payloadRef;
                    jsonObj[commandArg.name + QStringLiteral("_path")] = path;
                } else {
                    if (path.startsWith("~/"))
                        path = QDir::home().filePath(path.mid(2));

                    QFileInfo fileInfo(path);
                    if (!fileInfo.exists() || !fileInfo.isFile()) {
                        return CommanderResult{true, true, "File not found: " + path, {}, false, {}};
                    }

                    if (fileInfo.size() < 3 * 1024 * 1024) {
                        QFile file(path);
                        if (file.open(QIODevice::ReadOnly)) {
                            QByteArray fileData = file.readAll();
                            jsonObj[commandArg.name] = QString::fromLatin1(fileData.toBase64());
                            jsonObj[commandArg.name + "_path"] = path;
                            file.close();
                        } else {
                            return CommanderResult{true, true, "Failed to open file: " + path, {}, false, {}};
                        }
                    } else {
                        QJsonObject fileRef;
                        fileRef["__file_path"] = path;
                        fileRef["__file_size"] = fileInfo.size();
                        jsonObj[commandArg.name] = fileRef;
                    }
                }
            }
        } else if (commandArg.required) {
            if (!commandArg.defaultUsed) {
                return CommanderResult{true, true, "Missing required argument: " + commandArg.name + GenerateCommandHelp(command, commandName), {}, false, {}};
            }
            else {
                if (commandArg.type == "STRING" && commandArg.defaultValue.canConvert<QString>()) {
                    jsonObj[commandArg.name] = commandArg.defaultValue.toString();
                } else if (commandArg.type == "INT" && commandArg.defaultValue.canConvert<int>()) {
                    jsonObj[commandArg.name] = commandArg.defaultValue.toInt();
                } else if (commandArg.type == "BOOL" && commandArg.defaultValue.canConvert<bool>()) {
                    jsonObj[commandArg.mark] = commandArg.defaultValue.toBool();
                }
                else {
                    return CommanderResult{true, true, "Missing required argument: " + commandArg.name + GenerateCommandHelp(command, commandName), {}, false, {}};
                }
            }
        }
    }

    QString msg = command.message;
    if( !msg.isEmpty() ) {
        for ( QString k : parsedArgsMap.keys() ) {
            QString param = "<" + k + ">";
            if( msg.contains(param) )
                msg = msg.replace(param, parsedArgsMap[k]);
        }
        jsonObj["message"] = msg;
    }

    return CommanderResult{false, false, "", jsonObj, false, {} };
}

QString Commander::GetError() { return error; }

void Commander::appendHelpCommandLines(QTextStream &output, const QList<Command> &commands, int totalWidth, bool inactive) const
{
    auto emitLine = [&](const QString &line) {
        if (inactive)
            output << inactiveLine(line) << "\n";
        else
            output << line << "\n";
    };

    for (const auto &command : commands) {
        QString commandName = command.name;
        if (command.subcommands.isEmpty()) {
            QString tab = QString(totalWidth - commandName.size(), ' ');
            emitLine("  " + commandName + tab + "      " + command.description);
        } else {
            if (inactive) {
                QString nameStar = commandName + "*";
                QString tab = QString(totalWidth - nameStar.size(), ' ');
                emitLine("  " + nameStar + tab + "      " + command.description);
            }
            for (const auto &subcmd : command.subcommands) {
                QString subcmdName = commandName + " " + subcmd.name;
                QString tab = QString(totalWidth - subcmdName.size(), ' ');
                emitLine("  " + subcmdName + tab + "      " + subcmd.description);
            }
        }
    }
}

bool Commander::findCommand(const QString &commandName, Command *out, QString *groupIdOut, bool *enabledOut) const
{
    for (const auto &mg : mainGroups) {
        for (const Command &cmd : mg.group.commands) {
            if (cmd.name == commandName) {
                if (out) *out = cmd;
                if (groupIdOut) *groupIdOut = mg.groupId;
                if (enabledOut) *enabledOut = mg.enabled;
                return true;
            }
        }
    }
    for (auto it = serverGroups.begin(); it != serverGroups.end(); ++it) {
        for (const Command &cmd : it->group.commands) {
            if (cmd.name == commandName) {
                if (out) *out = cmd;
                if (groupIdOut) *groupIdOut = it.key();
                if (enabledOut) *enabledOut = it->enabled;
                return true;
            }
        }
    }
    for (const auto &cg : clientGroups) {
        for (const Command &cmd : cg.commands) {
            if (cmd.name == commandName) {
                if (out) *out = cmd;
                if (groupIdOut) *groupIdOut = cg.groupName;
                if (enabledOut) *enabledOut = true;
                return true;
            }
        }
    }
    return false;
}

CommanderResult Commander::ProcessHelp(QStringList commandParts)
{
    QString result;
    QTextStream output(&result);
    if (commandParts.isEmpty()) {
        const int TotalWidth = 24;
        bool anyInactive = false;
        output << QString("\n");
        output << QString("  Command                       Description\n");
        output << QString("  -------                       -----------\n");

        for (const auto &mg : mainGroups) {
            if (mg.groupId == agentType || mg.group.groupName == agentType) {
                if (!mg.enabled)
                    anyInactive = true;
                appendHelpCommandLines(output, mg.group.commands, TotalWidth, !mg.enabled);
            }
        }

        for (const auto &server_group : serverGroups) {
            if (server_group.group.groupName != agentType)
                continue;
            if (!server_group.enabled)
                anyInactive = true;
            appendHelpCommandLines(output, server_group.group.commands, TotalWidth, !server_group.enabled);
        }

        for (const auto &mg : mainGroups) {
            if (mg.groupId == agentType || mg.group.groupName == agentType)
                continue;
            if (!mg.enabled)
                anyInactive = true;
            output << QString("\n");
            QString header = mg.enabled ? QStringLiteral("[%1]").arg(mg.groupId) : QStringLiteral("[%1 · off]").arg(mg.groupId);
            if (!mg.enabled)
                output << inactiveLine(header) << "\n";
            else
                output << header << "\n";
            appendHelpCommandLines(output, mg.group.commands, TotalWidth, !mg.enabled);
        }

        for (const auto &server_group : serverGroups) {
            if (server_group.group.groupName == agentType)
                continue;
            if (!server_group.enabled)
                anyInactive = true;
            output << QString("\n");
            QString header = server_group.enabled
                ? QStringLiteral("Group - %1").arg(server_group.group.groupName)
                : QStringLiteral("Group - %1 · off").arg(server_group.group.groupName);
            if (!server_group.enabled)
                output << inactiveLine(header) << "\n" << inactiveLine("=====================================") << "\n";
            else
                output << header << "\n" << "=====================================\n";
            appendHelpCommandLines(output, server_group.group.commands, TotalWidth, !server_group.enabled);
        }

        for (const auto &client_group : clientGroups) {
            output << QString("\n");
            output << QString("Group - " + client_group.groupName + " (client)\n");
            output << QString("=====================================\n");
            appendHelpCommandLines(output, client_group.commands, TotalWidth, false);
        }

        CommanderResult r{false, true, result, {}, false, {}};
        r.styledHelp = anyInactive || result.contains(kHelpInactiveMarker);
        return r;
    }
    else {
        Command foundCommand;
        QString commandName = commandParts[0];
        QString groupId;
        bool enabled = true;

        if (!findCommand(commandName, &foundCommand, &groupId, &enabled))
            return CommanderResult{true, true, "Unknown command: " + commandName, {}, false, {}};

        if (!enabled)
            return CommanderResult{true, true, QStringLiteral("Command group '%1' is disabled for this session").arg(groupId), {}, false, {}};

        if (commandParts.size() == 1) {
            output << QString("\n");
            output << "  Command               : " + foundCommand.name + "\n";
            if(!foundCommand.description.isEmpty())
                output << "  Description           : " + foundCommand.description + "\n";
            if(!foundCommand.example.isEmpty())
                output << "  Example               : " + foundCommand.example + "\n";
            if( !foundCommand.subcommands.isEmpty() ) {
                output << "\n";
                output << "  SubCommand                Description\n";
                output << "  ----------                -----------\n";
                for ( auto subcmd : foundCommand.subcommands ) {
                    int TotalWidth = 20;
                    int cmdWidth = subcmd.name.size();
                    if (cmdWidth > TotalWidth)
                        cmdWidth = TotalWidth;

                    QString tab = QString(TotalWidth - cmdWidth, ' ');
                    output << "  " + subcmd.name + tab + "      " + subcmd.description + "\n";
                }
            }
            else if (!foundCommand.args.isEmpty()) {
                QString usageHelp;
                QTextStream usageStream(&usageHelp);
                usageStream << foundCommand.name;

                int maxArgLength = 0;
                for (const auto &arg : foundCommand.args) {
                    QString fullarg = ((arg.required && !arg.defaultUsed) ? "<" : "[") + arg.mark + (arg.mark.isEmpty() || arg.name.isEmpty() ? "" : " ") + arg.name + ((arg.required && !arg.defaultUsed) ? ">" : "]");
                    maxArgLength = qMax(maxArgLength, fullarg.size());
                    usageStream << " " + fullarg;
                }

                output << "  Usage                 : " + usageHelp + "\n\n";
                output << "  Arguments:\n";

                for (const auto &arg : foundCommand.args) {
                    QString fullarg = ((arg.required && !arg.defaultUsed) ? "<" : "[") + arg.mark + (arg.mark.isEmpty() || arg.name.isEmpty() ? "" : " ") + arg.name + ((arg.required && !arg.defaultUsed) ? ">" : "]");
                    QString padding = QString(maxArgLength - fullarg.size(), ' ');
                    output << "    " + fullarg + padding + "  : " + (arg.type + ".").leftJustified(9, ' ') + (arg.defaultUsed ? " (default: '" + arg.defaultValue.toString() + "'). " : " ") + arg.description + "\n";
                }
            }
        }
        else if (commandParts.size() == 2) {
            Command foundSubCommand;
            QString subCommandName = commandParts[1];
            for (Command subcmd : foundCommand.subcommands) {
                if (subcmd.name == subCommandName) {
                    foundSubCommand = subcmd;
                    break;
                }
            }

            if ( foundSubCommand.name.isEmpty() )
                return CommanderResult{true, true, "Unknown subcommand: " + subCommandName, {}, false, {}};

            output << "  Command               : " + foundCommand.name + " " + foundSubCommand.name +"\n";
            if(!foundSubCommand.description.isEmpty())
                output << "  Description           : " + foundSubCommand.description + "\n";
            if(!foundSubCommand.example.isEmpty())
                output << "  Example               : " + foundSubCommand.example + "\n";
            if (!foundSubCommand.args.isEmpty()) {
                QString usageHelp;
                QTextStream usageStream(&usageHelp);
                usageStream << foundCommand.name + " " + foundSubCommand.name;

                int maxArgLength = 0;
                for (const auto &arg : foundSubCommand.args) {
                    QString fullarg = ((arg.required && !arg.defaultUsed) ? "<" : "[") + arg.mark + (arg.mark.isEmpty() || arg.name.isEmpty() ? "" : " ") + arg.name + ((arg.required && !arg.defaultUsed) ? ">" : "]");
                    maxArgLength = qMax(maxArgLength, fullarg.size());
                    usageStream << " " + fullarg;
                }

                output << "  Usage                 : " + usageHelp + "\n\n";
                output << "  Arguments:\n";

                for (const auto &arg : foundSubCommand.args) {
                    QString fullarg = ((arg.required && !arg.defaultUsed) ? "<" : "[") + arg.mark + (arg.mark.isEmpty() || arg.name.isEmpty() ? "" : " ") + arg.name + ((arg.required && !arg.defaultUsed) ? ">" : "]");
                    QString padding = QString(maxArgLength - fullarg.size(), ' ');
                    output << "    " + fullarg + padding + "  : " + (arg.type + ".").leftJustified(9, ' ') + (arg.defaultUsed ? ".- (default: '" + arg.defaultValue.toString() + "'). " : " ") + arg.description + "\n";
                }
            }
        }
        else {
            return CommanderResult{true, true, "Error Help format: 'help [command [subcommand]]'", {}, false, {}};
        }
        return CommanderResult{false, true, output.readAll(), {}, false, {}};
    }
}

static void collectCommandsFromGroup(const QList<Command> &commands, QStringList &cmdList, QStringList &helpList)
{
    for (const Command &cmd : commands) {
        helpList << "help " + cmd.name;
        if (cmd.subcommands.isEmpty()) {
            cmdList << cmd.name;
        } else {
            for (const Command &subcmd : cmd.subcommands) {
                cmdList << cmd.name + " " + subcmd.name;
                helpList << "help " + cmd.name + " " + subcmd.name;
            }
        }
    }
}

QStringList Commander::GetCommands()
{
    QStringList commandList;
    QStringList helpCommandList;

    for (const auto &mg : mainGroups) {
        if (mg.enabled)
            collectCommandsFromGroup(mg.group.commands, commandList, helpCommandList);
    }

    for (const auto &server_group : serverGroups) {
        if (server_group.enabled)
            collectCommandsFromGroup(server_group.group.commands, commandList, helpCommandList);
    }

    for (const auto &client_group : clientGroups)
        collectCommandsFromGroup(client_group.commands, commandList, helpCommandList);

    commandList << helpCommandList;
    return commandList;
}
