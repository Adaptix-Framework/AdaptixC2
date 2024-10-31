#ifndef ADAPTIXCLIENT_CONSOLEWIDGET_H
#define ADAPTIXCLIENT_CONSOLEWIDGET_H

#include <main.h>
#include <Agent/Agent.h>

class ConsoleWidget : public QWidget
{
    QGridLayout* MainGridLayout = nullptr;
    QLabel*      CmdLabel       = nullptr;
    QLabel*      InfoLabel      = nullptr;
    QLineEdit*   InputLineEdit  = nullptr;
    QTextEdit*   OutputTextEdit = nullptr;

    Agent* agent = nullptr;

    void createUI();

public:
    explicit ConsoleWidget(Agent* a);
    ~ConsoleWidget();
};

#endif //ADAPTIXCLIENT_CONSOLEWIDGET_H
