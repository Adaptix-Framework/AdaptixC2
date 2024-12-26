#include <Agent/Commander.h>

void BofPacker::Pack(QJsonValue jsonValue)
{
    if(jsonValue.isString()) {

        QString str = jsonValue.toString();

        QByteArray strLengthData;
        int strLength = str.length() + 1;
        strLengthData.append(reinterpret_cast<const char*>(&strLength), sizeof(strLength));

        data.append(strLengthData);
        data.append(str.toUtf8());
    }
    else if(jsonValue.isDouble()) {
        int num = jsonValue.toDouble();
        QByteArray numData;
        numData.append(reinterpret_cast<const char*>(&num), sizeof(num));

        data.append(numData);
    }
}

void BofPacker::Pack(QString str)
{
    QByteArray strLengthData;
    int strLength = str.size() + 1;
    strLengthData.append(reinterpret_cast<const char*>(&strLength), sizeof(strLength));

    data.append(strLengthData);
    data.append(str.toUtf8());
}

QString BofPacker::Build()
{
    QByteArray strLengthData;
    int strLength = data.size();
    strLengthData.append(reinterpret_cast<const char*>(&strLength), sizeof(strLength));

    strLengthData.append(data);
    return strLengthData.toBase64();
}



Commander::Commander(){}

Commander::~Commander() = default;

bool Commander::AddRegCommands(QByteArray jsonData)
{
    QList<Command> commandsList;

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(jsonData, &parseError);

    QJsonArray commandArray = document.array();
    for (QJsonValue value : commandArray) {

        QJsonObject jsonObject = value.toObject();
        Command cmd = this->ParseCommand(jsonObject);
        commandsList.append(cmd);
    }

    commands = commandsList;
    return true;
}

bool Commander::AddExtCommands(QString filepath, QString extName, QList<QJsonObject> extCommands)
{
    QFileInfo fi(filepath);
    QString dirPath = fi.absolutePath();

    QList<Command> commandsList;

    for (QJsonObject jsonObject : extCommands) {
        Command extCmd = this->ParseCommand(jsonObject);
        extCmd.extPath = dirPath;
        commandsList.append(extCmd);
    }

    extModules[filepath].extName = extName;
    extModules[filepath].extCommands = commandsList;

    return true;
}

void Commander::RemoveExtCommands(QString filepath)
{
    extModules.remove(filepath);
}


Command Commander::ParseCommand(QJsonObject jsonObject)
{
    Command cmd;
    cmd.name        = jsonObject["command"].toString();
    cmd.message     = jsonObject["message"].toString();
    cmd.description = jsonObject["description"].toString();
    cmd.example     = jsonObject["example"].toString();
    cmd.exec        = jsonObject["exec"].toString();

    if (jsonObject.contains("subcommands")) {
        QJsonArray subcommandArray = jsonObject["subcommands"].toArray();
        for (QJsonValue subCmdVal : subcommandArray) {
            QJsonObject subCmdObj = subCmdVal.toObject();

            Command subCmd;
            subCmd.name        = subCmdObj["name"].toString();
            subCmd.message     = subCmdObj["message"].toString();
            subCmd.description = subCmdObj["description"].toString();
            subCmd.example     = subCmdObj["example"].toString();
            subCmd.exec        = subCmdObj["exec"].toString();

            QJsonArray subArgsArray = subCmdObj["args"].toArray();
            for (QJsonValue subArgVal : subArgsArray) {
                Argument subArg = ParseArgument(subArgVal.toString());
                if (subArg.valid)
                    subCmd.args.append(subArg);
            }
            cmd.subcommands.append(subCmd);
        }
    } else if (jsonObject.contains("args")) {

        QJsonArray argsArray = jsonObject["args"].toArray();
        for (QJsonValue argVal : argsArray) {
            Argument arg = ParseArgument(argVal.toString());
            if (arg.valid)
                cmd.args.append(arg);
        }
    }
    return cmd;
}


Argument Commander::ParseArgument(QString argString)
{
    Argument arg = {0};
    QRegularExpression regex(R"((\w+)\s+([\[\<][^\s\]]+[\s\w-]*[\>\]])(\s*\([^\)]*\))?(?:\s+\{(.+)\})?)");
    QRegularExpressionMatch match = regex.match(argString);

    if ( !match.hasMatch()) {
        error = "arguments not parsed";
        arg.valid = false;
        return arg;
    }

    arg.type             = match.captured(1);
    QString flagAndValue = match.captured(2).trimmed();
    QString defaultValue = match.captured(3).trimmed();
    arg.description      = match.captured(4).trimmed();

    if( !defaultValue.isEmpty() ) {
        arg.defaultValue = defaultValue.mid(1, defaultValue.size() - 2).trimmed();
    }

    if (flagAndValue.startsWith("<") && flagAndValue.endsWith(">")) {
        arg.required = true;
    }
    else if (flagAndValue.startsWith("[") && flagAndValue.endsWith("]")) {
        arg.required = false;
    }
    else {
        error = "argument must be in <> or []";
        arg.valid = false;
        return arg;
    }

    int spaceIndex = flagAndValue.indexOf(' ');
    if (spaceIndex != -1) {
        arg.mark = flagAndValue.mid(1, spaceIndex - 1).trimmed();
        arg.name = flagAndValue.mid(spaceIndex + 1, flagAndValue.size() - spaceIndex - 2).trimmed();
        arg.flag = true;
    }
    else {
        QString value = flagAndValue.mid(1, flagAndValue.size() - 2).trimmed();
        if( value.startsWith("-") || value.startsWith("/") ) {
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

CommanderResult Commander::ProcessInput(AgentData agentData, QString input)
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
        return CommanderResult{true, "", false};

    QString commandName = parts[0];
    parts.removeAt(0);

    if( commandName == "help") {
        return this->ProcessHelp(parts);
    }

    for (Command command : commands) {
        if (command.name == commandName) {
            return ProcessCommand(agentData, command, parts);
        }
    }

    for ( auto extMod : extModules ) {
        for (Command command : extMod.extCommands) {
            if (command.name == commandName) {
                return ProcessCommand(agentData, command, parts);
            }
        }
    }

    return CommanderResult{true, "Command not found", true};
}

CommanderResult Commander::ProcessCommand(AgentData agentData, Command command, QStringList args)
{
    QString execStr = "";
    QList<Argument> execArgs;

    QJsonObject jsonObj;
    jsonObj["command"] = command.name;

    if ( command.subcommands.size() == 0 ) {

        QMap<QString, QString> parsedArgsMap;

        QString wideKey;
        for (int i = 0; i < args.size(); ++i) {
            QString arg = args[i];

            bool isWideArgs = true;

            for (Argument commandArg : command.args) {
                if (commandArg.flag) {
                    if (commandArg.type == "BOOL" && commandArg.mark == arg ) {
                        parsedArgsMap[commandArg.mark] = "true";
                        wideKey = commandArg.mark;
                        isWideArgs = false;
                        break;
                    }
                    else if ( commandArg.mark == arg && args.size() > i+1 ) {
                        ++i;
                        parsedArgsMap[commandArg.name] = args[i];
                        wideKey = commandArg.name;
                        isWideArgs = false;
                        break;
                    }
                }
                else if (!parsedArgsMap.contains(commandArg.name)) {
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
                } else if (commandArg.type == "INT") {
                    jsonObj[commandArg.name] = parsedArgsMap[commandArg.name].toInt();
                } else if (commandArg.type == "BOOL") {
                    jsonObj[commandArg.mark] = parsedArgsMap[commandArg.mark] == "true";
                } else if (commandArg.type == "FILE") {
                    QString path = parsedArgsMap[commandArg.name];
                    if (path.startsWith("~/"))
                        path = QDir::home().filePath(path.mid(2));

                    QFile file(path);
                    if (file.open(QIODevice::ReadOnly)) {
                        QByteArray fileData = file.readAll();
                        jsonObj[commandArg.name] = QString::fromLatin1(fileData.toBase64());
                        file.close();
                    } else {
                        return CommanderResult{true, "Failed to open file: " + path, true };
                    }
                }
            } else if (commandArg.required) {
                if (commandArg.defaultValue.isEmpty()) {
                    return CommanderResult{true, "Missing required argument: " + commandArg.name, true };
                } else {
                    if (commandArg.type == "STRING") {
                        jsonObj[commandArg.name] = commandArg.defaultValue;
                    } else if (commandArg.type == "INT") {
                        jsonObj[commandArg.name] = commandArg.defaultValue.toInt();
                    } else if (commandArg.type == "BOOL") {
                        jsonObj[commandArg.mark] = commandArg.defaultValue == "true";
                    } else if (commandArg.type == "FILE") {
                        QString path = commandArg.defaultValue;
                        if (path.startsWith("~/"))
                            path = QDir::home().filePath(path.mid(2));

                        QFile file(path);
                        if (file.open(QIODevice::ReadOnly)) {
                            QByteArray fileData = file.readAll();
                            jsonObj[commandArg.name] = QString::fromLatin1(fileData.toBase64());
                            file.close();
                        } else {
                            return CommanderResult{true, "Failed to open file: " + path, true };
                        }
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

        execStr = command.exec;
        execArgs = command.args;
    }
    else {
        if ( args.isEmpty() )
            return CommanderResult{true, "Subcommand must be set", true };

        QString subCommandName = args[0];

        for (Command subcommand : command.subcommands) {
            if (subCommandName == subcommand.name) {
                jsonObj["subcommand"] = subcommand.name;

                QMap<QString, QString> parsedArgsMap;

                QString wideKey;
                for (int i = 1; i < args.size(); ++i) {
                    QString arg = args[i];

                    bool isWideArgs = true;

                    for (Argument commandArg : subcommand.args) {
                        if (commandArg.flag && commandArg.mark == arg && args.size() > i+1 ) {
                            if( commandArg.type == "BOOL" ) {
                                parsedArgsMap[commandArg.mark] = "true";
                                wideKey = commandArg.mark;
                                isWideArgs = false;
                            }
                            else {
                                ++i;
                                parsedArgsMap[commandArg.name] = args[i];
                                wideKey = commandArg.name;
                                isWideArgs = false;
                            }
                            break;
                        } else if (!commandArg.flag && !parsedArgsMap.contains(commandArg.name)) {
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

                for (Argument subArg : subcommand.args) {
                    if (parsedArgsMap.contains(subArg.name) || parsedArgsMap.contains(subArg.mark)) {
                        if (subArg.type == "STRING") {
                            jsonObj[subArg.name] = parsedArgsMap[subArg.name];
                        } else if (subArg.type == "INT") {
                            jsonObj[subArg.name] = parsedArgsMap[subArg.name].toInt();
                        } else if (subArg.type == "BOOL") {
                            jsonObj[subArg.mark] = parsedArgsMap[subArg.mark] == "true";
                        } else if (subArg.type == "FILE") {
                            QString path = parsedArgsMap[subArg.name];
                            if (path.startsWith("~/"))
                                path = QDir::home().filePath(path.mid(2));

                            QFile file(path);
                            if (file.open(QIODevice::ReadOnly)) {
                                QByteArray fileData = file.readAll();
                                jsonObj[subArg.name] = QString::fromLatin1(fileData.toBase64());
                                file.close();
                            } else {
                                return CommanderResult{true, "Failed to open file: " + path, true };
                            }
                        }
                    } else if (subArg.required) {
                        if (subArg.defaultValue.isEmpty()) {
                            return CommanderResult{true, "Missing required argument for subcommand: " + subArg.name, true };
                        } else {
                            if (subArg.type == "STRING") {
                                jsonObj[subArg.name] = subArg.defaultValue;
                            } else if (subArg.type == "INT") {
                                jsonObj[subArg.name] = subArg.defaultValue.toInt();
                            } else if (subArg.type == "BOOL") {
                                jsonObj[subArg.mark] = subArg.defaultValue == "true";
                            } else if (subArg.type == "FILE") {
                                QString path = subArg.defaultValue;
                                if (path.startsWith("~/"))
                                    path = QDir::home().filePath(path.mid(2));

                                QFile file(path);
                                if (file.open(QIODevice::ReadOnly)) {
                                    QByteArray fileData = file.readAll();
                                    jsonObj[subArg.name] = QString::fromLatin1(fileData.toBase64());
                                    file.close();
                                } else {
                                    return CommanderResult{true, "Failed to open file: " + path, true };
                                }
                            }
                        }
                    }
                }

                QString msg = subcommand.message;
                if( !msg.isEmpty() ) {
                    for ( QString k : parsedArgsMap.keys() ) {
                        QString param = "<" + k + ">";
                        if( msg.contains(param) )
                            msg = msg.replace(param, parsedArgsMap[k]);
                    }
                    jsonObj["message"] = msg;
                }

                execStr  = subcommand.exec;
                execArgs = subcommand.args;

                break;
            }
        }
    }

    if( !execStr.isEmpty() ) {
        QString newInput = this->ProcessExecExtension( agentData, command.extPath, execStr, execArgs, jsonObj);
        CommanderResult execCommandResult = this->ProcessInput(agentData, newInput);
        if( !execCommandResult.error ) {
            QJsonParseError parseError;
            QJsonDocument document = QJsonDocument::fromJson(execCommandResult.message.toUtf8(), &parseError);
            QJsonObject jsonObject = document.object();
            jsonObject["message"] = jsonObj["message"];
            QJsonDocument jsonDoc(jsonObject);
            execCommandResult.message = jsonDoc.toJson();
        }
        return execCommandResult;
    }

    QJsonDocument jsonDoc(jsonObj);
    return CommanderResult{false, jsonDoc.toJson(), false };
}

QString Commander::ProcessExecExtension(AgentData agentData, QString filepath, QString ExecString, QList<Argument> args, QJsonObject jsonObj)
{
    // ARCH

    ExecString = ExecString.replace("$ARCH()", agentData.Arch, Qt::CaseSensitive);

    // $EXT_DIR

    ExecString = ExecString.replace("$EXT_DIR()", filepath, Qt::CaseSensitive);

    // BOF_PACK

    int offset = 0;
    QRegularExpression packRegex(R"(\$PACK_BOF\s*\(([^)]*)\))");
    QRegularExpressionMatchIterator iter = packRegex.globalMatch(ExecString);

    while (iter.hasNext()) {
        QRegularExpressionMatch match = iter.next();
        QString packContent = match.captured(1); // Содержимое внутри скобок $PACK(...)

        QRegularExpression paramRegex(R"(\{\s*([^}]*)\s*\}|([^,\s][^,]*[^,\s]))");
        QRegularExpressionMatchIterator it = paramRegex.globalMatch(packContent);

        BofPacker packer;
        while (it.hasNext()) {
            QRegularExpressionMatch paramMatch = it.next();
            if (!paramMatch.captured(1).isEmpty()) {
                QString str = paramMatch.captured(1);
                if( jsonObj.contains(str) )
                    packer.Pack(jsonObj[str]);
            } else if (!paramMatch.captured(2).isEmpty()) {
                QString str = paramMatch.captured(2);
                packer.Pack(str);
            }
        }
        QString bofParam = packer.Build();

        ExecString.replace(match.capturedStart() + offset, match.capturedLength(), bofParam);
        offset += bofParam.length() - match.capturedLength();
    }

    // Arguments

    offset = 0;
    QRegularExpression remainingArgsRegex(R"(\{\s*([^}]*)\s*\})");
    QRegularExpressionMatchIterator remainingIt = remainingArgsRegex.globalMatch(ExecString);

    while (remainingIt.hasNext()) {
        QRegularExpressionMatch remainingMatch = remainingIt.next();
        QString paramName = remainingMatch.captured(1).trimmed();
        if( jsonObj.contains(paramName) && jsonObj[paramName].isString() ){
            QString paramValue = jsonObj[paramName].toString();
            ExecString.replace(remainingMatch.capturedStart() + offset, remainingMatch.capturedLength(), paramValue);
            offset += paramValue.length() - remainingMatch.capturedLength();
        }
    }

    return ExecString;
}


QString Commander::GetError()
{
    return error;
}

CommanderResult Commander::ProcessHelp(QStringList commandParts)
{
    QString result;
    QTextStream output(&result);
    if (commandParts.isEmpty()) {
        int TotalWidth = 20;
        output << QString("\n");
        output << QString("  Command                   Description\n");
        output << QString("  -------                   -----------\n");

        for ( auto command : commands ) {
            QString commandName = command.name;
            if (!command.subcommands.isEmpty())
                commandName += '*';

            QString tab = QString(TotalWidth - commandName.size(), ' ');
            output << "  " + commandName + tab + "      " + command.description + "\n";
        }

        for ( auto extMod : extModules.values() ){
            output << QString("\n");
            output << QString("  Extension - " + extMod.extName + "\n");
            output << QString("  =====================================\n");

            for ( auto command : extMod.extCommands ) {
                QString commandName = command.name;
                if (!command.subcommands.isEmpty())
                    commandName += '*';

                QString tab = QString(TotalWidth - commandName.size(), ' ');
                output << "  " + commandName + tab + "      " + command.description + "\n";
            }
        }

        return CommanderResult{true, result, false};
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

        for( auto extMod : extModules.values()) {
            if ( !foundCommand.name.isEmpty() )
                break;

            for (Command cmd : extMod.extCommands) {
                if (cmd.name == commandName) {
                    foundCommand = cmd;
                    break;
                }
            }
        }

        if ( foundCommand.name.isEmpty() )
            return CommanderResult{true, "Unknown command: " + commandName, true};

        if (commandParts.size() == 1) {
            output << QString("\n");
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
                    argsStream << "    " + fullarg + "  : " + arg.type + ( arg.defaultValue.isEmpty() ? ". " : " (default: '" + arg.defaultValue + "'). " ) + arg.description + "\n";
                }
                output << "  Usage                 : " + usageHelp;
                output << "\n";
                output << "  Arguments:\n";
                output << argsHelp;
            }
        }
        else if (commandParts.size() == 2) {
            QString subCommandName = commandParts[1];
            for (Command subcmd : foundCommand.subcommands) {
                if (subcmd.name == subCommandName) {
                    foundSubCommand = subcmd;
                    break;
                }
            }

            if ( foundSubCommand.name.isEmpty() )
                return CommanderResult{true, "Unknown subcommand: " + subCommandName, true};

            output << "  Command               : " + foundCommand.name + " " + foundSubCommand.name +"\n";
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
                for ( auto arg : foundSubCommand.args ) {
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
            return CommanderResult{true, "Error Help format: 'help [command [subcommand]]'", true};
        }
        return CommanderResult{true, output.readAll(), false};
    }
}

QStringList Commander::GetCommands()
{
    QStringList commandList;
    for (Command cmd : commands) {
        commandList << cmd.name;
        for (Command subcmd : cmd.subcommands) {
            commandList << cmd.name + " " + subcmd.name;
        }
    }

    for( auto extMod : extModules.values()) {
        for (Command cmd : extMod.extCommands) {
            commandList << cmd.name;
            for (Command subcmd : cmd.subcommands) {
                commandList << cmd.name + " " + subcmd.name;
            }
        }
    }

    QStringList copyCommandList = commandList;
    for( QString cmd : copyCommandList)
        commandList << "help " + cmd;

    return commandList;
}
