#ifndef ADAPTIXCLIENT_CHATWIDGET_H
#define ADAPTIXCLIENT_CHATWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <QListView>
#include <QStyledItemDelegate>
#include <QAbstractListModel>
#include <QTextDocument>
#include <QCache>
#include <QTextBrowser>
#include <QToolButton>
#include <oclero/qlementine/widgets/Switch.hpp>
#include <QSplitter>
#include <QScrollBar>
#include <QKeyEvent>

class AdaptixWidget;

struct ChatReaction {
    QString        emoji;
    QStringList    users;
};

struct ChatMessage {
    qint64              id;
    QString             username;
    QString             message;
    qint64              date;
    bool                edited;
    bool                deleted;
    qint64              deletedDate;
    QList<ChatReaction> reactions;
    qint64              replyToId;
    QString             replyToName;
    QString             replyToText;
    bool                replyToDeleted;
    bool                isMarkdown;
};

class ChatMessageModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum ChatRole {
        IdRole = Qt::UserRole + 1,
        UsernameRole, MessageRole, DateRole,
        EditedRole, DeletedRole, DeletedDateRole, ReactionsRole, IsMineRole,
        ReplyToIdRole, ReplyToNameRole, ReplyToTextRole, ReplyToDeletedRole,
        FlashRole, IsMarkdownRole
    };

    explicit ChatMessageModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void addMessage(const ChatMessage& msg);
    void prependMessages(const QList<ChatMessage>& msgs);
    void editMessage(qint64 id, const QString& newText);
    void deleteMessage(qint64 id);
    void setReactions(qint64 id, const QString& json);
    void refreshReplyDeleted();
    void resolveReplyTexts();
    void clear();
    qint64 oldestId() const;
    ChatMessage getMessageById(qint64 id) const;

private:
    QList<ChatMessage> messages;
    QString currentUser;
    qint64 flashId = 0;
    friend class ChatWidget;
};

class ChatMessageDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ChatMessageDelegate(const QString& currentUser, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    void invalidateCache(qint64 id);
    void clearCache();

Q_SIGNALS:
    void reactionClicked(qint64 id, const QString& emoji);
    void replyClicked(qint64 id);

protected:
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) override;
    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index) override;

private:
    QTextDocument* getDocument(const QModelIndex& index, int bubbleWidth) const;

    QString currentUser;
    mutable QCache<qint64, QTextDocument> docCache;
};

class TodoWidget : public QWidget {
    Q_OBJECT
public:
    explicit TodoWidget(QWidget* parent = nullptr);
    void SetTodo(const QString& content, const QString& updatedBy, qint64 updatedAt);

Q_SIGNALS:
    void saveRequested(const QString& content);

private:
    void setEditMode(bool on);
    void updatePreview();
    void insertMarkdown(const QString& before, const QString& after);

    QStackedWidget*  contentStack = nullptr;
    QTextBrowser*    renderedView = nullptr;
    QPlainTextEdit*  editor = nullptr;
    QTextBrowser*    livePreview = nullptr;
    QFrame*          toolbarRow = nullptr;
    QPushButton*     editBtn = nullptr;
    QPushButton*     saveBtn = nullptr;
    QPushButton*     cancelBtn = nullptr;
    QLabel*          statusLabel = nullptr;
    QTimer*          debounceTimer = nullptr;
    bool             updating = false;
    bool             editMode = false;
};

class ChatWidget : public DockTab
{
    Q_OBJECT

    AdaptixWidget*         adaptixWidget = nullptr;
    ChatMessageModel*      messageModel = nullptr;
    ChatMessageDelegate*   messageDelegate = nullptr;
    QListView*             messageView = nullptr;
    TodoWidget*            todoWidget = nullptr;

    QFrame*       searchBar = nullptr;
    QLineEdit*    searchInput = nullptr;
    QPushButton*  searchBtn = nullptr;
    QLabel*       searchResultLabel = nullptr;

    QPlainTextEdit* chatInput = nullptr;
    QTextBrowser*   mdPreview = nullptr;
    bool            markdownMode = false;
    QFrame*         replyPreview = nullptr;
    QLabel*         replyLabel = nullptr;

    QPushButton* loadEarlierBtn = nullptr;
    QLabel*      loadStatusLabel = nullptr;
    QPushButton* clearChatBtn = nullptr;
    QPushButton* deleteSelectedBtn = nullptr;
    oclero::qlementine::Switch* hideDeletedSwitch = nullptr;
    bool         hideDeleted = false;
    bool         loadingMore = false;
    bool         hasMore = true;
    int          totalMessages = 0;

    bool atBottom = true;
    bool editMode = false;
    bool presyncing = true;
    qint64 editId = 0;
    qint64 replyToId = 0;
    QString replyToName;
    qint64 flashId = 0;

    void createUI();
    void loadHistory();
    void loadMore();
    void updateLoadStatus();
    void scrollToBottomIfNeeded();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

public:
    explicit ChatWidget(AdaptixWidget* w);
    ~ChatWidget() override;

    void SetUpdatesEnabled(bool enabled);
    void AddChatMessage(qint64 id, const QString& username, const QString& message, qint64 date, bool edited, bool deleted, const QString& reactions, qint64 replyToId, const QString& replyToName);
    void EditChatMessage(qint64 id, const QString& text);
    void DeleteChatMessage(qint64 id);
    void UpdateReactions(qint64 id, const QString& json);
    void SetTodo(const QString& content, const QString& updatedBy, qint64 updatedAt);
    void FinishPresync();
    void Clear();

public Q_SLOTS:
    void handleSend();
    void handleSearch();
    void handleReaction(qint64 id, const QString& emoji);
    void enterEditMode(qint64 id);
    void exitEditMode();
    void enterReplyMode(qint64 id, const QString& username);
    void exitReplyMode();
    void sendToTodo(const QString& text);
    void onScrollChanged();
};

#endif //ADAPTIXCLIENT_CHATWIDGET_H
