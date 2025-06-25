#ifndef ADAPTIXCLIENT_LOGSWIDGET_H
#define ADAPTIXCLIENT_LOGSWIDGET_H

#include <main.h>
#include <Utils/CustomElements.h>

class LogsWidget : public QWidget
{
    QGridLayout*     mainGridLayout      = nullptr;
    QGridLayout*     logsGridLayout      = nullptr;
    QGridLayout*     todoGridLayout      = nullptr;
    TextEditConsole* logsConsoleTextEdit = nullptr;
    QLabel*          logsLabel           = nullptr;
    QLabel*          todoLabel           = nullptr;
    QSplitter*       mainHSplitter       = nullptr;
    QWidget*         logsWidget          = nullptr;
    QWidget*         todoWidget          = nullptr;

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
    explicit LogsWidget();
    ~LogsWidget() override;

     void AddLogs( int type, qint64 time, const QString &Message) const;
     void Clear() const;

public slots:
    void toggleSearchPanel();
    void handleSearch();
    void handleSearchBackward();
};

#endif //ADAPTIXCLIENT_LOGSWIDGET_H
