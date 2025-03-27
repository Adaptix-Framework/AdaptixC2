#ifndef ADAPTIXCLIENT_CONSOLEWIDGET_H
#define ADAPTIXCLIENT_CONSOLEWIDGET_H

#include <main.h>
#include <Agent/Agent.h>
#include <Utils/KeyPressHandler.h>

#define CONSOLE_OUT_LOCAL         1
#define CONSOLE_OUT_LOCAL_INFO    2
#define CONSOLE_OUT_LOCAL_ERROR   3
#define CONSOLE_OUT_LOCAL_SUCCESS 4
#define CONSOLE_OUT_INFO          5
#define CONSOLE_OUT_ERROR         6
#define CONSOLE_OUT_SUCCESS       7


class ConsoleWidget : public QWidget
{
    QGridLayout* MainGridLayout      = nullptr;
    QLabel*      CmdLabel            = nullptr;
    QLabel*      InfoLabel           = nullptr;
    QLineEdit*   InputLineEdit       = nullptr;
    QTextEdit*   OutputTextEdit      = nullptr;
    QCompleter*  CommandCompleter    = nullptr;
    QStringListModel* completerModel = nullptr;

    bool userSelectedCompletion      = false;

    Agent*     agent     = nullptr;
    Commander* commander = nullptr;
    KPH_ConsoleInput * kphInputLineEdit = nullptr;

    void createUI();

public:
    explicit ConsoleWidget(Agent* a, Commander* c);
    ~ConsoleWidget() override;

    void UpgradeCompleter() const;
    void InputFocus() const;

    void ConsoleOutputMessage( qint64 timestamp, const QString &taskId, int type, const QString &message, const QString &text, bool completed ) const;
    void ConsoleOutputPrompt( qint64 timestamp, const QString &taskId, const QString &user, const QString &commandLine ) const;

public slots:
    void processInput();
    void onCompletionSelected(const QString &selectedText);
};

#endif //ADAPTIXCLIENT_CONSOLEWIDGET_H