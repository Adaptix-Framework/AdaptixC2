#include <UI/Widgets/ChatWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Utils/Convert.h>
#include <Utils/FontManager.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <oclero/qlementine/style/QlementineStyle.hpp>

#include <QScrollBar>
#include <QMenu>
#include <QTextDocument>
#include <QPainter>
#include <QMouseEvent>
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
    QDateTime dt = QDateTime::fromSecsSinceEpoch(unixTs);
    QDateTime now = QDateTime::currentDateTime();
    if (dt.date() == now.date())
        return dt.toString("HH:mm");
    return dt.toString("dd.MM HH:mm");
}

static QString formatDate(qint64 unixTs) {
    QDateTime dt = QDateTime::fromSecsSinceEpoch(unixTs);
    return dt.toString("dd.MM.yyyy HH:mm");
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
        auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style());
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
        c.selectionBg     = t.backgroundColorMain4;
        c.flashBorder     = t.statusColorWarning;
        return c;
    }
};

static const ChatColors& chatColors() {
    static ChatColors cached;
    static oclero::qlementine::QlementineStyle* lastStyle = nullptr;
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style());
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

    if (option.state & QStyle::State_Selected) {
        const auto& cc = chatColors();
        painter->fillRect(option.rect, cc.selectionBg);
    }

    bool isMine = index.data(ChatMessageModel::IsMineRole).toBool();
    bool deleted = index.data(ChatMessageModel::DeletedRole).toBool();
    bool edited = index.data(ChatMessageModel::EditedRole).toBool();
    QString username = index.data(ChatMessageModel::UsernameRole).toString();
    qint64 date = index.data(ChatMessageModel::DateRole).toLongLong();
    QList<ChatReaction> reactions = index.data(ChatMessageModel::ReactionsRole).value<QList<ChatReaction>>();

    QRect r = option.rect;
    const ChatLayoutMetrics lm = chatLayout();
    int avatarSize = lm.avatarSize;
    int bubbleMaxWidth = qMin(r.width() * 70 / 100, 600);
    int hPad = lm.hPad;
    int vPad = lm.vPad;

    const auto& cc = chatColors();
    QColor bubbleColor = isMine ? cc.bubbleMine : cc.bubbleOther;
    QColor textColor = deleted ? cc.textDisabled : cc.text;
    QColor timeColor = cc.textSecondary;
    QColor avatarBg = isMine ? cc.avatarMine : cc.avatarOther;

    int avatarX, bubbleX;
    if (isMine) {
        avatarX = r.right() - avatarSize - hPad;
        bubbleX = avatarX - bubbleMaxWidth - 8;
    } else {
        avatarX = r.left() + hPad;
        bubbleX = avatarX + avatarSize + 8;
    }
    int bubbleWidth = bubbleMaxWidth - 24;
    int replyH = 0;
    qint64 replyToId = index.data(ChatMessageModel::ReplyToIdRole).toLongLong();
    QString replyToName = index.data(ChatMessageModel::ReplyToNameRole).toString();
    QString replyToText = index.data(ChatMessageModel::ReplyToTextRole).toString();
    bool replyToDeleted = index.data(ChatMessageModel::ReplyToDeletedRole).toBool();
    if (replyToId > 0)
        replyH = lm.replyH;

    QTextDocument* doc = getDocument(index, bubbleWidth);
    int textH = static_cast<int>(realDocHeight(doc));
    int bubbleH = textH + 2 * vPad + replyH;

    int y = r.top() + 4;

    QRect avatarRect(avatarX, y, avatarSize, avatarSize);
    painter->setBrush(avatarBg);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(avatarRect, 8, 8);
    painter->setPen(Qt::white);
    QFont avatarFont = chatFont(4, QFont::Bold);
    painter->setFont(avatarFont);
    painter->drawText(avatarRect, Qt::AlignCenter, username.left(1).toUpper());

    int nameH = lm.nameH;
    if (!isMine) {
        QRect nameRect(bubbleX, y, bubbleWidth, nameH);
        painter->setPen(cc.nameColor);
        QFont nameFont = chatFont(-1, QFont::Bold);
        painter->setFont(nameFont);
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, username);
    }
    int nameOffset = isMine ? 0 : nameH + 2;

    QRect bubbleRect(bubbleX, y + nameOffset, bubbleWidth + 16, bubbleH);
    painter->setBrush(bubbleColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(bubbleRect, 10, 10);

    bool isFlashed = index.data(ChatMessageModel::FlashRole).toBool();
    if (isFlashed) {
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
        QFont replyFont = chatFont(-2);
        painter->setFont(replyFont);
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

    QRect timeRect;
    if (isMine) {
        timeRect = QRect(bubbleX - 65, bubbleRect.bottom() + 1, 63, lm.timeH);
    } else {
        timeRect = QRect(bubbleRect.right() + 4, bubbleRect.bottom() + 1, 63, lm.timeH);
    }
    painter->setPen(timeColor);
    QFont timeFont = chatFont(-2);
    painter->setFont(timeFont);
    QString timeStr = formatTime(date);
    if (edited) timeStr += " ✏";
    painter->drawText(timeRect, Qt::AlignRight | Qt::AlignVCenter, timeStr);

    int reactionsY = bubbleRect.bottom() + 2;
    if (!reactions.isEmpty()) {
        int rx = bubbleX;
        QFont reactFont = chatFont(-1);
        painter->setFont(reactFont);
        for (const auto& react : reactions) {
            QString label = react.emoji + " " + QString::number(react.users.size());
            int w = painter->fontMetrics().horizontalAdvance(label) + 12;
            QRect chipRect(rx, reactionsY, w, lm.reactionH);
            bool iReacted = react.users.contains(currentUser);
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
    bool isMine = index.data(ChatMessageModel::IsMineRole).toBool();
    QList<ChatReaction> reactions = index.data(ChatMessageModel::ReactionsRole).value<QList<ChatReaction>>();

    int viewWidth = option.rect.width();
    bool widthUnreliable = (viewWidth < 300);
    if (widthUnreliable) viewWidth = 600;
    int bubbleMaxWidth = qMin(viewWidth * 70 / 100, 600);
    int bubbleWidth = bubbleMaxWidth - 24;

    const ChatLayoutMetrics lm = chatLayout();
    int nameH = isMine ? 0 : lm.nameH + 2;
    int replyH = 0;
    qint64 rtId = index.data(ChatMessageModel::ReplyToIdRole).toLongLong();
    if (rtId > 0) replyH = lm.replyH;
    int totalH;
    if (widthUnreliable) {
        QFontMetrics fm(chatFont(0));
        int lineH = fm.height();
        int charsPerLine = qMax(1, bubbleWidth / fm.averageCharWidth());
        QString msg = index.data(ChatMessageModel::MessageRole).toString();
        int lines = 0;
        for (const QString& line : msg.split('\n'))
            lines += qMax(1, (line.length() + charsPerLine - 1) / charsPerLine);
        totalH = 4 + nameH + replyH + lines * lineH + 12 + lm.timeH + 14;
    } else {
        std::unique_ptr<QTextDocument> doc(getDocument(index, bubbleWidth));
        qreal textH = doc->size().height();
        totalH = 4 + nameH + replyH + static_cast<int>(textH) + 12 + lm.timeH + 14;
    }
    if (!reactions.isEmpty()) totalH += lm.reactionH + 4;
    return QSize(viewWidth, totalH);
}

bool ChatMessageDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            const ChatLayoutMetrics lm = chatLayout();
            qint64 replyToId = index.data(ChatMessageModel::ReplyToIdRole).toLongLong();
            if (replyToId > 0) {
                bool isMine = index.data(ChatMessageModel::IsMineRole).toBool();
                int nameH = isMine ? 0 : lm.nameH + 2;
                int avatarX = isMine ? option.rect.right() - lm.avatarSize - lm.hPad : option.rect.left() + lm.hPad;
                int bubbleX = isMine ? avatarX - qMin(option.rect.width() * 70 / 100, 600) : avatarX + lm.avatarSize + 8;
                int replyTop = option.rect.top() + 4 + nameH + 4;
                QRect replyClickRect(bubbleX, replyTop, qMin(option.rect.width() * 70 / 100, 600) - 24 + 16, lm.replyH - 4);
                if (replyClickRect.contains(me->pos())) {
                    Q_EMIT replyClicked(replyToId);
                    return true;
                }
            }

            QList<ChatReaction> reactions = index.data(ChatMessageModel::ReactionsRole).value<QList<ChatReaction>>();
            if (!reactions.isEmpty()) {
                bool isMine = index.data(ChatMessageModel::IsMineRole).toBool();
                int bubbleMaxWidth = qMin(option.rect.width() * 70 / 100, 600);

                int rx = isMine
                    ? option.rect.right() - lm.avatarSize - bubbleMaxWidth - lm.hPad
                    : option.rect.left() + lm.avatarSize + lm.hPad * 2;

                int reactionsY = option.rect.bottom() - lm.reactionH - 2;
                QFont reactFont = chatFont(-1);
                QFontMetrics fm(reactFont);
                for (const auto& react : reactions) {
                    QString label = react.emoji + " " + QString::number(react.users.size());
                    int w = fm.horizontalAdvance(label) + 12;
                    QRect chipRect(rx, reactionsY, w, lm.reactionH);
                    if (chipRect.contains(me->pos())) {
                        qint64 id = index.data(ChatMessageModel::IdRole).toLongLong();
                        Q_EMIT reactionClicked(id, react.emoji);
                        return true;
                    }
                    rx += w + 4;
                }
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

static QRect reactionChipRect(const QStyleOptionViewItem& option, const QModelIndex& index, int reactionIndex) {
    const ChatLayoutMetrics lm = chatLayout();
    bool isMine = index.data(ChatMessageModel::IsMineRole).toBool();
    int bubbleMaxWidth = qMin(option.rect.width() * 70 / 100, 600);

    int rx = isMine
        ? option.rect.right() - lm.avatarSize - bubbleMaxWidth - lm.hPad
        : option.rect.left() + lm.avatarSize + lm.hPad * 2;

    int reactionsY = option.rect.bottom() - lm.reactionH - 2;
    QList<ChatReaction> reactions = index.data(ChatMessageModel::ReactionsRole).value<QList<ChatReaction>>();
    QFont reactFont = chatFont(-1);
    QFontMetrics fm(reactFont);
    for (int i = 0; i < reactions.size(); ++i) {
        const auto& react = reactions[i];
        QString label = react.emoji + " " + QString::number(react.users.size());
        int w = fm.horizontalAdvance(label) + 12;
        QRect chipRect(rx, reactionsY, w, lm.reactionH);
        if (i == reactionIndex) return chipRect;
        rx += w + 4;
    }
    return {};
}

bool ChatMessageDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() == QEvent::ToolTip) {
        QList<ChatReaction> reactions = index.data(ChatMessageModel::ReactionsRole).value<QList<ChatReaction>>();
        if (reactions.isEmpty()) return false;

        const ChatLayoutMetrics lm = chatLayout();
        bool isMine = index.data(ChatMessageModel::IsMineRole).toBool();
        int bubbleMaxWidth = qMin(option.rect.width() * 70 / 100, 600);
        int bubbleWidth = bubbleMaxWidth - 24;
        int nameH = isMine ? 0 : lm.nameH + 2;
        int replyH = 0;
        qint64 rtId = index.data(ChatMessageModel::ReplyToIdRole).toLongLong();
        if (rtId > 0) replyH = lm.replyH;
        int vPad = lm.vPad;
        int bubbleX = isMine
            ? option.rect.right() - lm.avatarSize - bubbleMaxWidth
            : option.rect.left() + lm.avatarSize + lm.hPad;
        int textH = 30;
        QRect bubbleRect(bubbleX, option.rect.top() + 4 + nameH, bubbleWidth + 16, textH + 2 * vPad + replyH);
        int reactionsY = bubbleRect.bottom() + 2;

        int rx = bubbleX;
        QFont reactFont = chatFont(-1);
        QFontMetrics fm(reactFont);
        QPoint pos = event->pos();
        for (const auto& react : reactions) {
            QString label = react.emoji + " " + QString::number(react.users.size());
            int w = fm.horizontalAdvance(label) + 12;
            QRect chipRect(rx, reactionsY, w, lm.reactionH);
            if (chipRect.contains(pos)) {
                QToolTip::showText(event->globalPos(), react.users.join("\n"));
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
    // Rendered view (default)
    renderedView = new QTextBrowser(this);
    renderedView->setOpenExternalLinks(true);
    renderedView->setPlaceholderText("Shared ToDo — click Edit to start");

    editor = new QPlainTextEdit(this);
    editor->setPlaceholderText("Markdown...");

    livePreview = new QTextBrowser(this);

    auto* editSplitter = new QSplitter(Qt::Vertical, this);
    editSplitter->addWidget(editor);
    editSplitter->addWidget(livePreview);
    editSplitter->setStretchFactor(0, 3);
    editSplitter->setStretchFactor(1, 1);

    contentStack = new QStackedWidget(this);
    contentStack->addWidget(renderedView);
    contentStack->addWidget(editSplitter);

    // Toolbar (edit mode only)
    toolbarRow = new QFrame(this);
    auto* toolbarLayout = new QHBoxLayout(toolbarRow);
    toolbarLayout->setContentsMargins(4, 2, 4, 2);
    toolbarLayout->setSpacing(4);

    auto makeToolBtn = [&](const QString& text, const QString& tooltip, std::function<void()> fn) {
        auto* btn = new QToolButton(this);
        btn->setText(text);
        btn->setToolTip(tooltip);
        btn->setFixedHeight(FontManager::instance().typography().controlInnerH);
        btn->setMinimumWidth(28);
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

    editBtn = new QPushButton("Edit", this);
    connect(editBtn, &QPushButton::clicked, this, [this]() { setEditMode(true); });

    saveBtn = new QPushButton("Save", this);
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        Q_EMIT saveRequested(editor->toPlainText());
        setEditMode(false);
    });

    cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, [this]() { setEditMode(false); });

    statusLabel = new QLabel("", this);

    auto* wrapSwitch = new oclero::qlementine::Switch(this);
    wrapSwitch->setFixedSize(36, 18);
    wrapSwitch->setChecked(true);
    wrapSwitch->setToolTip("Word wrap");
    auto* wrapLabel = new QLabel("Wrap", this);
    connect(wrapSwitch, &oclero::qlementine::Switch::checkStateChanged, this, [this](int state) {
        bool wrap = (state == Qt::Checked);
        editor->setWordWrapMode(wrap ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
        renderedView->setWordWrapMode(wrap ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
    });

    auto* footerBar = new QHBoxLayout();
    footerBar->setContentsMargins(4, 2, 4, 2);
    footerBar->setSpacing(6);
    footerBar->addWidget(editBtn);
    footerBar->addWidget(saveBtn);
    footerBar->addWidget(cancelBtn);
    footerBar->addWidget(statusLabel, 1);
    footerBar->addWidget(wrapSwitch);
    footerBar->addWidget(wrapLabel);
    saveBtn->setVisible(false);
    cancelBtn->setVisible(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(toolbarRow);
    layout->addWidget(contentStack, 1);
    layout->addLayout(footerBar);

    debounceTimer = new QTimer(this);
    debounceTimer->setSingleShot(true);
    debounceTimer->setInterval(500);
    connect(editor, &QPlainTextEdit::textChanged, this, [this]() {
        if (!updating) debounceTimer->start();
    });
    connect(debounceTimer, &QTimer::timeout, this, &TodoWidget::updatePreview);

    applyFonts();
    connect(&FontManager::instance(), &FontManager::typographyChanged, this, &TodoWidget::applyFonts);
}

void TodoWidget::applyFonts()
{
    const AppTypography& ty = FontManager::instance().typography();
    const QFont body = ty.regular;
    const QFont mono = ty.mono;

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
        if (text.isEmpty())
            text = renderedView->toPlainText();
        if (!text.isEmpty()) {
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
    toolbarRow->setVisible(on);
    editBtn->setVisible(!on);
    saveBtn->setVisible(on);
    cancelBtn->setVisible(on);
    if (on) {
        editor->setFocus();
        updatePreview();
    } else {
        renderedView->setMarkdown(editor->toPlainText());
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
    QString fixed = content;
    fixed.replace(QRegularExpression("(?<!\\n)\\n(?!\\n)"), "  \n");
    QTextDocument doc;
    doc.setDefaultFont(FontManager::instance().typography().regular);
    doc.setMarkdown(fixed);
    renderedView->setHtml(doc.toHtml());

    if (editMode) {
        statusLabel->setText(QString("Changed by %1 — press Save to overwrite").arg(updatedBy));
        return;
    }
    updating = true;
    editor->setPlainText(content);
    updating = false;
    if (updatedAt > 0) {
        statusLabel->setText(QString("Last updated by %1 at %2").arg(updatedBy, formatDate(updatedAt)));
    }
}

ChatWidget::ChatWidget(AdaptixWidget* w) : DockTab("Chat", w->GetProfile()->GetProject(), ":/icons/chat"), adaptixWidget(w)
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
        searchBar->setVisible(!searchBar->isVisible());
        if (searchBar->isVisible()) searchInput->setFocus();
    });

    auto* shortcutEsc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    shortcutEsc->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcutEsc, &QShortcut::activated, this, [this]() {
        exitEditMode();
        exitReplyMode();
    });

    chatInput->installEventFilter(this);

    connect(todoWidget, &TodoWidget::saveRequested, this, [this](const QString& content) {
        HttpReqChatUpdateTodoAsync(content, *(adaptixWidget->GetProfile()), [](bool, const QString&, const QJsonObject&) {});
    });

    connect(messageView->verticalScrollBar(), &QScrollBar::valueChanged, this, &ChatWidget::onScrollChanged);

    messageView->viewport()->installEventFilter(this);

    connect(adaptixWidget, &AdaptixWidget::SyncedSignal, this, [this]() {
        FinishPresync();
    });

    connect(&FontManager::instance(), &FontManager::typographyChanged, this, [this]() {
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
    });

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
    if (obj == messageView->viewport() && event->type() == QEvent::Resize) {
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
        auto* reactMenu = menu.addMenu("Add reaction");
        for (const auto& emoji : EMOJI_LIST) {
            connect(reactMenu->addAction(emoji), &QAction::triggered, this, [this, id, emoji]() {
                handleReaction(id, emoji);
            });
        }
        if (!deleted) {
            menu.addSeparator();
            if (isMine) {
                connect(menu.addAction("Edit"), &QAction::triggered, this, [this, id]() { enterEditMode(id); });
                connect(menu.addAction("Delete"), &QAction::triggered, this, [this, id]() {
                    HttpReqChatDeleteMessageAsync(id, *(adaptixWidget->GetProfile()), [](bool, const QString&, const QJsonObject&) {});
                });
            }
            menu.addSeparator();
            connect(menu.addAction("Reply"), &QAction::triggered, this, [this, id, username]() {
                enterReplyMode(id, username);
            });
            connect(menu.addAction("Send to ToDo"), &QAction::triggered, this, [this, msgText, username]() {
                sendToTodo("**" + username + "**: " + msgText);
            });
            menu.addSeparator();
            connect(menu.addAction("Copy text"), &QAction::triggered, this, [msgText]() {
                QApplication::clipboard()->setText(msgText);
            });
            connect(menu.addAction("Working text"), &QAction::triggered, this, [msgText, this]() {
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

    searchBar = new QFrame(this);
    searchInput = new QLineEdit(searchBar);
    searchInput->setPlaceholderText("Search messages...");
    searchBtn = new QPushButton("Search", searchBar);
    searchBtn->setFixedWidth(60);
    searchBtn->setVisible(false);
    searchResultLabel = new QLabel("", searchBar);
    auto* searchLayout = new QHBoxLayout(searchBar);
    searchLayout->setContentsMargins(4, 2, 4, 2);
    searchLayout->addWidget(searchInput, 1);
    searchLayout->addWidget(searchResultLabel);
    searchBar->setVisible(false);

    loadEarlierBtn = new QPushButton("▲ Load earlier messages", this);
    loadEarlierBtn->setStyleSheet("QPushButton { background: transparent; color: #7FA3C0; border: none; padding: 4px; } QPushButton:hover { color: #a0c3e0; }");
    loadEarlierBtn->setCursor(Qt::PointingHandCursor);
    loadEarlierBtn->setVisible(false);
    connect(loadEarlierBtn, &QPushButton::clicked, this, &ChatWidget::loadMore);

    loadStatusLabel = new QLabel("", this);
    loadStatusLabel->setAlignment(Qt::AlignCenter);

    clearChatBtn = new QPushButton("Clear chat", this);
    clearChatBtn->setCursor(Qt::PointingHandCursor);
    clearChatBtn->setMinimumWidth(80);
    connect(clearChatBtn, &QPushButton::clicked, this, [this]() {
        auto reply = QMessageBox::question(this, "Clear chat", "Delete all chat messages permanently?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        HttpReqChatClearAsync(*(adaptixWidget->GetProfile()), [this](bool success, const QString&, const QJsonObject&) {
            if (success) {
                messageModel->clear();
                updateLoadStatus();
                hasMore = false;
                loadEarlierBtn->setVisible(false);
            }
        });
    });

    hideDeletedSwitch = new oclero::qlementine::Switch(this);
    hideDeletedSwitch->setFixedSize(36, 18);
    hideDeletedSwitch->setToolTip("Hide deleted messages");
    connect(hideDeletedSwitch, &oclero::qlementine::Switch::checkStateChanged, this, [this](int state) {
        hideDeleted = (state == Qt::Checked);
        for (int i = 0; i < messageModel->rowCount(); ++i) {
            QModelIndex idx = messageModel->index(i);
            bool isDeleted = idx.data(ChatMessageModel::DeletedRole).toBool();
            messageView->setRowHidden(i, hideDeleted && isDeleted);
        }
    });

    auto* hideDeletedLabel = new QLabel("Hide deleted", this);
    auto* hdRow = new QHBoxLayout();
    hdRow->setContentsMargins(0, 0, 0, 0);
    hdRow->setSpacing(4);
    hdRow->addWidget(hideDeletedSwitch);
    hdRow->addWidget(hideDeletedLabel);

    deleteSelectedBtn = new QPushButton("Delete selected", this);
    deleteSelectedBtn->setCursor(Qt::PointingHandCursor);
    deleteSelectedBtn->setMinimumWidth(110);
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
        int selectedCount = messageView->selectionModel()->selectedIndexes().size();
        deleteSelectedBtn->setVisible(selectedCount > 1);
    });

    chatInput = new QPlainTextEdit(this);
    chatInput->setFixedHeight(chatLayout().inputH);
    chatInput->setFont(FontManager::instance().appRegularFont());
    chatInput->setPlaceholderText("Type a message... (Enter to send, Markdown supported)");

    mdPreview = new QTextBrowser(this);
    mdPreview->setMinimumHeight(24);
    mdPreview->setMaximumHeight(80);
    mdPreview->setVisible(false);

    auto* sendBtn = new QPushButton("Send", this);
    sendBtn->setMinimumHeight(26);
    sendBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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

    auto* mdRow = new QHBoxLayout();
    mdRow->setContentsMargins(0, 0, 0, 0);
    mdRow->setSpacing(4);
    mdRow->addWidget(mdSwitch);
    mdRow->addWidget(mdLabel);

    auto* rightPanel = new QVBoxLayout();
    rightPanel->setContentsMargins(0, 0, 0, 0);
    rightPanel->setSpacing(2);
    rightPanel->addWidget(sendBtn);
    rightPanel->addLayout(mdRow);
    rightPanel->addStretch(1);

    auto* inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(4, 2, 4, 2);
    inputLayout->addWidget(chatInput, 1);
    inputLayout->addLayout(rightPanel);
    inputLayout->setStretch(0, 1);
    inputLayout->setStretch(1, 0);

    auto* chatArea = new QVBoxLayout();
    chatArea->setContentsMargins(0, 0, 0, 0);
    chatArea->setSpacing(2);
    chatArea->addWidget(searchBar);

    auto* statusLayout = new QHBoxLayout();
    statusLayout->setContentsMargins(4, 0, 4, 0);
    statusLayout->setSpacing(6);
    statusLayout->addWidget(loadEarlierBtn, 0);
    statusLayout->addWidget(loadStatusLabel, 1);
    statusLayout->addLayout(hdRow);
    statusLayout->addWidget(deleteSelectedBtn, 0);
    statusLayout->addWidget(clearChatBtn, 0);
    chatArea->addLayout(statusLayout);
    chatArea->addWidget(messageView, 1);

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
    chatArea->addWidget(mdPreview);
    chatArea->addLayout(inputLayout);

    auto* chatFrame = new QFrame(this);
    chatFrame->setLayout(chatArea);

    todoWidget = new TodoWidget(this);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(chatFrame);
    splitter->addWidget(todoWidget);
    splitter->setSizes({500, 500});

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(splitter);
}

void ChatWidget::loadHistory() {
    HttpReqChatHistoryAsync(0, 200, *(adaptixWidget->GetProfile()),
        [this](bool success, const QString&, const QJsonObject& response) {
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
            hasMore = (batch.size() >= 200);
            if (response.contains("total"))
                totalMessages = response["total"].toInt();
            loadEarlierBtn->setVisible(hasMore);
            updateLoadStatus();
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
    loadEarlierBtn->setEnabled(false);
    loadEarlierBtn->setText("Loading...");

    QScrollBar* vbar = messageView->verticalScrollBar();
    int oldScrollMax = vbar->maximum();

    HttpReqChatHistoryAsync(beforeId, 50, *(adaptixWidget->GetProfile()),
        [this, oldScrollMax](bool success, const QString&, const QJsonObject& response) {
            loadingMore = false;
            loadEarlierBtn->setEnabled(true);
            loadEarlierBtn->setText("▲ Load earlier");

            if (!success) return;
            QJsonArray messages = response["messages"].toArray();
            if (messages.isEmpty()) {
                hasMore = false;
                loadEarlierBtn->setVisible(false);
                updateLoadStatus();
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
            if (batch.size() < 50) {
                hasMore = false;
                loadEarlierBtn->setVisible(false);
            }
            if (!batch.isEmpty()) {
                messageModel->prependMessages(batch);
                QScrollBar* vbar2 = messageView->verticalScrollBar();
                vbar2->setValue(vbar2->maximum() - oldScrollMax);
            }
            updateLoadStatus();
        }
    );
}

void ChatWidget::updateLoadStatus() {
    int count = messageModel->rowCount();
    if (count > 0) {
        if (totalMessages > 0)
            loadStatusLabel->setText(QString("%1/%2 messages").arg(count).arg(totalMessages));
        else
            loadStatusLabel->setText(QString("%1 messages loaded").arg(count));
    } else {
        loadStatusLabel->setText("");
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
        searchResultLabel->setText("");
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
            searchResultLabel->setText(QString("%1 found").arg(messageModel->messages.size()));
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
    HttpReqChatUpdateTodoAsync(text, *(adaptixWidget->GetProfile()),
        [this, text](bool success, const QString&, const QJsonObject& response) {
            if (!success) {
                MessageError("Failed to send to ToDo");
                return;
            }
            todoWidget->SetTodo(text, adaptixWidget->GetProfile()->GetUsername(), QDateTime::currentSecsSinceEpoch());
        }
    );
}

void ChatWidget::onScrollChanged() {
    QScrollBar* vbar = messageView->verticalScrollBar();
    atBottom = (vbar->value() >= vbar->maximum() - 10);
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
        auto* coreDw = dockWidget->dockWidget();
        if (coreDw && !coreDw->isCurrentTab()) {
            adaptixWidget->ChatUnreadIncrement();
            blinkNewContent();
        }
    }
}

void ChatWidget::FinishPresync() {
    presyncing = false;
    hasMore = (messageModel->rowCount() >= 40);
    loadEarlierBtn->setVisible(hasMore);
    messageModel->resolveReplyTexts();
    messageModel->refreshReplyDeleted();
    updateLoadStatus();
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

void ChatWidget::Clear() {
    messageModel->clear();
    todoWidget->SetTodo("", "", 0);
    hasMore = true;
    loadEarlierBtn->setVisible(false);
    updateLoadStatus();
}
