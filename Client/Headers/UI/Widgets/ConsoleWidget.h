#ifndef ADAPTIXCLIENT_CONSOLEWIDGET_H
#define ADAPTIXCLIENT_CONSOLEWIDGET_H

#include <main.h>
#include <Agent/Agent.h>

#define CONSOLE_OUT_LOCAL         1
#define CONSOLE_OUT_LOCAL_INFO    2
#define CONSOLE_OUT_LOCAL_ERROR   3
#define CONSOLE_OUT_LOCAL_SUCCESS 4
#define CONSOLE_OUT_INFO          5
#define CONSOLE_OUT_ERROR         6
#define CONSOLE_OUT_SUCCESS       7

class ConsoleWidget : public QWidget
{
    QGridLayout* MainGridLayout   = nullptr;
    QLabel*      CmdLabel         = nullptr;
    QLabel*      InfoLabel        = nullptr;
    QLineEdit*   InputLineEdit    = nullptr;
    QTextEdit*   OutputTextEdit   = nullptr;
    QCompleter*  CommandCompleter = nullptr;

    Agent*     agent     = nullptr;
    Commander* commander = nullptr;

    void createUI();

public:
    explicit ConsoleWidget(Agent* a, Commander* c);
    ~ConsoleWidget();

    void ConsoleOutputMessage( qint64 timestamp, QString taskId, int type, QString message, QString text, bool completed );
    void ConsoleOutputPrompt( qint64 timestamp, QString taskId, QString user, QString commandLine );

public slots:
    void processInput();
};

#endif //ADAPTIXCLIENT_CONSOLEWIDGET_H
