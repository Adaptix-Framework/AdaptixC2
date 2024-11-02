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
        output << "Available commands:\n";
        for (Command cmd : commands) {
            output << "- " << cmd.name << ": " << cmd.description << "\n";
        }
        return output.readAll();
    }
    else {
        const Command* foundCommand = nullptr;
        const Command* foundSubcommand = nullptr;

        if (commandParts.size() == 2) {
            QString mainCommandName = commandParts[0];
            QString subCommandName = commandParts[1];

            for (const Command& cmd : commands) {
                if (cmd.name == mainCommandName) {
                    for (Command subcmd : cmd.subcommands) {
                        if (subcmd.name == subCommandName) {
                            foundCommand = &cmd;
                            foundSubcommand = &subcmd;
                            break;
                        }
                    }
                }
                if (foundSubcommand) break;
            }
        }
        else {
            QString mainCommandName = commandParts[0];
            for (Command cmd : commands) {
                if (cmd.name == mainCommandName) {
                    foundCommand = &cmd;
                    break;
                }
            }
        }

        if (!foundCommand) {
            return QString("Command '%1' not found.\n").arg(commandParts[0]);
        }

        if (foundSubcommand) {
            output << "Subcommand: " << foundSubcommand->name << "\n";
            output << foundSubcommand->description << "\n";
            output << "Usage: " << foundSubcommand->example << "\n";
            output << "Arguments:\n";
            for (Argument arg : foundSubcommand->args) {
                output << "  " << (arg.required ? "<" : "[") << arg.name << (arg.required ? ">" : "]");
                if (!arg.mark.isEmpty()) output << " (" << arg.mark << ")";
                output << " - Type: " << arg.type << "\n";
            }
        } else {
            output << "Command: " << foundCommand->name << "\n";
            output << foundCommand->description << "\n";
            output << "Usage: " << foundCommand->example << "\n";
            if (!foundCommand->args.isEmpty()) {
                output << "Arguments:\n";
                for (Argument arg : foundCommand->args) {
                    output << "  " << (arg.required ? "<" : "[") << arg.name << (arg.required ? ">" : "]");
                    if (!arg.mark.isEmpty()) output << " (" << arg.mark << ")";
                    output << " - Type: " << arg.type << "\n";
                }
            }
            if (!foundCommand->subcommands.isEmpty()) {
                output << "Subcommands:\n";
                for ( Command subcmd : foundCommand->subcommands) {
                    output << "  " << subcmd.name << ": " << subcmd.description << "\n";
                }
            }
        }
        return output.readAll();
    }
}

QStringList Commander::getCommandList() {
    QStringList commandList;
    for (const Command& cmd : commands) {
        commandList << cmd.name;
        for (const Command& subcmd : cmd.subcommands) {
            commandList << cmd.name + " " + subcmd.name;
        }
    }
    return commandList;
}