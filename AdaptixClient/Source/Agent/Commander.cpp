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



Commander::Commander(){}

Commander::~Commander() = default;

void Commander::AddRegCommands(const CommandsGroup &group) { regCommandsGroup = group; }

void Commander::AddAxCommands(const CommandsGroup &group) {
    axCommandsGroup.append(group);
    emit commandsUpdated();
}

void Commander::RemoveAxCommands(const QString &filepath)
{
    for (int i = 0; i < axCommandsGroup.size(); ++i) {
        if (axCommandsGroup[i].filepath == filepath) {
            axCommandsGroup.removeAt(i);
            i--;
        }
    }
    emit commandsUpdated();
}

CommanderResult Commander::ProcessInput(QString agentId, QString cmdline)
{
    QStringList parts = unserializeParams(cmdline);
    if (parts.isEmpty())
        return CommanderResult{false, true, "", {}, false, {}};

    QString commandName = parts[0];
    parts.removeAt(0);

    if( commandName == "help")
        return this->ProcessHelp(parts);

    for ( auto script_group : axCommandsGroup ) {
        for (Command command : script_group.commands) {
            if (command.name == commandName) {
                QJsonObject jsonObj;
                jsonObj["command"] = command.name;

                if ( command.subcommands.isEmpty() ) {

                    auto cmdResult = ProcessCommand(command, parts, jsonObj);
                    if ( !cmdResult.output && command.is_pre_hook) {
                        QString hook_result = ProcessPreHook(script_group.engine, command, agentId, cmdline, cmdResult.data, parts);
                        if (hook_result.isEmpty()) {
                            return CommanderResult{false, false, "", {}, true, {} };
                        } else {
                            cmdResult.output = true;
                            cmdResult.error = true;
                            cmdResult.message = hook_result;
                        }
                    }
                    return cmdResult;

                }
                else {
                    if ( parts.isEmpty() )
                        return CommanderResult{true, true, "Subcommand must be set", {}, false, {}};

                    QString subCommandName = parts[0];
                    parts.removeAt(0);

                    for (Command subcommand : command.subcommands) {
                        if (subCommandName == subcommand.name) {
                            jsonObj["subcommand"] = subcommand.name;

                            auto cmdResult = ProcessCommand(subcommand, parts, jsonObj);
                            if ( !cmdResult.output && subcommand.is_pre_hook) {
                                QString hook_result = ProcessPreHook(script_group.engine, subcommand, agentId, cmdline, cmdResult.data, parts);
                                if (hook_result.isEmpty()) {
                                    return CommanderResult{false, false, "", {}, true, {} };
                                } else {
                                    cmdResult.output = true;
                                    cmdResult.error = true;
                                    cmdResult.message = hook_result;
                                }
                            }
                            return cmdResult;
                        }
                    }
                    return CommanderResult{true, true, "Subcommand not found", {}, false, {}};
                }
            }
        }
    }

    for (Command command : regCommandsGroup.commands) {
        if (command.name == commandName) {
            QJsonObject jsonObj;
            jsonObj["command"] = command.name;

            if ( command.subcommands.isEmpty() ) {

                auto cmdResult = ProcessCommand(command, parts, jsonObj);
                if ( !cmdResult.output && command.is_pre_hook) {
                    QString hook_result = ProcessPreHook(regCommandsGroup.engine, command, agentId, cmdline, cmdResult.data, parts);
                    if (hook_result.isEmpty()) {
                        return CommanderResult{false, false, "", {}, true, {} };
                    } else {
                        cmdResult.output = true;
                        cmdResult.error = true;
                        cmdResult.message = hook_result;
                    }
                }
                return cmdResult;

            } else {
                if ( parts.isEmpty() )
                    return CommanderResult{true, true, "Subcommand must be set", {}, false, {} };

                QString subCommandName = parts[0];
                parts.removeAt(0);

                for (Command subcommand : command.subcommands) {
                    if (subCommandName == subcommand.name) {
                        jsonObj["subcommand"] = subcommand.name;

                        auto cmdResult = ProcessCommand(subcommand, parts, jsonObj);
                        if ( !cmdResult.output && subcommand.is_pre_hook) {
                            QString hook_result = ProcessPreHook(regCommandsGroup.engine, subcommand, agentId, cmdline, cmdResult.data, parts);
                            if (hook_result.isEmpty()) {
                                return CommanderResult{false, false, "", {}, true, {} };
                            } else {
                                cmdResult.output = true;
                                cmdResult.error = true;
                                cmdResult.message = hook_result;
                            }

                        }
                        return cmdResult;
                    }
                }
                return CommanderResult{true, true, "Subcommand not found", {}, false, {} };
            }
        }
    }

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

CommanderResult Commander::ProcessCommand(Command command, QStringList args, QJsonObject jsonObj)
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

                QFile file(path);
                if (file.open(QIODevice::ReadOnly)) {
                    QByteArray fileData = file.readAll();
                    jsonObj[commandArg.name] = QString::fromLatin1(fileData.toBase64());
                    file.close();
                } else {
                    return CommanderResult{true, true, "Failed to open file: " + path, {}, false, {}};
                }
            }
        } else if (commandArg.required) {
            if ( (commandArg.defaultValue.isNull() || !commandArg.defaultValue.isValid()) && !commandArg.defaultUsed) {
                return CommanderResult{true, true, "Missing required argument: " + commandArg.name, {}, false, {}};
            }
            else {
                if (commandArg.type == "STRING" && commandArg.defaultValue.typeId() == QMetaType::QString) {
                    jsonObj[commandArg.name] = commandArg.defaultValue.toString();
                } else if (commandArg.type == "INT" && commandArg.defaultValue.typeId() == QMetaType::Int) {
                    jsonObj[commandArg.name] = commandArg.defaultValue.toInt();
                } else if (commandArg.type == "BOOL" && commandArg.defaultValue.typeId() == QMetaType::Bool) {
                    jsonObj[commandArg.mark] = commandArg.defaultValue.toBool();
                }
                else {
                    return CommanderResult{true, true, "Missing required argument: " + commandArg.name, {}, false, {}};
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

        for (auto command : regCommandsGroup.commands) {
            QString commandName = command.name;
            if (!command.subcommands.isEmpty())
                commandName += '*';

            QString tab = QString(TotalWidth - commandName.size(), ' ');
            output << "  " + commandName + tab + "      " + command.description + "\n";
        }

        for ( auto script_group : axCommandsGroup ){
            output << QString("\n");
            output << QString("  Group - " + script_group.groupName + "\n");
            output << QString("  =====================================\n");

            for ( auto command : script_group.commands ) {
                QString commandName = command.name;
                if ( command.subcommands.isEmpty() ) {
                    QString tab = QString(TotalWidth - commandName.size(), ' ');
                    output << "  " + commandName + tab + "      " + command.description + "\n";
                }
                else {
                    for ( auto subcmd : command.subcommands ) {
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

        for (Command cmd : regCommandsGroup.commands) {
            if (cmd.name == commandName) {
                foundCommand = cmd;
                break;
            }
        }

        for(auto script_group : axCommandsGroup) {
            if ( !foundCommand.name.isEmpty() )
                break;

            for (Command cmd : script_group.commands) {
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
                    QString fullarg = (arg.required ? "<" : "[") + arg.mark + (arg.mark.isEmpty() || arg.name.isEmpty() ? "" : " ") + arg.name + (arg.required ? ">" : "]");
                    maxArgLength = qMax(maxArgLength, fullarg.size());
                    usageStream << " " + fullarg;
                }

                output << "  Usage                 : " + usageHelp + "\n\n";
                output << "  Arguments:\n";

                for (const auto &arg : foundCommand.args) {
                    QString fullarg = (arg.required ? "<" : "[") + arg.mark + (arg.mark.isEmpty() || arg.name.isEmpty() ? "" : " ") + arg.name + (arg.required ? ">" : "]");
                    QString padding = QString(maxArgLength - fullarg.size(), ' ');
                    output << "    " + fullarg + padding + "  : " + arg.type + (arg.defaultUsed ? " (default: '" + arg.defaultValue.toString() + "'). " : ". ") + arg.description + "\n";
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
                    QString fullarg = (arg.required ? "<" : "[") + arg.mark + (arg.mark.isEmpty() || arg.name.isEmpty() ? "" : " ") + arg.name + (arg.required ? ">" : "]");
                    maxArgLength = qMax(maxArgLength, fullarg.size());
                    usageStream << " " + fullarg;
                }

                output << "  Usage                 : " + usageHelp + "\n\n";
                output << "  Arguments:\n";

                for (const auto &arg : foundSubCommand.args) {
                    QString fullarg = (arg.required ? "<" : "[") + arg.mark + (arg.mark.isEmpty() || arg.name.isEmpty() ? "" : " ") + arg.name + (arg.required ? ">" : "]");
                    QString padding = QString(maxArgLength - fullarg.size(), ' ');
                    output << "    " + fullarg + padding + "  : " + arg.type + (arg.defaultUsed ? " (default: '" + arg.defaultValue.toString() + "'). " : ". ") + arg.description + "\n";
                }
            }
        }
        else {
            return CommanderResult{true, true, "Error Help format: 'help [command [subcommand]]'", {}, false, {}};
        }
        return CommanderResult{false, true, output.readAll(), {}, false, {}};
    }
}

QStringList Commander::GetCommands()
{
    QStringList commandList;
    QStringList helpCommandList;

    for (Command cmd : regCommandsGroup.commands) {

        helpCommandList << "help " + cmd.name;
        if (cmd.subcommands.isEmpty())
            commandList << cmd.name;

        for (Command subcmd : cmd.subcommands) {
            commandList << cmd.name + " " + subcmd.name;
            helpCommandList << "help " + cmd.name + " " + subcmd.name;
        }
    }

    for( auto script_group : axCommandsGroup) {
        for (Command cmd : script_group.commands) {

            helpCommandList << "help " + cmd.name;
            if (cmd.subcommands.isEmpty())
                commandList << cmd.name;

            for (Command subcmd : cmd.subcommands) {
                commandList << cmd.name + " " + subcmd.name;
                helpCommandList << "help " + cmd.name + " " + subcmd.name;
            }
        }
    }

    for( QString cmd : helpCommandList)
        commandList << cmd;

    return commandList;
}
