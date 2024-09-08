#include <UI/Widgets/AdaptixWidget.h>

bool AdaptixWidget::isValidSyncPacket(QJsonObject jsonObj) {
    if ( !jsonObj.contains("type") || !jsonObj["type"].isDouble() || !jsonObj.contains("subtype") || !jsonObj["subtype"].isDouble() )
        return false;

    int spType    = jsonObj["type"].toDouble();
    int spSubType = jsonObj["subtype"].toDouble();

    if( spType == TYPE_SYNC ) {
        if( spSubType == TYPE_SYNC_START ) {
            if ( !jsonObj.contains("count") || !jsonObj["count"].isDouble() ) {
                return false;
            }
        }
        return true;
    }

    if ( !jsonObj.contains("time") || !jsonObj["time"].isDouble() )
        return false;

    if( spType == TYPE_CLIENT ) {
        if ( !jsonObj.contains("username") || !jsonObj["username"].isString() ) {
            return false;
        }
        return true;
    }

    return false;
}

void AdaptixWidget::processSyncPacket(QJsonObject jsonObj){
    int spType    = jsonObj["type"].toDouble();
    int spSubType = jsonObj["subtype"].toDouble();

    if( dialogSyncPacket != nullptr ) {
        dialogSyncPacket->receivedLogs++;
        dialogSyncPacket->upgrade();
    }

    switch (spType) {

        case TYPE_SYNC:
            if( spSubType == TYPE_SYNC_START)
            {
                int count = jsonObj["count"].toDouble();
                dialogSyncPacket = new DialogSyncPacket(count);
            }
            else if (spSubType == TYPE_SYNC_FINISH)
            {
                if (dialogSyncPacket != nullptr ) {
                    dialogSyncPacket->finish();
                    delete dialogSyncPacket;
                    dialogSyncPacket = nullptr;
                }
            }
            break;

        case TYPE_CLIENT:

            qint64 time = static_cast<qint64>(jsonObj["time"].toDouble());
            QString username = jsonObj["username"].toString();
            QString message;

            if( spSubType == TYPE_CLIENT_CONNECT )
                message = QString("Client '%1' connected to teamserver").arg(username);
            else if ( spSubType == TYPE_CLIENT_DISCONNECT )
                message = QString("Client '%1' disconnected from teamserver").arg(username);

            LogsTab->AddLogs(spSubType, time, message);
            break;
    }
}