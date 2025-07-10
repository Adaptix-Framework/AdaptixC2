#ifndef ADAPTIXCLIENT_CONSOLEWIDGET_H
#define ADAPTIXCLIENT_CONSOLEWIDGET_H

#include <main.h>
#include <Utils/KeyPressHandler.h>
#include <Utils/CustomElements.h>
#include <Agent/Commander.h>

class Agent;

#define CONSOLE_OUT_LOCAL         1
#define CONSOLE_OUT_LOCAL_INFO    2
#define CONSOLE_OUT_LOCAL_ERROR   3
#define CONSOLE_OUT_LOCAL_SUCCESS 4
#define CONSOLE_OUT_INFO          5
#define CONSOLE_OUT_ERROR         6
#define CONSOLE_OUT_SUCCESS       7
#define CONSOLE_OUT               10

class  ConsoleWidget : public QWidget
{
    QGridLayout*      MainGridLayout   = nullptr;
    QLabel*           CmdLabel         = nullptr;
    QLabel*           InfoLabel        = nullptr;
    QLineEdit*        InputLineEdit    = nullptr;
    TextEditConsole*  OutputTextEdit   = nullptr;
    QCompleter*       CommandCompleter = nullptr;
    QStringListModel* completerModel   = nullptr;

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

    Agent*     agent     = nullptr;
    Commander* commander = nullptr;
    KPH_ConsoleInput* kphInputLineEdit = nullptr;

    void createUI();
    void findAndHighlightAll(const QString& pattern);
    void highlightCurrent() const;

public:
    explicit ConsoleWidget(Agent* a, Commander* c);
    ~ConsoleWidget() override;

    void ProcessCmdResult(const QString &commandLine, const CommanderResult &cmdResult);

    void UpgradeCompleter() const;
    void InputFocus() const;
    void AddToHistory(const QString& command);
    void SetInput(const QString &command);
    void Clear();


    void ConsoleOutputMessage( qint64 timestamp, const QString &taskId, int type, const QString &message, const QString &text, bool completed ) const;
    void ConsoleOutputPrompt( qint64 timestamp, const QString &taskId, const QString &user, const QString &commandLine ) const;

public slots:
    void processInput();
    void onCompletionSelected(const QString &selectedText);
    void toggleSearchPanel();
    void handleSearch();
    void handleSearchBackward();
    void handleShowHistory();
};

#endif //ADAPTIXCLIENT_CONSOLEWIDGET_H