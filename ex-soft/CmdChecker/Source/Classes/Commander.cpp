#include <Classes/Commander.h>

Commander::Commander(QByteArray data)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        error = "Error JSON file";
        valid = false;
        return;
    }

    QJsonArray commandArray = doc.array();
    for (QJsonValue value : commandArray) {
        QJsonObject obj = value.toObject();

        Command cmd;
        cmd.name        = obj["command"].toString();
        cmd.description = obj["description"].toString();
        cmd.example     = obj["example"].toString();

        if (obj.contains("args")) {
            QJsonArray argsArray = obj["args"].toArray();
            for (QJsonValue argVal : argsArray) {
                Argument arg = parseArgument(argVal.toString());
                if (arg.valid)
                    cmd.args.append(arg);
            }
        } else if (obj.contains("subcommands")) {
            QJsonArray subcommandArray = obj["subcommands"].toArray();
            for (QJsonValue subCmdVal : subcommandArray) {
                QJsonObject subCmdObj = subCmdVal.toObject();
                Command subCmd;
                subCmd.name = subCmdObj["name"].toString();
                subCmd.description = subCmdObj["description"].toString();
                subCmd.example = subCmdObj["example"].toString();

                QJsonArray subArgsArray = subCmdObj["args"].toArray();
                for (QJsonValue subArgVal : subArgsArray) {
                    Argument subArg = parseArgument(subArgVal.toString());
                    if (subArg.valid)
                        subCmd.args.append(subArg);
                }
                cmd.subcommands.append(subCmd);
            }
        }
        commands.append(cmd);
    }
    valid = true;
}

Commander::~Commander() = default;

QString Commander::parseInput(QString input)
{
    QStringList parts;
    QRegularExpression regex(R"((?:\"((?:\\.|[^\"\\])*)\"|(\S+)))");
    QRegularExpressionMatchIterator it = regex.globalMatch(input);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        if (match.captured(1).isEmpty()) {
            parts.append(match.captured(2));
        } else {
            parts.append(match.captured(1));
        }
    }

    if (parts.isEmpty())
        return "";

    QString commandName = parts[0];
    parts.removeAt(0);

    if( commandName == "help") {
        return this->help(parts);
    }

    for (Command command : commands) {
        if (command.name == commandName) {
            return createJson(command, parts);
        }
    }

    return "Command not found.";
}

Argument Commander::parseArgument(QString argString)
{
    Argument arg = {0};
    QRegularExpression regex(R"((\w+)\s+([\[\<][^\s\]]+[\s\w-]*[\>\]])(?:\s+\{(.+)\})?)");
    QRegularExpressionMatch match = regex.match(argString);

    if ( !match.hasMatch())
        return arg;

    arg.type             = match.captured(1);
    arg.description      = match.captured(3).trimmed();
    QString flagAndValue = match.captured(2).trimmed();

    if (flagAndValue.startsWith("<") && flagAndValue.endsWith(">"))
        arg.required = true;
    else if (flagAndValue.startsWith("[") && flagAndValue.endsWith("]"))
        arg.required = false;
    else
        arg.valid = false;

    int spaceIndex = flagAndValue.indexOf(' ');
    if (spaceIndex != -1) {
        arg.mark = flagAndValue.mid(1, spaceIndex - 1).trimmed();
        arg.name = flagAndValue.mid(spaceIndex + 1, flagAndValue.size() - spaceIndex - 2).trimmed();
        arg.flag = true;
    }
    else {
        QString value = flagAndValue.mid(1, flagAndValue.size() - 2).trimmed();
        if( value.startsWith("-") ) {
            arg.mark = value;
            arg.flag = true;
        }
        else {
            arg.name = value;
        }
    }
    arg.valid = true;
    return arg;
}

QString Commander::createJson(Command command, QStringList args)
{
    QJsonObject jsonObj;
    jsonObj["command"] = command.name;

    if ( command.subcommands.size() == 0 ) {

        QMap<QString, QString> parsedArgsMap;

        for (int i = 0; i < args.size(); ++i) {
            QString arg = args[i];

            for (Argument commandArg : command.args) {
                if (commandArg.flag && commandArg.mark == arg && args.size() > i+1 ) {
                    if( commandArg.type == "BOOL" ) {
                        parsedArgsMap[commandArg.mark] = "true";
                    }
                    else {
                        ++i;
                        parsedArgsMap[commandArg.name] = args[i];
                    }
                    break;
                } else if (!commandArg.flag && !parsedArgsMap.contains(commandArg.name)) {
                    parsedArgsMap[commandArg.name] = arg;
                    break;
                }
            }
        }

        for (Argument commandArg : command.args) {
            if (parsedArgsMap.contains(commandArg.name)) {
                if (commandArg.type == "STRING") {
                    jsonObj[commandArg.name] = parsedArgsMap[commandArg.name];
                } else if (commandArg.type == "INT") {
                    jsonObj[commandArg.name] = parsedArgsMap[commandArg.name].toInt();
                } else if (commandArg.type == "BOOL") {
                    jsonObj[commandArg.mark] = parsedArgsMap[commandArg.mark] == "true";
                } else if (commandArg.type == "FILE") {
                    QFile file(parsedArgsMap[commandArg.name]);
                    if (file.open(QIODevice::ReadOnly)) {
                        QByteArray fileData = file.readAll();
                        jsonObj[commandArg.name] = QString::fromLatin1(fileData.toBase64());
                        file.close();
                    } else {
                        return QString("Failed to open file: %1").arg(parsedArgsMap[commandArg.name]);
                    }
                }
            } else if (commandArg.required) {
                return "Missing required argument: " + commandArg.name;
            }
        }
    }
    else {
        if ( args.isEmpty() )
            return "Subcommand must be set";

        QString subCommandName = args[0];

        for (Command subcommand : command.subcommands) {
            if (subCommandName == subcommand.name) {
                jsonObj["subcommand"] = subcommand.name;

                QMap<QString, QString> parsedArgsMap;

                for (int i = 1; i < args.size(); ++i) {
                    QString arg = args[i];

                    for (Argument commandArg : subcommand.args) {
                        if (commandArg.flag && commandArg.mark == arg && args.size() > i+1 ) {
                            if( commandArg.type == "BOOL" ) {
                                parsedArgsMap[commandArg.mark] = "true";
                            }
                            else {
                                ++i;
                                parsedArgsMap[commandArg.name] = args[i];
                            }
                            break;
                        } else if (!commandArg.flag && !parsedArgsMap.contains(commandArg.name)) {
                            parsedArgsMap[commandArg.name] = arg;
                            break;
                        }
                    }
                }

                for (Argument subArg : subcommand.args) {
                    if (parsedArgsMap.contains(subArg.name)) {
                        if (subArg.type == "STRING") {
                            jsonObj[subArg.name] = parsedArgsMap[subArg.name];
                        } else if (subArg.type == "INT") {
                            jsonObj[subArg.name] = parsedArgsMap[subArg.name].toInt();
                        } else if (subArg.type == "BOOL") {
                            jsonObj[subArg.mark] = parsedArgsMap[subArg.mark] == "true";
                        } else if (subArg.type == "FILE") {
                            QFile file(parsedArgsMap[subArg.name]);
                            if (file.open(QIODevice::ReadOnly)) {
                                QByteArray fileData = file.readAll();
                                jsonObj[subArg.name] = QString::fromLatin1(fileData.toBase64());
                                file.close();
                            } else {
                                return QString("Failed to open file: %1").arg(parsedArgsMap[subArg.name]);
                            }
                        }
                    } else if (subArg.required) {
                        return "Missing required argument for subcommand: " + subArg.name;
                    }
                }
                break;
            }
        }
    }

    QJsonDocument jsonDoc(jsonObj);
    return jsonDoc.toJson();
}

bool Commander::IsValid()
{
    return valid;
}

QString Commander::GetError()
{
    return error;
}

QString Commander::help(QStringList commandParts)
{
    QString result;
    QTextStream output(&result);
    if (commandParts.isEmpty()) {
        int TotalWidth = 20;
        output << QString("  Command                   Description\n");
        output << QString("  -------                   -----------\n");

        for ( auto command : commands ) {
            QString commandName = command.name;
            if (!command.subcommands.isEmpty())
                commandName += '*';

            QString tab = QString(TotalWidth - commandName.size(), ' ');
            output << "  " + commandName + tab + "      " + command.description + "\n";
        }
        return result;
    }
    else {
        Command foundCommand;
        Command foundSubCommand;
        QString commandName = commandParts[0];

        for (Command cmd : commands) {
            if (cmd.name == commandName) {
                foundCommand = cmd;
                break;
            }
        }

        if ( foundCommand.name.isEmpty() )
            return "Unknown command";

        if (commandParts.size() == 1) {
            output << "  Command               : " + foundCommand.name + "\n";
            if(!foundCommand.description.isEmpty())
                output << "  Description           : " + foundCommand.description + "\n";
            if(!foundCommand.example.isEmpty())
                output << "  Example               : " + foundCommand.example + "\n";
            if( !foundCommand.subcommands.isEmpty() ) {
                int TotalWidth = 20;
                output << "\n";
                output << "  SubCommand                Description\n";
                output << "  ----------                -----------\n";
                for ( auto subcmd : foundCommand.subcommands ) {
                    int cmdWidth = subcmd.name.size();
                    if (cmdWidth > TotalWidth)
                        cmdWidth = TotalWidth;

                    QString tab = QString(TotalWidth - cmdWidth, ' ');
                    output << "  " + subcmd.name + tab + "      " + subcmd.description + "\n";
                }
            }
            else if ( !foundCommand.args.isEmpty() ) {
                QString argsHelp;
                QTextStream argsStream(&argsHelp);
                QString usageHelp;
                QTextStream usageStream(&usageHelp);

                usageStream << foundCommand.name;
                for ( auto arg : foundCommand.args ) {
                    QString fullarg = (arg.required ? "<" : "[") + arg.mark + ( arg.mark.isEmpty() || arg.name.isEmpty() ? "" : " " ) + arg.name + (arg.required ? ">" : "]");
                    usageStream << " " + fullarg;
                    argsStream << "    " + fullarg + "  : " + arg.type + ". " + arg.description + "\n";
                }
                output << "  Usage                 : " + usageHelp;
                output << "\n";
                output << "  Arguments:\n";
                output << argsHelp;
            }
        }
        else if (commandParts.size() == 2) {
            QString subCommandName = commandParts[0];
            for (Command subcmd : foundCommand.subcommands) {
                if (subcmd.name == subCommandName) {
                    foundSubCommand = subcmd;
                    break;
                }
            }

            if ( foundSubCommand.name.isEmpty() )
                return "Unknown subcommand";

            output << "  Command               : " + foundCommand.name + "\n";
            output << "  SubCommand            : " + foundSubCommand.name + "\n";
            if(!foundSubCommand.description.isEmpty())
                output << "  Description           : " + foundSubCommand.description + "\n";
            if(!foundSubCommand.example.isEmpty())
                output << "  Example               : " + foundSubCommand.example + "\n";
            if ( !foundSubCommand.args.isEmpty() ) {
                QString argsHelp;
                QTextStream argsStream(&argsHelp);
                QString usageHelp;
                QTextStream usageStream(&usageHelp);

                usageStream << foundCommand.name + " " + foundSubCommand.name;
                for ( auto arg : foundCommand.args ) {
                    QString fullarg = (arg.required ? "<" : "[") + arg.mark + ( arg.mark.isEmpty() || arg.name.isEmpty() ? "" : " " ) + arg.name + (arg.required ? ">" : "]");
                    usageStream << " " + fullarg;
                    argsStream << "    " + fullarg + "  : " + arg.type + ". " + arg.description + "\n";
                }
                output << "  Usage                 : " + usageHelp;
                output << "\n";
                output << "  Arguments:\n";
                output << argsHelp;
            }
        }
        else {
            return "Error Help format: 'help [command [subcommand]]";
        }
        return output.readAll();
    }
}

QStringList Commander::getCommandList()
{
    QStringList commandList;
    for (Command cmd : commands) {
        commandList << cmd.name;
        for (Command subcmd : cmd.subcommands) {
            commandList << cmd.name + " " + subcmd.name;
        }
    }
    QStringList copyCommandList = commandList;
    for( auto cmd : copyCommandList) {
        commandList << "help " + cmd;
    }

    return commandList;
}