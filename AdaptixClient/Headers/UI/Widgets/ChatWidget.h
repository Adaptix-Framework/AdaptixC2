#ifndef ADAPTIXCLIENT_CHATWIDGET_H
#define ADAPTIXCLIENT_CHATWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements.h>
#include <oclero/qlementine/widgets/LineEdit.hpp>

class AdaptixWidget;

class ChatWidget : public DockTab
{
    AdaptixWidget*   adaptixWidget  = nullptr;
    QLabel*          usernameLabel  = nullptr;
    QLineEdit*       chatInput      = nullptr;
    TextEditConsole* chatTextEdit   = nullptr;
    QGridLayout*     chatGridLayout = nullptr;

    QWidget*        searchWidget   = nullptr;
    QHBoxLayout*    searchLayout   = nullptr;
    ClickableLabel* prevButton     = nullptr;
    ClickableLabel* nextButton     = nullptr;
    QLabel*         searchLabel    = nullptr;
    ClickableLabel* hideButton     = nullptr;
    QSpacerItem*    spacer         = nullptr;
    QShortcut*      shortcutSearch = nullptr;
    oclero::qlementine::LineEdit* searchLineEdit = nullptr;

    int  currentIndex = -1;
    QVector<QTextEdit::ExtraSelection> allSelections;

    void createUI();
    void findAndHighlightAll(const QString& pattern);
    void highlightCurrent() const;

public:
    explicit ChatWidget(AdaptixWidget* w);
    ~ChatWidget() override;

    void SetUpdatesEnabled(const bool enabled);

    void AddChatMessage(qint64 time, const QString &username, const QString &message);
    void Clear() const;

public Q_SLOTS:
    void handleChat();
    void toggleSearchPanel();
    void handleSearch();
    void handleSearchBackward();
};

#endif //ADAPTIXCLIENT_CHATWIDGET_H