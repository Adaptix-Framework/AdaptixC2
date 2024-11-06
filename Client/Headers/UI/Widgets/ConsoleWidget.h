#ifndef ADAPTIXCLIENT_CONSOLEWIDGET_H
#define ADAPTIXCLIENT_CONSOLEWIDGET_H

#include <main.h>
#include <Agent/Agent.h>

#define CONSOLE_OUT_LOCAL         0
#define CONSOLE_OUT_LOCAL_INFO    1
#define CONSOLE_OUT_LOCAL_ERROR   2
#define CONSOLE_OUT_LOCAL_SUCCESS 3
#define CONSOLE_OUT_INFO          4
#define CONSOLE_OUT_ERROR         5
#define CONSOLE_OUT_SUCCESS       6

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

    void OutputConsole( int type, qint64 timestamp, QString taskId, QString user, QString commandLine, QString message, QString data, bool taskFinished  );

public slots:
    void processInput();
};

#endif //ADAPTIXCLIENT_CONSOLEWIDGET_H
