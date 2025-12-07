#ifndef ADAPTIXCLIENT_LOGSWIDGET_H
#define ADAPTIXCLIENT_LOGSWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements.h>

class AdaptixWidget;

class LogsWidget : public DockTab
{
    QGridLayout*     mainGridLayout      = nullptr;
    QGridLayout*     logsGridLayout      = nullptr;
    // QGridLayout*     todoGridLayout      = nullptr;
    TextEditConsole* logsConsoleTextEdit = nullptr;
    QLabel*          logsLabel           = nullptr;
    // QLabel*          todoLabel           = nullptr;
    QSplitter*       mainHSplitter       = nullptr;
    QWidget*         logsWidget          = nullptr;
    // QWidget*         todoWidget          = nullptr;

    QWidget*        searchWidget   = nullptr;
    QHBoxLayout*    searchLayout   = nullptr;
    ClickableLabel* prevButton     = nullptr;
    ClickableLabel* nextButton     = nullptr;
    QLabel*         searchLabel    = nullptr;
    QLineEdit*      searchLineEdit = nullptr;
    ClickableLabel* hideButton     = nullptr;
    QSpacerItem*    spacer         = nullptr;
    QShortcut*      shortcutSearch = nullptr;

    bool userSelectedCompletion = false;
    int  currentIndex = -1;
    QVector<QTextEdit::ExtraSelection> allSelections;

    void createUI();
    void findAndHighlightAll(const QString& pattern);
    void highlightCurrent() const;

public:
    explicit LogsWidget(AdaptixWidget* w);
    ~LogsWidget() override;

    void SetUpdatesEnabled(bool enabled);

     void AddLogs( int type, qint64 time, const QString &Message);
     void Clear() const;

public Q_SLOTS:
    void toggleSearchPanel();
    void handleSearch();
    void handleSearchBackward();
};

#endif
