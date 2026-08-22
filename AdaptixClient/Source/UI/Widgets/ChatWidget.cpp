#include <UI/Widgets/ChatWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Utils/Convert.h>
#include <Utils/FontManager.h>
#include <Utils/ChatLayoutHelpers.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <oclero/qlementine/style/QlementineStyle.hpp>

#include <QScrollBar>
#include <QMenu>
#include <QTextDocument>
#include <QPainter>
#include <QMouseEvent>
#include <QShowEvent>
#include <QResizeEvent>
#include <QShortcut>
#include <QToolTip>
#include <QDialog>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QRegularExpression>
#include <QMessageBox>
#include <QToolButton>
#include <QAbstractTextDocumentLayout>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextLine>
#include <QSizePolicy>
#include <QSplitterHandle>
#include <QSpinBox>
#include <QFrame>
#include <cmath>

REGISTER_DOCK_WIDGET(ChatWidget, "Chat", true)

static const QStringList EMOJI_LIST = {"👍", "👌", "👎", "😂", "❤️", "👀", "🎉", "✅", "❌", "💩"};

static QFont chatFont(int delta = 0, QFont::Weight weight = QFont::Normal)
{
    const AppTypography& ty = FontManager::instance().typography();
    QFont f = ty.regular;
    f.setPointSize(qMax(6, ty.baseSize + delta));
    f.setWeight(weight);
    f.setStyleHint(QFont::SansSerif);
    f.setFixedPitch(false);
    return f;
}

struct ChatLayoutMetrics {
    int avatarSize;
    int replyH;
    int nameH;
    int reactionH;
    int timeH;
    int vPad;
    int hPad;
    int inputH;
};

static ChatLayoutMetrics chatLayout()
{
    const AppTypography& ty = FontManager::instance().typography();
    ChatLayoutMetrics m;
    m.avatarSize = ty.chatAvatarSize;
    m.replyH     = ty.chatReplyH;
    m.nameH      = ty.chatNameH;
    m.reactionH  = ty.chatReactionH;
    m.timeH      = ty.chatTimeH;
    m.vPad       = qMax(4, qRound(6 * (ty.baseSize / 10.0)));
    m.hPad       = qMax(6, qRound(8 * (ty.baseSize / 10.0)));
    m.inputH     = ty.chatInputH;
    return m;
}


static QList<ChatReaction> parseReactions(const QString& json) {
    QList<ChatReaction> result;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return result;
    QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        ChatReaction r;
        r.emoji = it.key();
        for (const auto& v : it.value().toArray())
            r.users.append(v.toString());
        result.append(r);
    }
    return result;
}

static QString reactionsToJson(const QList<ChatReaction>& reactions) {
    QJsonObject obj;
    for (const auto& r : reactions) {
        QJsonArray arr;
        for (const auto& u : r.users) arr.append(u);
        obj[r.emoji] = arr;
    }
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

static QString formatTime(qint64 unixTs) {
    return ChatLayoutHelpers::formatMessageTime(unixTs);
}

static QString formatDate(qint64 unixTs) {
    QDateTime dt = QDateTime::fromSecsSinceEpoch(unixTs);
    return dt.toString("dd.MM.yyyy HH:mm");
}

static bool chatIsGroupStart(const QModelIndex& index)
{
    if (!index.isValid() || index.row() <= 0)
        return true;

    const QModelIndex prev = index.sibling(index.row() - 1, 0);
    return ChatLayoutHelpers::isGroupStart(
        true,
        prev.data(ChatMessageModel::UsernameRole).toString(),
        prev.data(ChatMessageModel::DateRole).toLongLong(),
        prev.data(ChatMessageModel::ReplyToIdRole).toLongLong(),
        index.data(ChatMessageModel::UsernameRole).toString(),
        index.data(ChatMessageModel::DateRole).toLongLong(),
        index.data(ChatMessageModel::ReplyToIdRole).toLongLong());
}

static bool chatIsLastInGroup(const QModelIndex& index)
{
    if (!index.isValid())
        return true;

    const QAbstractItemModel* model = index.model();
    if (!model || index.row() >= model->rowCount() - 1)
        return true;

    const QModelIndex next = index.sibling(index.row() + 1, 0);
    return ChatLayoutHelpers::isGroupStart(
        true,
        index.data(ChatMessageModel::UsernameRole).toString(),
        index.data(ChatMessageModel::DateRole).toLongLong(),
        index.data(ChatMessageModel::ReplyToIdRole).toLongLong(),
        next.data(ChatMessageModel::UsernameRole).toString(),
        next.data(ChatMessageModel::DateRole).toLongLong(),
        next.data(ChatMessageModel::ReplyToIdRole).toLongLong());
}

static bool chatNeedsDaySeparator(const QModelIndex& index)
{
    if (!index.isValid() || index.row() <= 0)
        return true;

    const QModelIndex prev = index.sibling(index.row() - 1, 0);
    const qint64 a = prev.data(ChatMessageModel::DateRole).toLongLong();
    const qint64 b = index.data(ChatMessageModel::DateRole).toLongLong();
    if (a <= 0 || b <= 0)
        return false;

    return QDateTime::fromSecsSinceEpoch(a).date() != QDateTime::fromSecsSinceEpoch(b).date();
}

static int measureIdealTextWidth(const QString& text, bool deleted, const QFont& font)
{
    if (deleted)
        return QFontMetrics(font).horizontalAdvance("This message has been deleted") + 8;

    QTextDocument doc;
    doc.setDocumentMargin(0);
    doc.setDefaultFont(font);
    QString fixed = text;
    fixed.replace(QRegularExpression("(?<!\n)\n(?!\n)"), "  \n");
    doc.setMarkdown(fixed);
    doc.setTextWidth(-1);
    const int ideal = static_cast<int>(std::ceil(doc.idealWidth()));
    return qMax(ChatLayoutHelpers::kBubbleMinText, ideal);
}

static int measureBubbleIdeal(const QModelIndex& index, bool deleted, const QFont& bodyFont)
{
    const QString msg = index.data(ChatMessageModel::MessageRole).toString();
    int ideal = measureIdealTextWidth(msg, deleted, bodyFont);
    const qint64 replyId = index.data(ChatMessageModel::ReplyToIdRole).toLongLong();
    if (replyId > 0) {
        QFontMetrics fm(chatFont(-2));
        const QString rName = index.data(ChatMessageModel::ReplyToNameRole).toString();
        const QString rText = index.data(ChatMessageModel::ReplyToTextRole).toString();
        ideal = qMax(ideal, fm.horizontalAdvance("↩ " + rName) + 16);
        if (!rText.isEmpty())
            ideal = qMax(ideal, fm.horizontalAdvance(rText) + 16);
        ideal = qMax(ideal, 120);
    }
    return ideal;
}

ChatMessageModel::ChatMessageModel(QObject* parent) : QAbstractListModel(parent) {}

int ChatMessageModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return messages.size();
}

QVariant ChatMessageModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= messages.size())
        return {};

    const ChatMessage& msg = messages.at(index.row());

    switch (role) {
        case IdRole:         return msg.id;
        case UsernameRole:   return msg.username;
        case MessageRole:    return msg.message;
        case DateRole:       return msg.date;
        case EditedRole:     return msg.edited;
        case DeletedRole:    return msg.deleted;
        case DeletedDateRole: return msg.deletedDate;
        case ReactionsRole:  return QVariant::fromValue(msg.reactions);
        case IsMineRole:     return msg.username == currentUser;
        case ReplyToIdRole:  return msg.replyToId;
        case ReplyToNameRole: return msg.replyToName;
        case ReplyToTextRole: return msg.replyToText;
        case ReplyToDeletedRole: return msg.replyToDeleted;
        case FlashRole: return msg.id == flashId;
        case IsMarkdownRole: return msg.isMarkdown;
        default:             return {};
    }
}

void ChatMessageModel::addMessage(const ChatMessage& msg) {
    beginInsertRows(QModelIndex(), messages.size(), messages.size());
    messages.append(msg);
    endInsertRows();
}

void ChatMessageModel::prependMessages(const QList<ChatMessage>& msgs) {
    if (msgs.isEmpty()) return;
    beginInsertRows(QModelIndex(), 0, msgs.size() - 1);
    for (int i = msgs.size() - 1; i >= 0; --i)
        messages.prepend(msgs[i]);
    endInsertRows();
}

void ChatMessageModel::editMessage(qint64 id, const QString& newText) {
    for (int i = 0; i < messages.size(); ++i) {
        if (messages[i].id == id) {
            messages[i].message = newText;
            messages[i].edited = true;
            QModelIndex idx = index(i);
            Q_EMIT dataChanged(idx, idx);
            return;
        }
    }
}

void ChatMessageModel::deleteMessage(qint64 id) {
    for (int i = 0; i < messages.size(); ++i) {
        if (messages[i].id == id) {
            messages[i].deleted = true;
            messages[i].deletedDate = QDateTime::currentSecsSinceEpoch();
            QModelIndex idx = index(i);
            Q_EMIT dataChanged(idx, idx);
            break;
        }
    }
    refreshReplyDeleted();
}

void ChatMessageModel::refreshReplyDeleted() {
    QSet<qint64> deletedIds;
    for (const auto& m : messages) {
        if (m.deleted) deletedIds.insert(m.id);
    }
    for (int i = 0; i < messages.size(); ++i) {
        if (messages[i].replyToId > 0) {
            bool nowDeleted = deletedIds.contains(messages[i].replyToId);
            if (messages[i].replyToDeleted != nowDeleted) {
                messages[i].replyToDeleted = nowDeleted;
                QModelIndex idx = index(i);
                Q_EMIT dataChanged(idx, idx);
            }
        }
    }
}

void ChatMessageModel::resolveReplyTexts() {
    QMap<qint64, QString> msgTexts;
    for (const auto& m : messages) {
        if (!m.deleted) {
            QString t = m.message;
            t.replace('\n', ' ');
            if (t.length() > 60) t = t.left(60) + "...";
            msgTexts[m.id] = t;
        }
    }
    for (int i = 0; i < messages.size(); ++i) {
        if (messages[i].replyToId > 0 && messages[i].replyToText.isEmpty()) {
            messages[i].replyToText = msgTexts.value(messages[i].replyToId, "");
        }
    }
}

void ChatMessageModel::setReactions(qint64 id, const QString& json) {
    for (int i = 0; i < messages.size(); ++i) {
        if (messages[i].id == id) {
            messages[i].reactions = parseReactions(json);
            QModelIndex idx = index(i);
            Q_EMIT dataChanged(idx, idx);
            return;
        }
    }
}

void ChatWidget::UpdateReactions(qint64 id, const QString& json) {
    messageModel->setReactions(id, json);
}

void ChatMessageModel::clear() {
    beginResetModel();
    messages.clear();
    endResetModel();
}

qint64 ChatMessageModel::oldestId() const {
    if (messages.isEmpty()) return 0;
    return messages.first().id;
}

ChatMessage ChatMessageModel::getMessageById(qint64 id) const {
    for (const auto& m : messages) {
        if (m.id == id) return m;
    }
    return {};
}

ChatMessageDelegate::ChatMessageDelegate(const QString& user, QObject* parent)
    : QStyledItemDelegate(parent), currentUser(user), docCache(500) {}

void ChatMessageDelegate::invalidateCache(qint64 id) {
    docCache.remove(id);
}

void ChatMessageDelegate::clearCache() {
    docCache.clear();
}

QTextDocument* ChatMessageDelegate::getDocument(const QModelIndex& index, int bubbleWidth) const {
    QString text = index.data(ChatMessageModel::MessageRole).toString();
    bool deleted = index.data(ChatMessageModel::DeletedRole).toBool();

    auto* doc = new QTextDocument();
    doc->setDocumentMargin(0);
    doc->setDefaultFont(chatFont(0));
    if (deleted) {
        qint64 delDate = index.data(ChatMessageModel::DeletedDateRole).toLongLong();
        QString delTime;
        if (delDate > 0) {
            delTime = QDateTime::fromSecsSinceEpoch(delDate).toString("dd.MM.yyyy HH:mm");
        } else {
            qint64 msgDate = index.data(ChatMessageModel::DateRole).toLongLong();
            delTime = msgDate > 0 ? QDateTime::fromSecsSinceEpoch(msgDate).toString("dd.MM.yyyy HH:mm") : "";
        }
        doc->setPlainText(delTime.isEmpty() ? "This message has been deleted" : "This message has been deleted (" + delTime + ")");
    } else {
        QString fixed = text;
        fixed.replace(QRegularExpression("(?<!\n)\n(?!\n)"), "  \n");
        doc->setMarkdown(fixed);
    }
    doc->setTextWidth(bubbleWidth);
    return doc;
}

static qreal realDocHeight(QTextDocument* doc) {
    return doc->size().height();
}

struct ChatColors {
    QColor bubbleMine;
    QColor bubbleOther;
    QColor avatarMine;
    QColor avatarOther;
    QColor text;
    QColor textSecondary;
    QColor textDisabled;
    QColor nameColor;
    QColor replyBg;
    QColor replyText;
    QColor replyDeletedBg;
    QColor replyDeletedText;
    QColor reactionBg;
    QColor reactionBgActive;
    QColor reactionBorder;
    QColor selectionBg;
    QColor flashBorder;

    static ChatColors fromTheme() {
        auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
        const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();

        bool isDark = t.backgroundColorMain1.lightnessF() < 0.5;
        int bubbleShift = isDark ? 125 : 85;

        ChatColors c;
        c.text            = t.secondaryColor;
        c.textSecondary   = t.secondaryAlternativeColor;
        c.textDisabled    = t.secondaryColorDisabled;
        c.nameColor       = t.primaryColor;

        c.bubbleMine      = t.backgroundColorMain1.lighter(bubbleShift + 15);
        c.bubbleOther     = t.backgroundColorMain1.lighter(bubbleShift);

        c.avatarMine      = t.statusColorSuccess;
        c.avatarOther     = t.primaryColor;
        c.replyBg         = t.backgroundColorMain1.lighter(bubbleShift - 10);
        c.replyText       = t.primaryColor;
        c.replyDeletedBg  = t.statusColorError.lighter(isDark ? 160 : 130);
        c.replyDeletedText = t.statusColorError;
        c.reactionBg      = t.secondaryColor.darker(isDark ? 140 : 115);
        c.reactionBgActive = t.primaryColor.darker(isDark ? 130 : 110);
        c.reactionBorder  = t.borderColor.lighter(isDark ? 130 : 110);
        c.selectionBg     = t.primaryColor;
        c.selectionBg.setAlpha(isDark ? 48 : 40);
        c.flashBorder     = t.statusColorWarning;
        return c;
    }
};

static const ChatColors& chatColors() {
    static ChatColors cached;
    static oclero::qlementine::QlementineStyle* lastStyle = nullptr;
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    if (qs && qs != lastStyle) {
        cached = ChatColors::fromTheme();
        lastStyle = qs;
    }
    return cached;
}

void ChatMessageDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const bool isMine = index.data(ChatMessageModel::IsMineRole).toBool();
    const bool deleted = index.data(ChatMessageModel::DeletedRole).toBool();
    const bool edited = index.data(ChatMessageModel::EditedRole).toBool();
    const QString username = index.data(ChatMessageModel::UsernameRole).toString();
    const qint64 date = index.data(ChatMessageModel::DateRole).toLongLong();
    const QList<ChatReaction> reactions = index.data(ChatMessageModel::ReactionsRole).value<QList<ChatReaction>>();
    const bool groupStart = chatIsGroupStart(index);
    const bool daySep = chatNeedsDaySeparator(index) && groupStart && index.row() > 0;

    QRect r = option.rect;
    const ChatLayoutMetrics lm = chatLayout();
    const int avatarSize = lm.avatarSize;
    const int hPad = lm.hPad;
    const int vPad = lm.vPad;
    const int viewWidth = r.width();
    const int streamW = ChatLayoutHelpers::conversationWidth(viewWidth);

    const auto& cc = chatColors();
    const QColor bubbleColor = isMine ? cc.bubbleMine : cc.bubbleOther;
    const QColor timeColor = cc.textSecondary;
    const QColor avatarBg = isMine ? cc.avatarMine : cc.avatarOther;

    const int ideal = measureBubbleIdeal(index, deleted, chatFont(0));
    const int textW = ChatLayoutHelpers::bubbleTextWidth(ideal, streamW);
    int bubbleOuterW = ChatLayoutHelpers::bubbleOuterWidth(textW);

    int replyH = 0;
    const qint64 replyToId = index.data(ChatMessageModel::ReplyToIdRole).toLongLong();
    const QString replyToName = index.data(ChatMessageModel::ReplyToNameRole).toString();
    const QString replyToText = index.data(ChatMessageModel::ReplyToTextRole).toString();
    const bool replyToDeleted = index.data(ChatMessageModel::ReplyToDeletedRole).toBool();
    if (replyToId > 0)
        replyH = lm.replyH;

    QTextDocument* doc = getDocument(index, textW);
    const int textH = static_cast<int>(realDocHeight(doc));

    QString timeStr = formatTime(date);
    if (edited)
        timeStr += " ✏";
    {
        QFontMetrics tfm(chatFont(-2));
        const int timeW = tfm.horizontalAdvance(timeStr) + 2;
        bubbleOuterW = ChatLayoutHelpers::bubbleOuterWithTime(bubbleOuterW, textH, QFontMetrics(chatFont(0)).lineSpacing(), timeW);
        const int maxOuter = streamW - 2 * hPad - (avatarSize + 8);
        if (bubbleOuterW > maxOuter && maxOuter > 40)
            bubbleOuterW = maxOuter;
    }
    const int bottomPad = ChatLayoutHelpers::bubbleTimeBottomPad(lm.timeH, vPad, true);
    const int bubbleH = textH + replyH + vPad + bottomPad;

    int y = r.top() + (groupStart ? 6 : 2);
    int dayH = 0;
    const int streamLeft = ChatLayoutHelpers::conversationLeft(r.left(), viewWidth);
    if (daySep) {
        dayH = lm.timeH + 10;
        QFont dayFont = chatFont(-2);
        painter->setFont(dayFont);
        painter->setPen(timeColor);
        const QString dayLabel = QDateTime::fromSecsSinceEpoch(date).toString("dd MMM yyyy");
        const int midY = y + lm.timeH / 2;
        QColor line = timeColor;
        line.setAlpha(70);
        painter->setPen(QPen(line, 1.0));
        const int labelW = painter->fontMetrics().horizontalAdvance(dayLabel) + 16;
        const int labelX = streamLeft + (streamW - labelW) / 2;
        painter->drawLine(streamLeft + hPad, midY, labelX - 4, midY);
        painter->drawLine(labelX + labelW + 4, midY, streamLeft + streamW - hPad, midY);
        painter->setPen(timeColor);
        painter->drawText(QRect(labelX, y, labelW, lm.timeH), Qt::AlignCenter, dayLabel);
        y += dayH;
    }

    if (groupStart) {
        const int ax = ChatLayoutHelpers::avatarX(isMine, r.left(), viewWidth, hPad, avatarSize);
        QRect avatarRect(ax, y, avatarSize, avatarSize);
        painter->setBrush(avatarBg);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(avatarRect, 8, 8);
        painter->setPen(Qt::white);
        painter->setFont(chatFont(2, QFont::Bold));
        painter->drawText(avatarRect, Qt::AlignCenter, username.left(1).toUpper());
    }

    int nameOffset = 0;
    const int bubbleX = ChatLayoutHelpers::bubbleLeftX(
        isMine, r.left(), viewWidth, hPad, avatarSize, bubbleOuterW);
    if (groupStart && !isMine) {
        nameOffset = lm.nameH + 2;
        QRect nameRect(bubbleX, y, qMax(40, streamW - (bubbleX - r.left()) - hPad), lm.nameH);
        QColor nameCol = cc.nameColor;
        nameCol.setAlpha(200);
        painter->setPen(nameCol);
        QFont nameFont = chatFont(-2, QFont::DemiBold);
        painter->setFont(nameFont);
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, username);
    }

    QRect bubbleRect(bubbleX, y + nameOffset, bubbleOuterW, bubbleH);

    if (option.state & QStyle::State_Selected) {
        QColor wash = bubbleColor;
        const QColor acc = cc.selectionBg;
        wash = QColor::fromRgb(
            int(wash.red()   * 0.82 + acc.red()   * 0.18),
            int(wash.green() * 0.82 + acc.green() * 0.18),
            int(wash.blue()  * 0.82 + acc.blue()  * 0.18));
        painter->setBrush(wash);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(bubbleRect, 10, 10);
        QColor edge = acc;
        edge.setAlpha(140);
        painter->setPen(QPen(edge, 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(bubbleRect).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
    } else {
        painter->setBrush(bubbleColor);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(bubbleRect, 10, 10);
    }

    if (index.data(ChatMessageModel::FlashRole).toBool()) {
        painter->setPen(QPen(cc.flashBorder, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(bubbleRect.adjusted(-1, -1, 1, 1), 10, 10);
    }

    int textOffset = vPad;
    if (replyToId > 0) {
        QRect replyRect(bubbleRect.left() + 8, bubbleRect.top() + 4, bubbleRect.width() - 16, lm.replyH - 4);
        painter->setPen(Qt::NoPen);
        painter->setBrush(replyToDeleted ? cc.replyDeletedBg : cc.replyBg);
        painter->drawRoundedRect(replyRect, 3, 3);
        painter->setPen(replyToDeleted ? cc.replyDeletedText : cc.replyText);
        painter->setFont(chatFont(-2));
        QRect nameLine(replyRect.left() + 4, replyRect.top() + 1, replyRect.width() - 8, 13);
        painter->drawText(nameLine, Qt::AlignLeft | Qt::AlignVCenter, "↩ " + replyToName);
        if (!replyToDeleted && !replyToText.isEmpty()) {
            painter->setPen(cc.textSecondary);
            QRect textLine(replyRect.left() + 4, replyRect.top() + 14, replyRect.width() - 8, 13);
            painter->drawText(textLine, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, replyToText);
        }
        textOffset += lm.replyH;
    }

    painter->save();
    painter->translate(bubbleRect.left() + 8, bubbleRect.top() + textOffset);
    doc->drawContents(painter);
    painter->restore();

    {
        QFont timeFont = chatFont(-2);
        painter->setFont(timeFont);
        QColor tc = timeColor;
        tc.setAlpha(175);
        painter->setPen(tc);
        const QRect timeRect(bubbleRect.left() + 6, bubbleRect.bottom() - lm.timeH - 3, bubbleRect.width() - 12, lm.timeH);
        painter->drawText(timeRect, Qt::AlignRight | Qt::AlignVCenter, timeStr);
    }

    if (!reactions.isEmpty()) {
        int reactionsY = bubbleRect.bottom() + 4;
        int rx = bubbleRect.left();
        painter->setFont(chatFont(-1));
        for (const auto& react : reactions) {
            const QString label = react.emoji + QLatin1Char(' ') + QString::number(react.users.size());
            const int w = painter->fontMetrics().horizontalAdvance(label) + 12;
            QRect chipRect(rx, reactionsY, w, lm.reactionH);
            const bool iReacted = react.users.contains(currentUser);
            painter->setBrush(iReacted ? cc.reactionBgActive : cc.reactionBg);
            painter->setPen(cc.reactionBorder);
            painter->drawRoundedRect(chipRect, 6, 6);
            painter->setPen(cc.text);
            painter->drawText(chipRect, Qt::AlignCenter, label);
            rx += w + 4;
        }
    }

    painter->restore();
}

QSize ChatMessageDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const bool isMine = index.data(ChatMessageModel::IsMineRole).toBool();
    const bool deleted = index.data(ChatMessageModel::DeletedRole).toBool();
    const QList<ChatReaction> reactions = index.data(ChatMessageModel::ReactionsRole).value<QList<ChatReaction>>();
    const bool groupStart = chatIsGroupStart(index);
    const bool daySep = chatNeedsDaySeparator(index) && groupStart && index.row() > 0;

    int viewWidth = option.rect.width();
    if (viewWidth < 200)
        viewWidth = 600;
    const int streamW = ChatLayoutHelpers::conversationWidth(viewWidth);

    const ChatLayoutMetrics lm = chatLayout();
    const int ideal = measureBubbleIdeal(index, deleted, chatFont(0));
    const int textW = ChatLayoutHelpers::bubbleTextWidth(ideal, streamW);

    int nameH = 0;
    if (groupStart && !isMine)
        nameH = lm.nameH + 2;

    int replyH = 0;
    if (index.data(ChatMessageModel::ReplyToIdRole).toLongLong() > 0)
        replyH = lm.replyH;

    int textH = 0;
    {
        std::unique_ptr<QTextDocument> doc(getDocument(index, textW));
        textH = static_cast<int>(realDocHeight(doc.get()));
    }

    const int bottomPad = ChatLayoutHelpers::bubbleTimeBottomPad(lm.timeH, lm.vPad, true);
    const int topPad = groupStart ? 6 : 2;
    int totalH = topPad + nameH + replyH + textH + lm.vPad + bottomPad;
    if (!reactions.isEmpty())
        totalH += lm.reactionH + 6;
    else
        totalH += groupStart ? 8 : 4;
    if (daySep)
        totalH += lm.timeH + 10;

    return QSize(viewWidth, totalH);
}

static void chatBubbleGeometry(const QStyleOptionViewItem& option, const QModelIndex& index, int* outBubbleX, int* outBubbleOuterW, int* outBubbleBottom, int* outNameOffset)
{
    const ChatLayoutMetrics lm = chatLayout();
    const bool isMine = index.data(ChatMessageModel::IsMineRole).toBool();
    const bool deleted = index.data(ChatMessageModel::DeletedRole).toBool();
    const bool groupStart = chatIsGroupStart(index);
    const bool edited = index.data(ChatMessageModel::EditedRole).toBool();
    const qint64 date = index.data(ChatMessageModel::DateRole).toLongLong();
    const QRect r = option.rect;
    int viewWidth = r.width();
    if (viewWidth < 200)
        viewWidth = 600;
    const int streamW = ChatLayoutHelpers::conversationWidth(viewWidth);

    const int ideal = measureBubbleIdeal(index, deleted, chatFont(0));
    const int textW = ChatLayoutHelpers::bubbleTextWidth(ideal, streamW);
    int bubbleOuterW = ChatLayoutHelpers::bubbleOuterWidth(textW);

    int nameOffset = 0;
    if (groupStart && !isMine)
        nameOffset = lm.nameH + 2;

    int dayH = 0;
    if (chatNeedsDaySeparator(index) && groupStart && index.row() > 0)
        dayH = lm.timeH + 10;

    const int replyH = index.data(ChatMessageModel::ReplyToIdRole).toLongLong() > 0 ? lm.replyH : 0;
    QTextDocument doc;
    doc.setDocumentMargin(0);
    doc.setDefaultFont(chatFont(0));
    QString text = index.data(ChatMessageModel::MessageRole).toString();
    if (deleted)
        doc.setPlainText("This message has been deleted");
    else {
        text.replace(QRegularExpression("(?<!\n)\n(?!\n)"), "  \n");
        doc.setMarkdown(text);
    }
    doc.setTextWidth(textW);
    const int textH = static_cast<int>(doc.size().height());

    QString timeStr = formatTime(date);
    if (edited)
        timeStr += " ✏";
    const int timeW = QFontMetrics(chatFont(-2)).horizontalAdvance(timeStr) + 2;
    bubbleOuterW = ChatLayoutHelpers::bubbleOuterWithTime(bubbleOuterW, textH, QFontMetrics(chatFont(0)).lineSpacing(), timeW);
    const int maxOuter = streamW - 2 * lm.hPad - (lm.avatarSize + 8);
    if (bubbleOuterW > maxOuter && maxOuter > 40)
        bubbleOuterW = maxOuter;
    const int bottomPad = ChatLayoutHelpers::bubbleTimeBottomPad(lm.timeH, lm.vPad, true);
    const int bubbleH = textH + replyH + lm.vPad + bottomPad;

    const int y = r.top() + (groupStart ? 6 : 2) + dayH;
    const int bubbleX = ChatLayoutHelpers::bubbleLeftX(isMine, r.left(), viewWidth, lm.hPad, lm.avatarSize, bubbleOuterW);

    if (outBubbleX) *outBubbleX = bubbleX;
    if (outBubbleOuterW) *outBubbleOuterW = bubbleOuterW;
    if (outBubbleBottom) *outBubbleBottom = y + nameOffset + bubbleH;
    if (outNameOffset) *outNameOffset = nameOffset;
}

bool ChatMessageDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) {
    Q_UNUSED(model);
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            const ChatLayoutMetrics lm = chatLayout();
            int bubbleX = 0, bubbleOuterW = 0, bubbleBottom = 0, nameOffset = 0;
            chatBubbleGeometry(option, index, &bubbleX, &bubbleOuterW, &bubbleBottom, &nameOffset);

            qint64 replyToId = index.data(ChatMessageModel::ReplyToIdRole).toLongLong();
            if (replyToId > 0) {
                const int replyTop = option.rect.top() + (chatIsGroupStart(index) ? 6 : 2) + nameOffset + 4 + (chatNeedsDaySeparator(index) && chatIsGroupStart(index) && index.row() > 0 ? lm.timeH + 6 : 0);
                QRect replyClickRect(bubbleX + 8, replyTop, bubbleOuterW - 16, lm.replyH - 4);
                if (replyClickRect.contains(me->pos())) {
                    Q_EMIT replyClicked(replyToId);
                    return true;
                }
            }

            QList<ChatReaction> reactions = index.data(ChatMessageModel::ReactionsRole).value<QList<ChatReaction>>();
            if (!reactions.isEmpty()) {
                int rx = bubbleX;
                int reactionsY = bubbleBottom + 4;
                QFontMetrics fm(chatFont(-1));
                for (const auto& react : reactions) {
                    QString label = react.emoji + QLatin1Char(' ') + QString::number(react.users.size());
                    int w = fm.horizontalAdvance(label) + 12;
                    QRect chipRect(rx, reactionsY, w, lm.reactionH);
                    if (chipRect.contains(me->pos())) {
                        Q_EMIT reactionClicked(index.data(ChatMessageModel::IdRole).toLongLong(), react.emoji);
                        return true;
                    }
                    rx += w + 4;
                }
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

bool ChatMessageDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index) {
    Q_UNUSED(view);
    if (event->type() == QEvent::ToolTip) {
        QList<ChatReaction> reactions = index.data(ChatMessageModel::ReactionsRole).value<QList<ChatReaction>>();
        if (reactions.isEmpty())
            return false;

        const ChatLayoutMetrics lm = chatLayout();
        int bubbleX = 0, bubbleOuterW = 0, bubbleBottom = 0;
        chatBubbleGeometry(option, index, &bubbleX, &bubbleOuterW, &bubbleBottom, nullptr);
        int rx = bubbleX;
        int reactionsY = bubbleBottom + 4;
        QFontMetrics fm(chatFont(-1));
        const QPoint pos = event->pos();
        for (const auto& react : reactions) {
            QString label = react.emoji + QLatin1Char(' ') + QString::number(react.users.size());
            int w = fm.horizontalAdvance(label) + 12;
            QRect chipRect(rx, reactionsY, w, lm.reactionH);
            if (chipRect.contains(pos)) {
                QToolTip::showText(event->globalPos(), react.users.join(QLatin1Char('\n')));
                return true;
            }
            rx += w + 4;
        }
        QToolTip::hideText();
        return true;
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}

TodoWidget::TodoWidget(QWidget* parent) : QWidget(parent), updating(false) {
    setObjectName("TodoPanel");

    panelFrame = new QFrame(this);
    panelFrame->setObjectName("TodoPanelFrame");

    titleLabel = new QLabel("Team Notes", panelFrame);
    titleLabel->setObjectName("TodoTitle");

    renderedView = new QTextBrowser(panelFrame);
    renderedView->setOpenExternalLinks(true);
    renderedView->setPlaceholderText(QString());
    renderedView->setFrameShape(QFrame::NoFrame);
    renderedView->setObjectName("TodoBody");

    emptyHintLabel = new QLabel(panelFrame);
    emptyHintLabel->setVisible(false);

    editor = new QPlainTextEdit(panelFrame);
    editor->setPlaceholderText("Write notes in Markdown...");
    editor->setObjectName("TodoEditor");

    livePreview = new QTextBrowser(panelFrame);
    livePreview->setFrameShape(QFrame::NoFrame);
    livePreview->setObjectName("TodoPreview");

    auto* editSplitter = new QSplitter(Qt::Vertical, panelFrame);
    editSplitter->addWidget(editor);
    editSplitter->addWidget(livePreview);
    editSplitter->setStretchFactor(0, 3);
    editSplitter->setStretchFactor(1, 1);

    contentStack = new QStackedWidget(panelFrame);
    contentStack->addWidget(renderedView);
    contentStack->addWidget(editSplitter);

    toolbarRow = new QFrame(panelFrame);
    toolbarRow->setObjectName("TodoMdToolbar");
    auto* toolbarLayout = new QHBoxLayout(toolbarRow);
    toolbarLayout->setContentsMargins(4, 2, 4, 2);
    toolbarLayout->setSpacing(3);

    auto makeToolBtn = [&](const QString& text, const QString& tooltip, std::function<void()> fn) {
        auto* btn = new QToolButton(toolbarRow);
        btn->setText(text);
        btn->setToolTip(tooltip);
        btn->setFixedHeight(FontManager::instance().typography().controlInnerH);
        btn->setMinimumWidth(26);
        btn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        connect(btn, &QToolButton::clicked, this, fn);
        toolbarLayout->addWidget(btn);
    };

    makeToolBtn("B", "Bold", [this]() { insertMarkdown("**", "**"); });
    makeToolBtn("I", "Italic", [this]() { insertMarkdown("*", "*"); });
    makeToolBtn("U", "Underline", [this]() { insertMarkdown("<u>", "</u>"); });
    makeToolBtn("S", "Strikethrough", [this]() { insertMarkdown("~~", "~~"); });
    makeToolBtn("<>", "Code", [this]() { insertMarkdown("`", "`"); });
    makeToolBtn("H1", "Heading 1", [this]() { insertMarkdown("# ", ""); });
    makeToolBtn("H2", "Heading 2", [this]() { insertMarkdown("## ", ""); });
    makeToolBtn("H3", "Heading 3", [this]() { insertMarkdown("### ", ""); });
    makeToolBtn("-", "Bullet list", [this]() { insertMarkdown("- ", ""); });
    makeToolBtn("1.", "Numbered list", [this]() { insertMarkdown("1. ", ""); });
    makeToolBtn("☐", "Checkbox", [this]() { insertMarkdown("- [ ] ", ""); });
    makeToolBtn("☑", "Checked", [this]() { insertMarkdown("- [x] ", ""); });
    makeToolBtn(">", "Quote", [this]() { insertMarkdown("> ", ""); });
    toolbarLayout->addStretch();
    toolbarRow->setVisible(false);

    editBtn = new QPushButton("Edit", panelFrame);
    editBtn->setObjectName("TodoEditBtn");
    editBtn->setCursor(Qt::PointingHandCursor);
    connect(editBtn, &QPushButton::clicked, this, [this]() { setEditMode(true); });

    saveBtn = new QPushButton("Save", panelFrame);
    saveBtn->setObjectName("TodoSaveBtn");
    saveBtn->setCursor(Qt::PointingHandCursor);
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        Q_EMIT saveRequested(editor->toPlainText());
        setEditMode(false);
    });

    cancelBtn = new QPushButton(QStringLiteral("Cancel"), panelFrame);
    cancelBtn->setObjectName(QStringLiteral("TodoCancelBtn"));
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QPushButton::clicked, this, [this]() { setEditMode(false); });

    statusLabel = new QLabel(QString(), panelFrame);
    statusLabel->setObjectName(QStringLiteral("TodoStatus"));
    statusLabel->setWordWrap(true);

    auto* wrapSwitch = new oclero::qlementine::Switch(panelFrame);
    wrapSwitch->setFixedSize(34, 16);
    wrapSwitch->setChecked(true);
    wrapSwitch->setToolTip(QStringLiteral("Word wrap"));
    auto* wrapLabel = new QLabel(QStringLiteral("Wrap"), panelFrame);
    wrapLabel->setObjectName(QStringLiteral("TodoMuted"));
    connect(wrapSwitch, &oclero::qlementine::Switch::checkStateChanged, this, [this](int state) {
        bool wrap = (state == Qt::Checked);
        editor->setWordWrapMode(wrap ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
        renderedView->setWordWrapMode(wrap ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
    });

    wrapRowWidget = new QWidget(panelFrame);
    auto* wrapRow = new QHBoxLayout(wrapRowWidget);
    wrapRow->setContentsMargins(0, 0, 0, 0);
    wrapRow->setSpacing(4);
    wrapRow->addWidget(wrapSwitch);
    wrapRow->addWidget(wrapLabel);

    auto* headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(6);
    headerRow->addWidget(titleLabel, 1);

    auto* footerRow = new QHBoxLayout();
    footerRow->setContentsMargins(0, 0, 0, 0);
    footerRow->setSpacing(6);
    footerRow->addWidget(wrapRowWidget, 0);
    footerRow->addStretch(1);
    footerRow->addWidget(cancelBtn, 0);
    footerRow->addWidget(saveBtn, 0);
    footerRow->addWidget(editBtn, 0);
    saveBtn->setVisible(false);
    cancelBtn->setVisible(false);

    auto* frameLayout = new QVBoxLayout(panelFrame);
    frameLayout->setContentsMargins(12, 10, 12, 10);
    frameLayout->setSpacing(8);
    frameLayout->addLayout(headerRow, 0);
    frameLayout->addWidget(toolbarRow, 0);
    frameLayout->addWidget(contentStack, 1);
    frameLayout->addWidget(statusLabel, 0);
    frameLayout->addLayout(footerRow, 0);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(panelFrame, 1);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setMinimumWidth(kPanelMinW);
    setMaximumWidth(QWIDGETSIZE_MAX);

    debounceTimer = new QTimer(this);
    debounceTimer->setSingleShot(true);
    debounceTimer->setInterval(500);
    connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
        if (!updating) debounceTimer->start();
    });
    connect(debounceTimer, &QTimer::timeout, this, &TodoWidget::updatePreview);

    applyFonts();
    applyPanelStyle();
    connect(&FontManager::instance(), &FontManager::typographyChanged, this, [this]() {
        applyFonts();
        applyPanelStyle();
    });
    if (auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr)) {
        connect(qs, &oclero::qlementine::QlementineStyle::themeChanged, this, &TodoWidget::refreshTheme,
                Qt::UniqueConnection);
    }
    updateEmptyChrome();
}

void TodoWidget::refreshTheme()
{
    applyFonts();
    applyPanelStyle();
    updateEmptyChrome();
}

QString TodoWidget::content() const
{
    if (!editor)
        return {};
    return editor->toPlainText();
}

bool TodoWidget::isEmpty() const
{
    if (editor && !editor->toPlainText().trimmed().isEmpty())
        return false;
    return true;
}

QSize TodoWidget::sizeHint() const
{
    return QSize(kPanelPreferredW, 400);
}

QSize TodoWidget::minimumSizeHint() const
{
    return QSize(kPanelMinW, 200);
}

void TodoWidget::showEmptyPlaceholder()
{
    if (!renderedView)
        return;

    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();
    const AppTypography& ty = FontManager::instance().typography();
    const int bodyPx = qMax(11, ty.baseSize);
    const int captionPx = qMax(10, ty.chromeFontPx);
    const QString muted = t.secondaryAlternativeColor.name();
    const QString family = ty.regular.family().toHtmlEscaped();

    renderedView->setHtml(QStringLiteral(
        "<div style='color:%1;text-align:center;margin-top:30%;"
        "font-family:'%2';font-size:%3px;line-height:1.6;padding:0 20px;'>"
        "<div style='opacity:0.9;font-size:%4px;'>Shared with operators · Markdown supported</div>"
        "<div style='opacity:0.7;font-size:%4px;margin-top:14px;'>Click <b>Edit</b> to start</div>"
        "</div>")
        .arg(muted, family)
        .arg(bodyPx)
        .arg(captionPx));
}

void TodoWidget::applyPanelStyle()
{
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();
    const AppTypography& ty = FontManager::instance().typography();
    const int fontPx = ty.chromeFontPx;
    const int titlePx = qMax(fontPx + 1, ty.baseSize);
    const int btnH = ty.controlInnerH;
    const QString family = ty.regular.family();

    if (panelFrame) {
        panelFrame->setStyleSheet(QStringLiteral(
            "QFrame#TodoPanelFrame {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 10px;"
            "}"
            "QLabel#TodoTitle {"
            "  color: %3; font-family: '%10'; font-size: %7px; font-weight: 600;"
            "}"
            "QLabel#TodoMuted, QLabel#TodoStatus {"
            "  color: %4; font-family: '%10'; font-size: %8px;"
            "}"
            "QTextBrowser#TodoBody, QTextBrowser#TodoPreview, QPlainTextEdit#TodoEditor {"
            "  background: transparent; border: none; color: %5;"
            "  font-family: '%10';"
            "}"
            "QFrame#TodoMdToolbar {"
            "  background: transparent; border-bottom: 1px solid %2;"
            "}"
            "QPushButton#TodoEditBtn {"
            "  background-color: %3; color: %6; border: none; border-radius: 5px;"
            "  padding: 4px 16px; font-weight: 600; font-size: %8px; font-family: '%10';"
            "}"
            "QPushButton#TodoEditBtn:hover { background-color: %9; }"
            "QPushButton#TodoSaveBtn {"
            "  background-color: %3; color: %6; border: none; border-radius: 5px;"
            "  padding: 4px 14px; font-weight: 600; font-size: %8px; font-family: '%10';"
            "}"
            "QPushButton#TodoSaveBtn:hover { background-color: %9; }"
            "QPushButton#TodoCancelBtn {"
            "  background: transparent; color: %4; border: 1px solid %2; border-radius: 5px;"
            "  padding: 4px 12px; font-size: %8px; font-family: '%10';"
            "}"
            "QPushButton#TodoCancelBtn:hover { color: %5; border-color: %4; }"
            "QToolButton {"
            "  color: %5; font-family: '%10'; font-size: %8px;"
            "}"
        ).arg(t.backgroundColorMain2.name(),
              t.borderColor.name(),
              t.primaryColor.name(),
              t.secondaryAlternativeColor.name(),
              t.secondaryColor.name(),
              t.primaryColorForeground.name())
         .arg(titlePx)
         .arg(fontPx)
         .arg(t.primaryColorHovered.name())
         .arg(family));
    }
    if (editBtn) editBtn->setFixedHeight(btnH + 2);
    if (saveBtn) saveBtn->setFixedHeight(btnH + 2);
    if (cancelBtn) cancelBtn->setFixedHeight(btnH + 2);

    if (!editMode && isEmpty())
        showEmptyPlaceholder();
}

void TodoWidget::updateEmptyChrome()
{
    if (contentStack)
        contentStack->setVisible(true);
    if (emptyHintLabel)
        emptyHintLabel->setVisible(false);
    if (wrapRowWidget)
        wrapRowWidget->setVisible(true);
    if (titleLabel)
        titleLabel->setText(editMode ? QStringLiteral("Editing notes") : QStringLiteral("Team Notes"));

    if (editBtn) {
        editBtn->setVisible(!editMode);
        editBtn->setToolTip(QStringLiteral("Edit team notes"));
    }
    if (statusLabel)
        statusLabel->setVisible(!statusLabel->text().isEmpty());

    if (!editMode) {
        contentStack->setCurrentIndex(0);
        if (isEmpty())
            showEmptyPlaceholder();
    }

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setMinimumWidth(kPanelMinW);
    setMaximumWidth(QWIDGETSIZE_MAX);
    updateGeometry();
    Q_EMIT contentPresenceChanged(!isEmpty() || editMode);
}

void TodoWidget::applyFonts()
{
    const AppTypography& ty = FontManager::instance().typography();
    const QFont body = ty.regular;
    const QFont mono = ty.mono;
    QFont titleFont = ty.regular;
    titleFont.setWeight(QFont::DemiBold);
    titleFont.setPointSize(qMax(ty.baseSize, ty.chromeFontPx + 1));

    if (titleLabel)
        titleLabel->setFont(titleFont);
    if (renderedView) {
        renderedView->setFont(body);
        renderedView->document()->setDefaultFont(body);
    }
    if (editor)
        editor->setFont(mono);
    if (livePreview) {
        livePreview->setFont(body);
        livePreview->document()->setDefaultFont(body);
    }
    if (statusLabel)
        statusLabel->setFont(ty.caption);

    const int btnH = ty.controlInnerH;
    if (toolbarRow) {
        for (auto* btn : toolbarRow->findChildren<QToolButton*>())
            btn->setFixedHeight(btnH);
    }
    if (editMode) {
        updatePreview();
    } else if (renderedView && editor) {
        QString text = editor->toPlainText();
        if (text.trimmed().isEmpty()) {
            showEmptyPlaceholder();
        } else {
            QString fixed = text;
            fixed.replace(QRegularExpression("(?<!\\n)\\n(?!\\n)"), "  \n");
            QTextDocument doc;
            doc.setDefaultFont(body);
            doc.setMarkdown(fixed);
            renderedView->setHtml(doc.toHtml());
        }
    }
}

void TodoWidget::setEditMode(bool on) {
    editMode = on;
    contentStack->setCurrentIndex(on ? 1 : 0);
    contentStack->setVisible(true);
    toolbarRow->setVisible(on);
    editBtn->setVisible(!on);
    saveBtn->setVisible(on);
    cancelBtn->setVisible(on);
    if (wrapRowWidget)
        wrapRowWidget->setVisible(true);
    if (titleLabel)
        titleLabel->setText(on ? QStringLiteral("Editing notes") : QStringLiteral("Team Notes"));

    setMinimumWidth(kPanelMinW);
    setMaximumWidth(QWIDGETSIZE_MAX);

    if (on) {
        editor->setFocus();
        updatePreview();
        updateGeometry();
        Q_EMIT contentPresenceChanged(true);
    } else {
        const QString src = editor->toPlainText();
        if (src.trimmed().isEmpty()) {
            showEmptyPlaceholder();
        } else {
            QString fixed = src;
            fixed.replace(QRegularExpression("(?<!\\n)\\n(?!\\n)"), "  \n");
            QTextDocument doc;
            doc.setDefaultFont(FontManager::instance().typography().regular);
            doc.setMarkdown(fixed);
            renderedView->setHtml(doc.toHtml());
        }
        updateEmptyChrome();
    }
}

void TodoWidget::updatePreview() {
    QString text = editor->toPlainText();
    text.replace(QRegularExpression("(?<!\\n)\\n(?!\\n)"), "  \n");
    QTextDocument doc;
    doc.setDefaultFont(FontManager::instance().typography().regular);
    doc.setMarkdown(text);
    livePreview->setHtml(doc.toHtml());
}

void TodoWidget::insertMarkdown(const QString& before, const QString& after) {
    QTextCursor cursor = editor->textCursor();
    if (cursor.hasSelection()) {
        QString selected = cursor.selectedText();
        cursor.insertText(before + selected + after);
    } else {
        cursor.insertText(before + after);
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, after.length());
        editor->setTextCursor(cursor);
    }
}

void TodoWidget::SetTodo(const QString& content, const QString& updatedBy, qint64 updatedAt) {
    if (editMode) {
        statusLabel->setText(QString("Changed by %1 — press Save to overwrite").arg(updatedBy));
        statusLabel->setVisible(true);
        Q_EMIT contentPresenceChanged(true);
        return;
    }
    updating = true;
    editor->setPlainText(content);
    updating = false;

    if (content.trimmed().isEmpty()) {
        showEmptyPlaceholder();
    } else {
        QString fixed = content;
        fixed.replace(QRegularExpression("(?<!\\n)\\n(?!\\n)"), "  \n");
        QTextDocument doc;
        doc.setDefaultFont(FontManager::instance().typography().regular);
        doc.setMarkdown(fixed);
        renderedView->setHtml(doc.toHtml());
    }

    if (updatedAt > 0) {
        statusLabel->setText(QString("Last updated by %1 at %2").arg(updatedBy, formatDate(updatedAt)));
        statusLabel->setVisible(true);
    } else {
        statusLabel->clear();
        statusLabel->setVisible(false);
    }
    updateEmptyChrome();
}

ChatWidget::ChatWidget(AdaptixWidget* w) : DockTab("Chat", w->GetProfile()->GetProject(), ":/icons/chat", w), adaptixWidget(w)
{
    this->createUI();

    connect(searchBtn, &QPushButton::clicked, this, &ChatWidget::handleSearch);
    connect(searchInput, &QLineEdit::returnPressed, this, &ChatWidget::handleSearch);

    connect(messageDelegate, &ChatMessageDelegate::reactionClicked, this, &ChatWidget::handleReaction);
    connect(messageDelegate, &ChatMessageDelegate::replyClicked, this, [this](qint64 id) {
        for (int i = 0; i < messageModel->rowCount(); ++i) {
            QModelIndex idx = messageModel->index(i);
            if (idx.data(ChatMessageModel::IdRole).toLongLong() == id) {
                messageView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
                messageModel->flashId = id;
                messageModel->dataChanged(idx, idx);
                QTimer::singleShot(1500, this, [this, id]() {
                    messageModel->flashId = 0;
                    for (int j = 0; j < messageModel->rowCount(); ++j) {
                        QModelIndex idx2 = messageModel->index(j);
                        if (idx2.data(ChatMessageModel::IdRole).toLongLong() == id) {
                            messageModel->dataChanged(idx2, idx2);
                            break;
                        }
                    }
                });
                return;
            }
        }
    });

    auto* shortcutFind = new QShortcut(QKeySequence("Ctrl+F"), this);
    shortcutFind->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutFind, &QShortcut::activated, this, [this]() {
        toggleSearchChrome(true);
    });

    auto* shortcutEsc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    shortcutEsc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutEsc, &QShortcut::activated, this, [this]() {
        if (searchChrome && searchChrome->isVisible() && searchInput && searchInput->hasFocus()) {
            searchChrome->setVisible(false);
            return;
        }
        exitEditMode();
        exitReplyMode();
    });

    chatInput->installEventFilter(this);
    if (searchInput)
        searchInput->installEventFilter(this);

    connect(todoWidget, &TodoWidget::saveRequested, this, [this](const QString& content) {
        HttpReqChatUpdateTodoAsync(content, *(adaptixWidget->GetProfile()), [](bool, const QString&, const QJsonObject&) {});
    });

    connect(messageView->verticalScrollBar(), &QScrollBar::valueChanged, this, &ChatWidget::onScrollChanged);

    messageView->viewport()->installEventFilter(this);

    connect(adaptixWidget, &AdaptixWidget::SyncedSignal, this, [this]() {
        FinishPresync();
    });

    connect(dockWidget, &KDDockWidgets::QtWidgets::DockWidget::isCurrentTabChanged, this, [this](bool current) {
        if (current && adaptixWidget)
            adaptixWidget->ChatUnreadClear();
    });
    connect(dockWidget, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged, this, [this](bool open) {
        if (!open || !adaptixWidget || !dockWidget)
            return;
        auto* core = dockWidget->dockWidget();
        if (core && core->isCurrentTab())
            adaptixWidget->ChatUnreadClear();
    });

    auto refreshChrome = [this]() {
        if (messageDelegate)
            messageDelegate->clearCache();
        if (messageView) {
            messageView->doItemsLayout();
            messageView->viewport()->update();
        }
        if (chatInput) {
            chatInput->setFont(FontManager::instance().appRegularFont());
            chatInput->setFixedHeight(chatLayout().inputH);
        }
        if (todoWidget)
            todoWidget->refreshTheme();
        applyChatChromeMetrics();
        applyChatChromeStyle();
        positionChatOverlays();
    };

    connect(&FontManager::instance(), &FontManager::typographyChanged, this, refreshChrome);

    if (auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr)) {
        connect(qs, &oclero::qlementine::QlementineStyle::themeChanged, this, [this, refreshChrome]() {
            refreshChrome();
        });
    }

    this->dockWidget->setWidget(this);
}

ChatWidget::~ChatWidget() = default;

void ChatWidget::SetUpdatesEnabled(bool enabled) {
    QWidget::setUpdatesEnabled(enabled);
}

bool ChatWidget::eventFilter(QObject* obj, QEvent* event) {
    if (obj == chatInput && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && !(ke->modifiers() & Qt::ShiftModifier)) {
            handleSend();
            return true;
        }
    }
    if (obj == searchInput && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            if (searchChrome)
                searchChrome->setVisible(false);
            return true;
        }
    }
    if (obj == chatHost && (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest)) {
        positionChatOverlays();
    }
    if (messageView && obj == messageView->viewport() && event->type() == QEvent::Resize) {
        positionChatOverlays();
        if (messageModel->rowCount() > 0) {
            QMetaObject::invokeMethod(this, [this]() {
                messageView->doItemsLayout();
            }, Qt::QueuedConnection);
        }
    }
    return QWidget::eventFilter(obj, event);
}

void ChatWidget::createUI() {
    auto* profile = adaptixWidget->GetProfile();

    messageModel = new ChatMessageModel(this);
    messageModel->currentUser = profile->GetUsername();

    messageDelegate = new ChatMessageDelegate(profile->GetUsername(), this);

    messageView = new QListView(this);
    messageView->setModel(messageModel);
    messageView->setItemDelegate(messageDelegate);
    messageView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    messageView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    messageView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    messageView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(messageView, &QListView::customContextMenuRequested, this, [this](const QPoint& pos) {
        QModelIndex idx = messageView->indexAt(pos);
        if (!idx.isValid()) return;
        qint64 id = idx.data(ChatMessageModel::IdRole).toLongLong();
        bool isMine = idx.data(ChatMessageModel::IsMineRole).toBool();
        bool deleted = idx.data(ChatMessageModel::DeletedRole).toBool();
        QString msgText = idx.data(ChatMessageModel::MessageRole).toString();
        QString username = idx.data(ChatMessageModel::UsernameRole).toString();

        QMenu menu(this);
        auto* reactMenu = menu.addMenu(QIcon(":/icons/add_reaction"), "Add reaction");
        for (const auto& emoji : EMOJI_LIST) {
            connect(reactMenu->addAction(emoji), &QAction::triggered, this, [this, id, emoji]() {
                handleReaction(id, emoji);
            });
        }
        if (!deleted) {
            menu.addSeparator();
            if (isMine) {
                connect(menu.addAction(QIcon(":/icons/edit_note"), "Edit"), &QAction::triggered, this, [this, id]() { enterEditMode(id); });
                connect(menu.addAction(QIcon(":/icons/delete"), "Delete"), &QAction::triggered, this, [this, id]() {
                    HttpReqChatDeleteMessageAsync(id, *(adaptixWidget->GetProfile()), [](bool, const QString&, const QJsonObject&) {});
                });
            }
            menu.addSeparator();
            connect(menu.addAction(QIcon(":/icons/reply"), "Reply"), &QAction::triggered, this, [this, id, username]() {
                enterReplyMode(id, username);
            });
            connect(menu.addAction(QIcon(":/icons/notes"), "Send to ToDo"), &QAction::triggered, this, [this, msgText, username]() {
                sendToTodo("**" + username + "**: " + msgText);
            });
            menu.addSeparator();
            connect(menu.addAction(QIcon(":/icons/copy_all"), "Copy text"), &QAction::triggered, this, [msgText]() {
                QApplication::clipboard()->setText(msgText);
            });
            connect(menu.addAction(QIcon(":/icons/two_pager"), "Working text"), &QAction::triggered, this, [msgText, this]() {
                auto* dlg = new QDialog(this);
                dlg->setWindowTitle("Message text");
                dlg->resize(500, 300);
                auto* lay = new QVBoxLayout(dlg);
                auto* edit = new QTextEdit(dlg);
                edit->setPlainText(msgText);
                edit->selectAll();
                lay->addWidget(edit);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->show();
            });
        }
        menu.exec(messageView->viewport()->mapToGlobal(pos));
    });

    chatHost = new QWidget(this);
    chatHost->setObjectName(QStringLiteral("ChatHost"));
    messageView->setParent(chatHost);
    auto* hostLayout = new QVBoxLayout(chatHost);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);
    hostLayout->addWidget(messageView, 1);

    searchChrome = new QFrame(chatHost);
    searchChrome->setObjectName(QStringLiteral("ConsoleSearchChrome"));
    searchInput = new QLineEdit(searchChrome);
    searchInput->setPlaceholderText(QStringLiteral("Find messages"));
    searchInput->setMinimumWidth(140);
    searchInput->setMaximumWidth(220);
    searchInput->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    searchBtn = new QPushButton(QStringLiteral("Search"), searchChrome);
    searchBtn->setVisible(false);
    searchResultLabel = new QLabel(QStringLiteral("0"), searchChrome);
    searchResultLabel->setObjectName(QStringLiteral("ConsoleSearchCount"));
    searchResultLabel->setMinimumWidth(28);
    searchResultLabel->setAlignment(Qt::AlignCenter);
    searchResultLabel->setToolTip(QStringLiteral("Matches in search result"));

    hideDeletedSwitch = new oclero::qlementine::Switch(searchChrome);
    hideDeletedSwitch->setToolTip(QStringLiteral("Hide deleted messages"));
    connect(hideDeletedSwitch, &oclero::qlementine::Switch::checkStateChanged, this, [this](int state) {
        hideDeleted = (state == Qt::Checked);
        for (int i = 0; i < messageModel->rowCount(); ++i) {
            QModelIndex idx = messageModel->index(i);
            bool isDeleted = idx.data(ChatMessageModel::DeletedRole).toBool();
            messageView->setRowHidden(i, hideDeleted && isDeleted);
        }
    });
    hideDeletedLabel = new QLabel(QStringLiteral("Hide"), searchChrome);
    hideDeletedLabel->setObjectName(QStringLiteral("ConsoleSearchCount"));
    hideDeletedLabel->setToolTip(QStringLiteral("Hide deleted messages"));

    clearChatBtn = new QToolButton(searchChrome);
    clearChatBtn->setText(QStringLiteral("Clear"));
    clearChatBtn->setObjectName(QStringLiteral("ChatClearBtn"));
    clearChatBtn->setAutoRaise(true);
    clearChatBtn->setCursor(Qt::PointingHandCursor);
    clearChatBtn->setToolTip(QStringLiteral("Delete all chat messages permanently"));
    connect(clearChatBtn, &QToolButton::clicked, this, [this]() {
        auto reply = QMessageBox::question(this, QStringLiteral("Clear chat"), QStringLiteral("Delete all chat messages permanently?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;

        HttpReqChatClearAsync(*(adaptixWidget->GetProfile()), [this](bool success, const QString&, const QJsonObject&) {
            if (success) {
                messageModel->clear();
                hasMore = false;
                updateLoadStatus();
                updateHistoryControls();
            }
        });
    });

    searchCloseBtn = new QToolButton(searchChrome);
    searchCloseBtn->setText(QStringLiteral("✕"));
    searchCloseBtn->setAutoRaise(true);
    searchCloseBtn->setCursor(Qt::PointingHandCursor);
    searchCloseBtn->setToolTip(QStringLiteral("Close search (Esc)"));
    connect(searchCloseBtn, &QToolButton::clicked, this, [this]() {
        if (searchChrome) searchChrome->setVisible(false);
    });

    deleteSelectedBtn = new QPushButton(QStringLiteral("Delete selected"), chatHost);
    deleteSelectedBtn->setCursor(Qt::PointingHandCursor);
    deleteSelectedBtn->setVisible(false);
    connect(deleteSelectedBtn, &QPushButton::clicked, this, [this]() {
        QModelIndexList selected = messageView->selectionModel()->selectedIndexes();
        if (selected.isEmpty()) return;
        QString myName = adaptixWidget->GetProfile()->GetUsername();
        for (const QModelIndex& idx : selected) {
            qint64 id = idx.data(ChatMessageModel::IdRole).toLongLong();
            QString owner = idx.data(ChatMessageModel::UsernameRole).toString();
            bool deleted = idx.data(ChatMessageModel::DeletedRole).toBool();
            if (deleted || owner != myName) continue;
            HttpReqChatDeleteMessageAsync(id, *(adaptixWidget->GetProfile()), [](bool, const QString&, const QJsonObject&) {});
        }
        messageView->clearSelection();
    });
    connect(messageView->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() {
        deleteSelectedBtn->setVisible(messageView->selectionModel()->selectedIndexes().size() > 1);
        positionChatOverlays();
    });

    auto* searchLayout = new QHBoxLayout(searchChrome);
    searchLayout->setContentsMargins(8, 2, 6, 2);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(searchInput, 1);
    searchLayout->addWidget(searchResultLabel, 0);
    searchLayout->addWidget(hideDeletedSwitch, 0);
    searchLayout->addWidget(hideDeletedLabel, 0);
    searchLayout->addWidget(clearChatBtn, 0);
    searchLayout->addWidget(searchCloseBtn, 0);
    searchChrome->setVisible(true);

    historyBar = new QFrame(chatHost);
    historyBar->setObjectName(QStringLiteral("ConsoleHistoryBar"));
    historyBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    autoLoadSwitch = new oclero::qlementine::Switch(historyBar);
    autoLoadSwitch->setChecked(autoLoadEarlier);
    autoLoadSwitch->setToolTip(QStringLiteral("Auto-load older messages when scrolling to the top"));
    connect(autoLoadSwitch, &oclero::qlementine::Switch::toggled, this, [this](bool on) {
        autoLoadEarlier = on;
    });

    loadStatusLabel = new QLabel(QStringLiteral("—"), historyBar);
    loadStatusLabel->setObjectName(QStringLiteral("ConsoleHistoryStatus"));
    loadStatusLabel->setToolTip(QStringLiteral("Loaded messages / total on server"));

    pageSizeLabel = new QLabel(QStringLiteral("count"), historyBar);
    pageSizeLabel->setObjectName(QStringLiteral("ConsoleHistoryMuted"));
    pageSizeLabel->setToolTip(QStringLiteral("History page size"));

    pageSizeSpin = new QSpinBox(historyBar);
    pageSizeSpin->setRange(10, 500);
    pageSizeSpin->setSingleStep(10);
    pageSizeSpin->setValue(pageSize);
    pageSizeSpin->setFixedWidth(60);
    pageSizeSpin->setToolTip(QStringLiteral("Messages per page"));
    pageSizeSpin->setButtonSymbols(QAbstractSpinBox::UpDownArrows);
    connect(pageSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        pageSize = qBound(10, v, 500);
    });

    auto makeHistBtn = [this](const QString& name, const QString& text, const QString& tip) {
        auto* btn = new QToolButton(historyBar);
        btn->setObjectName(name);
        btn->setText(text);
        btn->setToolTip(tip);
        btn->setAutoRaise(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        return btn;
    };
    auto makeSep = [this]() {
        auto* sep = new QFrame(historyBar);
        sep->setObjectName(QStringLiteral("ConsoleHistorySep"));
        sep->setFrameShape(QFrame::VLine);
        sep->setFixedWidth(1);
        return sep;
    };

    loadEarlierBtn = makeHistBtn(QStringLiteral("HistBtnEarlier"), QStringLiteral("▲ Earlier"), QStringLiteral("Load older messages"));
    connect(loadEarlierBtn, &QToolButton::clicked, this, &ChatWidget::loadMore);

    jumpLatestBtn = makeHistBtn(QStringLiteral("HistBtnJump"), QStringLiteral("↓"), QStringLiteral("Jump to latest messages"));
    connect(jumpLatestBtn, &QToolButton::clicked, this, [this]() {
        messageView->scrollToBottom();
        atBottom = true;
    });

    auto* histLayout = new QHBoxLayout(historyBar);
    histLayout->setContentsMargins(8, 2, 8, 2);
    histLayout->setSpacing(6);
    histLayout->addWidget(autoLoadSwitch, 0);
    histLayout->addWidget(loadStatusLabel, 0);
    histLayout->addWidget(makeSep(), 0);
    histLayout->addWidget(loadEarlierBtn, 0);
    histLayout->addWidget(makeSep(), 0);
    histLayout->addWidget(jumpLatestBtn, 0);
    histLayout->addWidget(makeSep(), 0);
    histLayout->addWidget(pageSizeLabel, 0);
    histLayout->addWidget(pageSizeSpin, 0);

    applyChatChromeStyle();
    applyChatChromeMetrics();
    chatHost->installEventFilter(this);
    QTimer::singleShot(0, this, [this]() { positionChatOverlays(); });

    chatInput = new QPlainTextEdit(this);
    chatInput->setFixedHeight(chatLayout().inputH);
    chatInput->setFont(FontManager::instance().appRegularFont());
    chatInput->setPlaceholderText("Type a message... (Enter to send, Markdown supported)");

    mdPreview = new QTextBrowser(this);
    mdPreview->setMinimumHeight(24);
    mdPreview->setMaximumHeight(80);
    mdPreview->setVisible(false);

    sendBtn = new QPushButton("Send", this);
    sendBtn->setMinimumHeight(28);
    sendBtn->setMinimumWidth(72);
    sendBtn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sendBtn->setCursor(Qt::PointingHandCursor);
    sendBtn->setStyleSheet(
        QStringLiteral("QPushButton { background-color: palette(highlight); color: palette(highlighted-text); "
                       "border: none; border-radius: 4px; padding: 4px 14px; font-weight: 600; }"
                       "QPushButton:hover { background-color: palette(highlight); }"
                       "QPushButton:pressed { padding-top: 5px; padding-bottom: 3px; }"));
    connect(sendBtn, &QPushButton::clicked, this, &ChatWidget::handleSend);

    auto* mdSwitch = new oclero::qlementine::Switch(this);
    mdSwitch->setFixedSize(36, 18);
    mdSwitch->setToolTip("Markdown preview");
    auto* mdLabel = new QLabel("MD preview", this);
    connect(mdSwitch, &oclero::qlementine::Switch::checkStateChanged, this, [this](int state) {
        markdownMode = (state == Qt::Checked);
        mdPreview->setVisible(markdownMode);
        if (markdownMode) {
            QTextDocument doc;
            doc.setMarkdown(chatInput->toPlainText());
            mdPreview->setHtml(doc.toHtml());
        }
    });

    connect(chatInput, &QPlainTextEdit::textChanged, this, [this]() {
        if (markdownMode) {
            QTextDocument doc;
            doc.setMarkdown(chatInput->toPlainText());
            mdPreview->setHtml(doc.toHtml());
        }
    });

    auto* composerFooter = new QHBoxLayout();
    composerFooter->setContentsMargins(0, 0, 0, 0);
    composerFooter->setSpacing(8);
    composerFooter->addWidget(mdSwitch, 0);
    composerFooter->addWidget(mdLabel, 0);
    composerFooter->addStretch(1);
    composerFooter->addWidget(sendBtn, 0);

    auto* inputColumn = new QVBoxLayout();
    inputColumn->setContentsMargins(8, 2, 8, 4);
    inputColumn->setSpacing(4);
    inputColumn->addWidget(chatInput, 0);
    inputColumn->addWidget(mdPreview, 0);
    inputColumn->addLayout(composerFooter, 0);
    chatInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    chatInput->setMaximumWidth(QWIDGETSIZE_MAX);

    auto* chatArea = new QVBoxLayout();
    chatArea->setContentsMargins(0, 0, 0, 0);
    chatArea->setSpacing(2);
    chatArea->addWidget(chatHost, 1);

    replyPreview = new QFrame(this);
    replyPreview->setVisible(false);
    replyLabel = new QLabel(replyPreview);
    replyLabel->setWordWrap(true);
    auto* replyGotoBtn = new QPushButton("↗", replyPreview);
    replyGotoBtn->setFixedSize(20, 20);
    replyGotoBtn->setToolTip("Go to original message");
    connect(replyGotoBtn, &QPushButton::clicked, this, [this]() {
        if (replyToId <= 0) return;
        for (int i = 0; i < messageModel->rowCount(); ++i) {
            QModelIndex idx = messageModel->index(i);
            if (idx.data(ChatMessageModel::IdRole).toLongLong() == replyToId) {
                messageView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
                return;
            }
        }
    });
    auto* replyCloseBtn = new QPushButton("✕", replyPreview);
    replyCloseBtn->setFixedSize(20, 20);
    connect(replyCloseBtn, &QPushButton::clicked, this, &ChatWidget::exitReplyMode);
    auto* replyLayout = new QHBoxLayout(replyPreview);
    replyLayout->setContentsMargins(4, 2, 4, 2);
    replyLayout->addWidget(replyLabel, 1);
    replyLayout->addWidget(replyGotoBtn);
    replyLayout->addWidget(replyCloseBtn);

    chatArea->addWidget(replyPreview);
    chatArea->addLayout(inputColumn);

    auto* chatFrame = new QFrame(this);
    chatFrame->setObjectName(QStringLiteral("ChatColumn"));
    chatFrame->setLayout(chatArea);
    chatFrame->setMinimumWidth(TodoWidget::kChatMinW);
    chatFrame->setMaximumWidth(QWIDGETSIZE_MAX);

    todoWidget = new TodoWidget(this);

    mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(chatFrame);
    mainSplitter->addWidget(todoWidget);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setCollapsible(0, false);
    mainSplitter->setCollapsible(1, false);
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->setSizes({TodoWidget::kChatPreferredW, TodoWidget::kPanelPreferredW});
    todoWidget->setMinimumWidth(TodoWidget::kPanelMinW);
    todoWidget->setMaximumWidth(QWIDGETSIZE_MAX);
    if (QSplitterHandle* h = mainSplitter->handle(1))
        h->setEnabled(true);

    QTimer::singleShot(0, this, [this]() { seedSplitterSizes(); });

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(mainSplitter);
}

void ChatWidget::seedSplitterSizes()
{
    if (splitSeeded || !mainSplitter || !todoWidget)
        return;

    const int total = mainSplitter->width();
    if (total < 200)
        return;

    int chatW = qBound(TodoWidget::kChatMinW, (total * TodoWidget::kChatSeedPct) / 100, total - TodoWidget::kPanelMinW);
    int todoW = total - chatW;
    if (todoW < TodoWidget::kPanelMinW) {
        todoW = TodoWidget::kPanelMinW;
        chatW = qMax(TodoWidget::kChatMinW, total - todoW);
    }

    if (QWidget* chatCol = mainSplitter->widget(0)) {
        chatCol->setMinimumWidth(TodoWidget::kChatMinW);
        chatCol->setMaximumWidth(QWIDGETSIZE_MAX);
    }
    todoWidget->setMinimumWidth(TodoWidget::kPanelMinW);
    todoWidget->setMaximumWidth(QWIDGETSIZE_MAX);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 1);
    mainSplitter->setSizes({chatW, todoW});
    splitSeeded = true;
    positionChatOverlays();
}

void ChatWidget::applyChatChromeStyle()
{
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();
    const int fontPx = FontManager::instance().typography().chromeFontPx;
    const AppTypography& ty = FontManager::instance().typography();
    const int innerH = ty.controlInnerH;
    const int minH = qMax(16, innerH - 4);
    const QString monoFamily = ty.family;

    if (searchChrome) {
        searchChrome->setStyleSheet(QStringLiteral(
            "QFrame#ConsoleSearchChrome {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 8px;"
            "}"
            "QLabel#ConsoleSearchCount {"
            "  color: %3; font-size: %6px; padding: 0 2px;"
            "}"
            "QToolButton {"
            "  border: none; border-radius: 4px; padding: 0 4px;"
            "  color: %4; font-size: %6px; background: transparent;"
            "}"
            "QToolButton:hover { background-color: %5; }"
            "QToolButton#ChatClearBtn { color: %3; }"
            "QToolButton#ChatClearBtn:hover { color: %9; background-color: %5; }"
            "QLineEdit {"
            "  min-height: %7px; max-height: %8px;"
            "  padding-top: 0px; padding-bottom: 0px;"
            "  background: transparent; border: none;"
            "}"
        ).arg(t.backgroundColorMain3.name(),
              t.borderColor.name(),
              t.secondaryColor.name(),
              t.primaryColor.name(),
              t.backgroundColorMain4.name())
         .arg(fontPx)
         .arg(minH)
         .arg(innerH)
         .arg(t.statusColorError.name()));
    }

    if (sendBtn) {
        sendBtn->setStyleSheet(
            QStringLiteral("QPushButton { background-color: %1; color: %2; "
                           "border: none; border-radius: 4px; padding: 4px 14px; font-weight: 600; }"
                           "QPushButton:hover { background-color: %3; }"
                           "QPushButton:pressed { padding-top: 5px; padding-bottom: 3px; }")
                .arg(t.primaryColor.name(),
                     t.primaryColorForeground.name(),
                     t.primaryColorHovered.name()));
    }

    if (historyBar) {
        historyBar->setStyleSheet(QStringLiteral(
            "QFrame#ConsoleHistoryBar {"
            "  background-color: rgba(%1, %2, %3, 210);"
            "  border: 1px solid %4;"
            "  border-radius: 8px;"
            "}"
            "QLabel#ConsoleHistoryStatus {"
            "  color: %5; font-size: %8px; font-family: '%9';"
            "  padding: 0 4px 0 2px; min-width: 52px;"
            "}"
            "QLabel#ConsoleHistoryMuted {"
            "  color: %6; font-size: %8px; padding-right: 2px;"
            "}"
            "QFrame#ConsoleHistorySep {"
            "  background-color: %4; max-width: 1px; margin: 2px 1px; border: none;"
            "}"
            "QToolButton {"
            "  border: none; background: transparent; border-radius: 4px;"
            "  padding: 2px 6px; color: %5; font-size: %8px;"
            "}"
            "QToolButton:hover { background-color: %7; }"
            "QToolButton:disabled { color: %6; }"
            "QToolButton#HistBtnEarlier { padding-left: 3px; padding-right: 5px; }"
            "QToolButton#HistBtnJump { padding: 2px 4px; }"
        ).arg(t.backgroundColorMain3.red())
         .arg(t.backgroundColorMain3.green())
         .arg(t.backgroundColorMain3.blue())
         .arg(t.borderColor.name(),
              t.primaryColor.name(),
              t.secondaryColor.name(),
              t.backgroundColorMain4.name())
         .arg(fontPx)
         .arg(monoFamily));
    }
}

void ChatWidget::applyChatChromeMetrics()
{
    const AppTypography& ty = FontManager::instance().typography();
    const int barH = ty.historyBarHeight;
    const int btnH = ty.controlInnerH;
    const int sepH = qMax(12, btnH - 6);
    const qreal s = ty.baseSize / 10.0;

    if (searchChrome)
        searchChrome->setFixedHeight(barH);
    if (searchInput)
        searchInput->setFixedHeight(btnH);
    if (searchResultLabel)
        searchResultLabel->setFixedHeight(btnH);
    if (searchCloseBtn)
        searchCloseBtn->setFixedSize(btnH, btnH);
    if (clearChatBtn)
        clearChatBtn->setFixedHeight(btnH);
    if (hideDeletedSwitch)
        hideDeletedSwitch->setFixedSize(qMax(30, qRound(34 * s)), qMax(14, qRound(16 * s)));
    if (hideDeletedLabel)
        hideDeletedLabel->setFixedHeight(btnH);

    if (historyBar)
        historyBar->setFixedHeight(barH);
    if (pageSizeSpin)
        pageSizeSpin->setFixedHeight(btnH);
    if (autoLoadSwitch)
        autoLoadSwitch->setFixedSize(qMax(30, qRound(34 * s)), qMax(14, qRound(16 * s)));
    if (loadEarlierBtn)
        loadEarlierBtn->setFixedHeight(btnH);
    if (jumpLatestBtn) {
        jumpLatestBtn->setFixedHeight(btnH);
        jumpLatestBtn->setFixedWidth(qMax(24, btnH + 4));
    }
    if (historyBar) {
        const auto seps = historyBar->findChildren<QFrame*>(QStringLiteral("ConsoleHistorySep"));
        for (QFrame* sep : seps)
            sep->setFixedHeight(sepH);
    }
}

void ChatWidget::positionChatOverlays()
{
    if (!chatHost)
        return;

    constexpr int margin = 6;
    constexpr int gap = 4;

    int sbW = 0;
    if (messageView && messageView->verticalScrollBar() && messageView->verticalScrollBar()->isVisible())
        sbW = messageView->verticalScrollBar()->width();

    const int hostW = chatHost->width();
    const bool searchVis = searchChrome && searchChrome->isVisible();
    const bool histVis = historyBar != nullptr;

    if (searchVis) {
        searchChrome->setMaximumWidth(QWIDGETSIZE_MAX);
        searchChrome->adjustSize();
    }
    if (histVis)
        historyBar->adjustSize();

    const int histW = histVis ? historyBar->width() : 0;
    const int histH = histVis ? historyBar->height() : 0;
    const int searchW = searchVis ? searchChrome->width() : 0;

    const int needSideBySide = margin + searchW + gap + histW + margin + sbW;
    const bool stack = searchVis && histVis && needSideBySide > hostW;

    int histX = margin;
    int histY = margin;
    if (histVis) {
        histX = qMax(margin, hostW - histW - margin - sbW);
        histY = margin;
        historyBar->move(histX, histY);
        historyBar->show();
        historyBar->raise();
    }

    int searchBottom = margin;
    if (searchVis) {
        int searchX = margin;
        int searchY = margin;
        if (stack) {
            searchY = margin + histH + gap;
            const int maxSearchW = qMax(160, hostW - 2 * margin);
            if (searchChrome->width() > maxSearchW) {
                searchChrome->setMaximumWidth(maxSearchW);
                searchChrome->adjustSize();
            }
        } else {
            const int maxAlone = qMax(160, hostW - 2 * margin);
            if (searchChrome->width() > maxAlone) {
                searchChrome->setMaximumWidth(maxAlone);
                searchChrome->adjustSize();
            }
        }
        searchChrome->move(searchX, searchY);
        searchChrome->raise();
        searchBottom = searchY + searchChrome->height();
    } else if (histVis) {
        searchBottom = margin + histH;
    }

    if (deleteSelectedBtn && deleteSelectedBtn->isVisible()) {
        deleteSelectedBtn->adjustSize();
        deleteSelectedBtn->raise();
        deleteSelectedBtn->move(margin, searchBottom + gap);
    }
}

void ChatWidget::toggleSearchChrome(bool forceShow)
{
    if (!searchChrome)
        return;
    if (forceShow || !searchChrome->isVisible()) {
        searchChrome->setVisible(true);
        applyChatChromeMetrics();
        applyChatChromeStyle();
        positionChatOverlays();
        if (searchInput) {
            searchInput->setFocus();
            searchInput->selectAll();
        }
    } else {
        searchChrome->setVisible(false);
    }
}

void ChatWidget::loadHistory() {
    const int limit = pageSize;
    HttpReqChatHistoryAsync(0, limit, *(adaptixWidget->GetProfile()),
        [this, limit](bool success, const QString&, const QJsonObject& response) {
            if (!success) return;
            QJsonArray messages = response["messages"].toArray();
            QList<ChatMessage> batch;
            for (const auto& v : messages) {
                QJsonObject obj = v.toObject();
                ChatMessage msg;
                msg.id = (qint64)obj["Id"].toDouble();
                msg.username = obj["Username"].toString();
                msg.message = obj["Message"].toString();
                msg.date = (qint64)obj["Date"].toDouble();
                msg.edited = obj["Edited"].toBool();
                msg.deleted = obj["Deleted"].toBool();
                msg.deletedDate = (qint64)obj["DeletedDate"].toDouble();
                msg.reactions = parseReactions(obj["Reactions"].toString());
                msg.replyToId = (qint64)obj["ReplyToId"].toDouble();
                msg.replyToName = obj["ReplyToName"].toString();
                msg.replyToDeleted = false;
                msg.isMarkdown = true;
                batch.append(msg);
            }
            hasMore = (batch.size() >= limit);
            if (response.contains("total"))
                totalMessages = response["total"].toInt();
            updateLoadStatus();
            updateHistoryControls();
            if (!batch.isEmpty()) {
                messageModel->beginResetModel();
                messageModel->messages = batch;
                messageModel->endResetModel();
                messageModel->resolveReplyTexts();
                messageModel->refreshReplyDeleted();
                QMetaObject::invokeMethod(this, [this]() {
                    messageView->doItemsLayout();
                    messageView->scrollToBottom();
                    atBottom = true;
                }, Qt::QueuedConnection);
            }
        }
    );
}

void ChatWidget::loadMore() {
    if (loadingMore || !hasMore) return;
    qint64 beforeId = messageModel->oldestId();
    if (beforeId <= 1) return;

    loadingMore = true;
    updateHistoryControls();

    QScrollBar* vbar = messageView->verticalScrollBar();
    int oldScrollMax = vbar->maximum();
    const int limit = pageSize;

    HttpReqChatHistoryAsync(beforeId, limit, *(adaptixWidget->GetProfile()),
        [this, oldScrollMax, limit](bool success, const QString&, const QJsonObject& response) {
            loadingMore = false;

            if (!success) {
                updateHistoryControls();
                return;
            }
            QJsonArray messages = response["messages"].toArray();
            if (messages.isEmpty()) {
                hasMore = false;
                updateLoadStatus();
                updateHistoryControls();
                return;
            }
            QList<ChatMessage> batch;
            for (const auto& v : messages) {
                QJsonObject obj = v.toObject();
                ChatMessage msg;
                msg.id = (qint64)obj["Id"].toDouble();
                msg.username = obj["Username"].toString();
                msg.message = obj["Message"].toString();
                msg.date = (qint64)obj["Date"].toDouble();
                msg.edited = obj["Edited"].toBool();
                msg.deleted = obj["Deleted"].toBool();
                msg.deletedDate = (qint64)obj["DeletedDate"].toDouble();
                msg.reactions = parseReactions(obj["Reactions"].toString());
                msg.replyToId = (qint64)obj["ReplyToId"].toDouble();
                msg.replyToName = obj["ReplyToName"].toString();
                msg.replyToDeleted = false;
                msg.isMarkdown = true;
                batch.append(msg);
            }
            if (batch.size() < limit)
                hasMore = false;
            if (response.contains("total"))
                totalMessages = response["total"].toInt();
            if (!batch.isEmpty()) {
                messageModel->prependMessages(batch);
                QScrollBar* vbar2 = messageView->verticalScrollBar();
                vbar2->setValue(vbar2->maximum() - oldScrollMax);
            }
            updateLoadStatus();
            updateHistoryControls();
        }
    );
}

void ChatWidget::updateHistoryControls()
{
    if (loadEarlierBtn) {
        loadEarlierBtn->setEnabled(hasMore && !loadingMore);
        loadEarlierBtn->setText(loadingMore ? QStringLiteral("… Loading") : QStringLiteral("▲ Earlier"));
        loadEarlierBtn->setVisible(true);
    }
    if (pageSizeSpin)
        pageSizeSpin->setEnabled(!loadingMore);
    if (jumpLatestBtn)
        jumpLatestBtn->setEnabled(messageModel && messageModel->rowCount() > 0);
}

void ChatWidget::updateLoadStatus() {
    int count = messageModel ? messageModel->rowCount() : 0;
    if (!loadStatusLabel)
        return;
    if (totalMessages > 0) {
        loadStatusLabel->setText(QStringLiteral("%1/%2").arg(count).arg(totalMessages));
        loadStatusLabel->setToolTip(QStringLiteral("%1 of %2 messages loaded").arg(count).arg(totalMessages));
    } else if (count > 0) {
        loadStatusLabel->setText(QStringLiteral("%1").arg(count));
        loadStatusLabel->setToolTip(QStringLiteral("%1 messages loaded").arg(count));
    } else {
        loadStatusLabel->setText(QStringLiteral("—"));
        loadStatusLabel->setToolTip(QStringLiteral("No messages loaded"));
    }
}

void ChatWidget::scrollToBottomIfNeeded() {
    if (atBottom) {
        QMetaObject::invokeMethod(this, [this]() {
            messageView->scrollToBottom();
        }, Qt::QueuedConnection);
    }
}

void ChatWidget::handleSend() {
    QString text = chatInput->toPlainText().trimmed();
    if (text.isEmpty()) return;
    if (text.size() > 4096) {
        MessageError("Message too long (max 4096 chars)");
        return;
    }

    if (editMode) {
        HttpReqChatEditMessageAsync(editId, text, *(adaptixWidget->GetProfile()),
            [this](bool success, const QString& msg, const QJsonObject&) {
                if (!success) MessageError(msg.isEmpty() ? "Edit failed" : msg);
            }
        );
        exitEditMode();
    } else {
        qint64 sendReplyId = replyToId;
        QString sendReplyName = replyToName;
        HttpReqChatSendMessageAsync(text, sendReplyId, sendReplyName, *(adaptixWidget->GetProfile()),
            [this](bool success, const QString& msg, const QJsonObject&) {
                if (!success) MessageError(msg.isEmpty() ? "Send failed" : msg);
            }
        );
        chatInput->clear();
        exitReplyMode();
        atBottom = true;
        QMetaObject::invokeMethod(this, [this]() {
            messageView->scrollToBottom();
        }, Qt::QueuedConnection);
    }
}

void ChatWidget::handleSearch() {
    QString query = searchInput->text().trimmed();
    if (query.isEmpty()) {
        if (searchResultLabel)
            searchResultLabel->setText(QStringLiteral("0"));
        messageModel->clear();
        loadHistory();
        return;
    }
    HttpReqChatSearchAsync(query, 0, 100, *(adaptixWidget->GetProfile()),
        [this](bool success, const QString&, const QJsonObject& response) {
            if (!success) return;
            QJsonArray messages = response["messages"].toArray();
            messageModel->beginResetModel();
            messageModel->messages.clear();
            for (const auto& v : messages) {
                QJsonObject obj = v.toObject();
                ChatMessage msg;
                msg.id = (qint64)obj["Id"].toDouble();
                msg.username = obj["Username"].toString();
                msg.message = obj["Message"].toString();
                msg.date = (qint64)obj["Date"].toDouble();
                msg.edited = obj["Edited"].toBool();
                msg.deleted = obj["Deleted"].toBool();
                msg.deletedDate = (qint64)obj["DeletedDate"].toDouble();
                msg.reactions = parseReactions(obj["Reactions"].toString());
                msg.replyToId = (qint64)obj["ReplyToId"].toDouble();
                msg.replyToName = obj["ReplyToName"].toString();
                msg.replyToDeleted = false;
                msg.isMarkdown = true;
                messageModel->messages.append(msg);
            }
            messageModel->endResetModel();
            if (searchResultLabel)
                searchResultLabel->setText(QString::number(messageModel->messages.size()));
            positionChatOverlays();
        }
    );
}

void ChatWidget::handleReaction(qint64 id, const QString& emoji) {
    HttpReqChatReactionAsync(id, emoji, *(adaptixWidget->GetProfile()),
        [](bool, const QString&, const QJsonObject&) {}
    );
}

void ChatWidget::enterEditMode(qint64 id) {
    ChatMessage msg = messageModel->getMessageById(id);
    if (msg.id == 0) return;
    editMode = true;
    editId = id;
    chatInput->setPlainText(msg.message);
    chatInput->setFocus();
}

void ChatWidget::exitEditMode() {
    editMode = false;
    editId = 0;
    chatInput->clear();
}

void ChatWidget::enterReplyMode(qint64 id, const QString& username) {
    ChatMessage msg = messageModel->getMessageById(id);
    if (msg.id == 0) return;
    exitEditMode();
    replyToId = id;
    replyToName = username;
    QString preview = msg.message;
    preview.replace('\n', ' ');
    if (preview.length() > 80) preview = preview.left(80) + "...";
    replyLabel->setTextFormat(Qt::RichText);
    replyLabel->setText(QString("<b>%1</b>: %2").arg(username.toHtmlEscaped(), preview.toHtmlEscaped()));
    replyPreview->setVisible(true);
    chatInput->setFocus();
}

void ChatWidget::exitReplyMode() {
    replyToId = 0;
    replyToName.clear();
    replyPreview->setVisible(false);
    replyLabel->clear();
}

void ChatWidget::sendToTodo(const QString& text) {
    QString current = todoWidget ? todoWidget->content() : QString();
    QString merged;
    if (current.trimmed().isEmpty()) {
        merged = text;
    } else {
        merged = current;
        while (merged.endsWith(QLatin1Char('\n')) || merged.endsWith(QLatin1Char('\r')) || merged.endsWith(QLatin1Char(' ')) || merged.endsWith(QLatin1Char('\t')))
            merged.chop(1);
        merged += QStringLiteral("\n\n") + text;
    }

    HttpReqChatUpdateTodoAsync(merged, *(adaptixWidget->GetProfile()),
        [this, merged](bool success, const QString&, const QJsonObject&) {
            if (!success) {
                MessageError("Failed to send to ToDo");
                return;
            }
            todoWidget->SetTodo(merged, adaptixWidget->GetProfile()->GetUsername(), QDateTime::currentSecsSinceEpoch());
        }
    );
}

void ChatWidget::onScrollChanged() {
    QScrollBar* vbar = messageView->verticalScrollBar();
    atBottom = (vbar->value() >= vbar->maximum() - 10);
    if (autoLoadEarlier && !loadingMore && hasMore && vbar->value() <= 8)
        loadMore();
}

void ChatWidget::AddChatMessage(qint64 id, const QString& username, const QString& message, qint64 date, bool edited, bool deleted, const QString& reactions, qint64 replyToId, const QString& replyToName) {
    ChatMessage msg;
    msg.id = id;
    msg.username = username;
    msg.message = message;
    msg.date = date;
    msg.edited = edited;
    msg.deleted = deleted;
    msg.deletedDate = 0;
    msg.reactions = parseReactions(reactions);
    msg.replyToId = replyToId;
    msg.replyToName = replyToName;
    msg.replyToDeleted = false;
    msg.replyToText = "";
    msg.isMarkdown = markdownMode;
    if (replyToId > 0) {
        ChatMessage orig = messageModel->getMessageById(replyToId);
        if (orig.id > 0) {
            QString t = orig.message;
            t.replace('\n', ' ');
            if (t.length() > 60) t = t.left(60) + "...";
            msg.replyToText = t;
        }
    }
    messageModel->addMessage(msg);
    messageModel->refreshReplyDeleted();
    updateLoadStatus();
    if (!presyncing) {
        scrollToBottomIfNeeded();
        auto* coreDw = dockWidget ? dockWidget->dockWidget() : nullptr;
        const bool viewingChat = coreDw && coreDw->isOpen() && coreDw->isCurrentTab();
        if (!viewingChat) {
            if (adaptixWidget && adaptixWidget->IsSynchronized())
                adaptixWidget->ChatUnreadIncrement();
            blinkNewContent();
        }
    }
}

void ChatWidget::FinishPresync() {
    presyncing = false;
    hasMore = (messageModel->rowCount() >= pageSize);
    messageModel->resolveReplyTexts();
    messageModel->refreshReplyDeleted();
    updateLoadStatus();
    updateHistoryControls();
    QMetaObject::invokeMethod(this, [this]() {
        messageView->doItemsLayout();
        messageView->scrollToBottom();
        atBottom = true;
    }, Qt::QueuedConnection);
}

void ChatWidget::EditChatMessage(qint64 id, const QString& text) {
    messageModel->editMessage(id, text);
    messageDelegate->invalidateCache(id);
    QScrollBar* vbar = messageView->verticalScrollBar();
    int scrollPos = vbar->value();
    messageModel->beginResetModel();
    messageModel->endResetModel();
    vbar->setValue(scrollPos);
}

void ChatWidget::DeleteChatMessage(qint64 id) {
    messageModel->deleteMessage(id);
    messageDelegate->invalidateCache(id);
    QScrollBar* vbar = messageView->verticalScrollBar();
    int scrollPos = vbar->value();
    messageModel->beginResetModel();
    messageModel->endResetModel();
    vbar->setValue(scrollPos);
    if (hideDeleted) {
        for (int i = 0; i < messageModel->rowCount(); ++i) {
            QModelIndex idx = messageModel->index(i);
            if (idx.data(ChatMessageModel::IdRole).toLongLong() == id) {
                messageView->setRowHidden(i, true);
                break;
            }
        }
    }
}

void ChatWidget::SetTodo(const QString& content, const QString& updatedBy, qint64 updatedAt) {
    todoWidget->SetTodo(content, updatedBy, updatedAt);
}

void ChatWidget::showEvent(QShowEvent* event)
{
    DockTab::showEvent(event);
    seedSplitterSizes();
    applyChatChromeMetrics();
    applyChatChromeStyle();
    positionChatOverlays();
}

void ChatWidget::Clear() {
    presyncing = true;
    messageModel->clear();
    todoWidget->SetTodo("", "", 0);
    hasMore = true;
    updateLoadStatus();
    updateHistoryControls();
}
