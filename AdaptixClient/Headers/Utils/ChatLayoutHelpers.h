#ifndef ADAPTIXCLIENT_CHATLAYOUTHELPERS_H
#define ADAPTIXCLIENT_CHATLAYOUTHELPERS_H

#include <QDateTime>
#include <QString>
#include <QtGlobal>

namespace ChatLayoutHelpers {

inline constexpr int kBubbleMaxPct = 68;
inline constexpr int kBubbleAbsMax = 720;
inline constexpr int kConversationMax = 1080;
inline constexpr int kBubbleInnerPad = 16;
inline constexpr int kBubbleMinText = 48;
inline constexpr int kGroupGapSec = 8 * 60; // 8 minutes

inline int conversationWidth(int rowWidth)
{
    if (rowWidth < 80)
        return qMin(400, kConversationMax);
    return qMin(rowWidth, kConversationMax);
}

inline int conversationLeft(int rowLeft, int rowWidth)
{
    const int streamW = conversationWidth(rowWidth);
    if (streamW >= rowWidth)
        return rowLeft;
    return rowLeft + (rowWidth - streamW) / 2;
}

inline int bubbleTextWidth(int idealTextWidth, int streamWidth)
{
    if (streamWidth < 80)
        streamWidth = 400;
    const int maxW = qMin(streamWidth * kBubbleMaxPct / 100, kBubbleAbsMax) - kBubbleInnerPad;
    const int minW = kBubbleMinText;
    if (maxW <= minW)
        return minW;
    int w = idealTextWidth;
    if (w < minW)
        w = minW;
    if (w > maxW)
        w = maxW;
    return w;
}

inline int bubbleOuterWidth(int textWidth)
{
    return textWidth + kBubbleInnerPad;
}

inline int mineBubbleRightEdge(int streamLeft, int streamWidth, int hPad, int avatarSize)
{
    return streamLeft + streamWidth - hPad - (avatarSize + 8);
}

inline int otherBubbleLeftEdge(int streamLeft, int hPad, int avatarSize)
{
    return streamLeft + hPad + avatarSize + 8;
}

inline int bubbleLeftX(bool isMine, int rowLeft, int rowWidth, int hPad, int avatarSize, int bubbleOuterW)
{
    const int streamW = conversationWidth(rowWidth);
    const int streamLeft = conversationLeft(rowLeft, rowWidth);
    if (isMine) {
        int x = mineBubbleRightEdge(streamLeft, streamW, hPad, avatarSize) - bubbleOuterW;
        const int minX = streamLeft + hPad;
        if (x < minX)
            x = minX;
        return x;
    }
    return otherBubbleLeftEdge(streamLeft, hPad, avatarSize);
}

inline int avatarX(bool isMine, int rowLeft, int rowWidth, int hPad, int avatarSize)
{
    const int streamW = conversationWidth(rowWidth);
    const int streamLeft = conversationLeft(rowLeft, rowWidth);
    if (isMine)
        return streamLeft + streamW - hPad - avatarSize;
    return streamLeft + hPad;
}

inline bool isGroupStart(bool hasPrev, const QString& prevUser, qint64 prevDate, qint64 prevReplyToId, const QString& currUser, qint64 currDate, qint64 currReplyToId)
{
    if (!hasPrev)
        return true;
    if (QString::compare(prevUser, currUser, Qt::CaseSensitive) != 0)
        return true;
    if (currReplyToId > 0)
        return true;
    if (prevReplyToId > 0 && currReplyToId == 0) {
        return true;
    }
    if (currDate > 0 && prevDate > 0 && qAbs(currDate - prevDate) > kGroupGapSec)
        return true;
    return false;
}

inline bool showTimestamp(bool isLastInGroup)
{
    return isLastInGroup;
}

inline QString formatMessageTime(qint64 unixTs)
{
    if (unixTs <= 0)
        return {};
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(unixTs);
    const QDateTime now = QDateTime::currentDateTime();
    if (dt.date() == now.date())
        return dt.toString(QStringLiteral("HH:mm"));
    return dt.toString(QStringLiteral("dd.MM HH:mm"));
}

inline int idealPlainTextWidth(const QString& text, int avgCharWidth, int maxProbe = 80)
{
    if (text.isEmpty())
        return 28;
    int maxLen = 0;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& line : lines)
        maxLen = qMax(maxLen, line.size());
    if (avgCharWidth < 4)
        avgCharWidth = 7;
    const int chars = qMin(maxLen, maxProbe);
    return chars * avgCharWidth + 8;
}

inline int bubbleTimeBottomPad(int timeH, int vPad, bool showTime)
{
    if (!showTime)
        return vPad;
    return qMax(vPad, 3) + timeH;
}

inline int bubbleOuterWithTime(int bubbleOuterW, int textH, int lineH, int timeW, int hChrome = 16)
{
    if (timeW <= 0)
        return bubbleOuterW;
    if (textH <= lineH + 6) {
        const int need = bubbleOuterW + timeW + 10;
        return need;
    }
    return qMax(bubbleOuterW, timeW + hChrome + 8);
}

}

#endif
