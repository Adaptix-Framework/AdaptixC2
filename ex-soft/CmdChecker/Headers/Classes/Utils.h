#ifndef AXCHECKER_UTILS_H
#define AXCHECKER_UTILS_H

#include <main.h>

typedef struct AgentData
{
//    QString     Id;
//    QString     Name;
//    QString     Listener;
//    bool        Async;
//    QString     ExternalIP;
//    QString     InternalIP;
//    int         GmtOffset;
//    int         Sleep;
//    int         Jitter;
//    QString     Pid;
//    QString     Tid;
    QString     Arch;
//    bool        Elevated;
//    QString     Process;
//    int         Os;
//    QString     OsDesc;
//    QString     Domain;
//    QString     Computer;
//    QString     Username;
//    QString     Tags;
//    int         LastTick;
} AgentData;

QString ReadFileString(const QString &filePath, bool* result);

QByteArray ReadFileBytearray(const QString &filePath, bool* result);

QString ValidCommandsFile(QByteArray jsonData, bool* result);

QString ValidCommand(QJsonObject extJsonObject, bool* result);

QString ValidExtCommand(QJsonObject extJsonObject, bool* result);

#endif //AXCHECKER_UTILS_H
