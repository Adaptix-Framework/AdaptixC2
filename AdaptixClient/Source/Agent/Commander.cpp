#include <Agent/Commander.h>
#include <QJSEngine>

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

        /* If we encounter a double quote */
        if (c == '"') {
            inQuotes = !inQuotes;
            ++i;
            continue;
        }

        /* If we encounter a backslash, handle escape sequences */
        if (c == '\\') {
            int numBS = 0;
            /*Count the number of consecutive backslashes*/
            while (i < len && commandline[i] == '\\') {
                ++numBS;
                ++i;
            }
            /*Check if the next character is a double quote*/
            if (i < len && commandline[i] == '"') {
                /*Append half the number of backslashes (integer division)*/
                token.append(QString(numBS / 2, '\\'));
                if (numBS % 2 == 0) {
                    /*Even number of backslashes: the quote is not escaped, so it toggles the quote state*/
                    inQuotes = !inQuotes;
                } else {
                    /*Odd number of backslashes: the quote is escaped, add it to the token*/
                    token.append('"');
                }
                ++i;
            } else {
                /*No double quote after backslashes: all backslashes are literal*/
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



Commander::Commander()
{
    mainCommandsGroup = {};
    serverGroups      = {};
    clientGroups      = {};
}

Commander::~Commander() = default;

void Commander::SetAgentType(const QString &type) { agentType = type; }

void Commander::SetMainCommands(const CommandsGroup &group) { mainCommandsGroup = group; }

void Commander::AddServerGroup(const QString &scriptName, const QString &description, const CommandsGroup &group)
{
    ServerCommandsGroup sg;
    sg.scriptName  = scriptName;
    sg.description = description;
    sg.enabled     = true;
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
    if (!serverGroups.contains(scriptName))
        return;
    if (serverGroups[scriptName].enabled == enabled)
        return;
    serverGroups[scriptName].enabled = enabled;
    Q_EMIT commandsUpdated();
}

void Commander::SetServerGroupEngine(const QString &scriptName, QJSEngine* engine)
{
    if (!serverGroups.contains(scriptName))
        return;
    serverGroups[scriptName].group.engine = engine;
}

bool Commander::IsServerGroupEnabled(const QString &scriptName) const
{
    if (!serverGroups.contains(scriptName))
        return false;
    return serverGroups[scriptName].enabled;
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

CommanderResult Commander::ProcessInputForGroup(const CommandsGroup &group, const QString &commandName, QStringList args, const QString &agentId, const QString &cmdline)
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
            return cmdResult;
        }
        return CommanderResult{true, true, "Subcommand not found", {}, false, {}};
    }
    return CommanderResult{false, false, "__not_found__", {}, false, {}};
}

CommanderResult Commander::ProcessInput(QString agentId, QString cmdline)
{
    QStringList parts = unserializeParams(cmdline);
    if (parts.isEmpty())
        return CommanderResult{false, true, "", {}, false, {}};

    QString commandName = parts[0];
    parts.removeAt(0);

    if (commandName == "help")
        return this->ProcessHelp(parts);

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

    auto result = ProcessInputForGroup(mainCommandsGroup, commandName, parts, agentId, cmdline);
    if (result.message != "__not_found__")
        return result;

    return CommanderResult{true, true, "Command not found", {}, false, {}};
}

QString Commander::ProcessPreHook(QJSEngine *engine, const Command &command, const QString &agentId, const QString &cmdline, const QJsonObject &jsonObj, QStringList args)
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
    QString wideKey;

    for (int i = 0; i < args.size(); ++i) {
        QString arg = args[i];

        bool isWideArgs = true;

        for (Argument commandArg : command.args) {
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

    for (Argument commandArg : command.args) {
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
                QString path = parsedArgsMap[commandArg.name];
                if (path.startsWith("~/"))
                    path = QDir::home().filePath(path.mid(2));

                QFileInfo fileInfo(path);
                if (!fileInfo.exists() || !fileInfo.isFile()) {
                    return CommanderResult{true, true, "File not found: " + path, {}, false, {}};
                }

                /// 3 Mb
                if (fileInfo.size() < 3 * 1024 * 1024) {
                    QFile file(path);
                    if (file.open(QIODevice::ReadOnly)) {
                        QByteArray fileData = file.readAll();
                        jsonObj[commandArg.name] = QString::fromLatin1(fileData.toBase64());
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

CommanderResult Commander::ProcessHelp(QStringList commandParts)
{
    QString result;
    QTextStream output(&result);
    if (commandParts.isEmpty()) {
        int TotalWidth = 24;
        output << QString("\n");
        output << QString("  Command                       Description\n");
        output << QString("  -------                       -----------\n");

        for (auto command : mainCommandsGroup.commands) {
            QString commandName = command.name;
            if (!command.subcommands.isEmpty())
                commandName += '*';

            QString tab = QString(TotalWidth - commandName.size(), ' ');
            output << "  " + commandName + tab + "      " + command.description + "\n";
        }

        for (const auto &server_group : serverGroups) {
            if (!server_group.enabled)
                continue;
            if (server_group.group.groupName != agentType)
                continue;

            for (const auto &command : server_group.group.commands) {
                QString commandName = command.name;
                if (command.subcommands.isEmpty()) {
                    QString tab = QString(TotalWidth - commandName.size(), ' ');
                    output << "  " + commandName + tab + "      " + command.description + "\n";
                } else {
                    for (const auto &subcmd : command.subcommands) {
                        QString subcmdName = commandName + " " + subcmd.name;
                        QString tab = QString(TotalWidth - subcmdName.size(), ' ');
                        output << "  " + subcmdName + tab + "      " + subcmd.description + "\n";
                    }
                }
            }
        }

        for (const auto &server_group : serverGroups) {
            if (!server_group.enabled)
                continue;
            if (server_group.group.groupName == agentType)
                continue;

            output << QString("\n");
            output << QString("  Group - " + server_group.group.groupName + "\n");
            output << QString("  =====================================\n");

            for (const auto &command : server_group.group.commands) {
                QString commandName = command.name;
                if (command.subcommands.isEmpty()) {
                    QString tab = QString(TotalWidth - commandName.size(), ' ');
                    output << "  " + commandName + tab + "      " + command.description + "\n";
                } else {
                    for (const auto &subcmd : command.subcommands) {
                        QString subcmdName = commandName + " " + subcmd.name;
                        QString tab = QString(TotalWidth - subcmdName.size(), ' ');
                        output << "  " + subcmdName + tab + "      " + subcmd.description + "\n";
                    }
                }
            }
        }

        for (const auto &client_group : clientGroups) {
            output << QString("\n");
            output << QString("  Group - " + client_group.groupName + " (client)\n");
            output << QString("  =====================================\n");

            for (const auto &command : client_group.commands) {
                QString commandName = command.name;
                if (command.subcommands.isEmpty()) {
                    QString tab = QString(TotalWidth - commandName.size(), ' ');
                    output << "  " + commandName + tab + "      " + command.description + "\n";
                } else {
                    for (const auto &subcmd : command.subcommands) {
                        QString subcmdName = commandName + " " + subcmd.name;
                        QString tab = QString(TotalWidth - subcmdName.size(), ' ');
                        output << "  " + subcmdName + tab + "      " + subcmd.description + "\n";
                    }
                }
            }
        }

        return CommanderResult{false, true, result, {}, false, {}};
    }
    else {
        Command foundCommand;
        QString commandName = commandParts[0];

        for (Command cmd : mainCommandsGroup.commands) {
            if (cmd.name == commandName) {
                foundCommand = cmd;
                break;
            }
        }

        for (const auto &server_group : serverGroups) {
            if ( !foundCommand.name.isEmpty() )
                break;
            if (!server_group.enabled)
                continue;

            for (Command cmd : server_group.group.commands) {
                if (cmd.name == commandName) {
                    foundCommand = cmd;
                    break;
                }
            }
        }

        for (const auto &client_group : clientGroups) {
            if ( !foundCommand.name.isEmpty() )
                break;

            for (Command cmd : client_group.commands) {
                if (cmd.name == commandName) {
                    foundCommand = cmd;
                    break;
                }
            }
        }

        if ( foundCommand.name.isEmpty() )
            return CommanderResult{true, true, "Unknown command: " + commandName, {}, false, {}};

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

    collectCommandsFromGroup(mainCommandsGroup.commands, commandList, helpCommandList);

    for (const auto &server_group : serverGroups) {
        if (server_group.enabled)
            collectCommandsFromGroup(server_group.group.commands, commandList, helpCommandList);
    }

    for (const auto &client_group : clientGroups)
        collectCommandsFromGroup(client_group.commands, commandList, helpCommandList);

    commandList << helpCommandList;
    return commandList;
}
