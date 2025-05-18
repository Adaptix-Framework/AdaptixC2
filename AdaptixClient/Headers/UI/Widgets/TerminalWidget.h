#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <main.h>
#include <Agent/Agent.h>
#include <Konsole/konsole.h>

class TerminalWidget : public QWidget
{
     QWidget*     mainWidget      = nullptr;
     QGridLayout* mainGridLayout  = nullptr;
     QHBoxLayout* topHBoxLayout   = nullptr;
     QLabel*      statusDescLabel = nullptr;
     QLabel*      statusLabel     = nullptr;
     QFrame*      line_1          = nullptr;
     QFrame*      line_2          = nullptr;
     QPushButton* startButton     = nullptr;
     QPushButton* restartButton   = nullptr;
     QPushButton* stopButton      = nullptr;
     QComboBox*   programComboBox = nullptr;
     QLineEdit*   programInput    = nullptr;
     QSpacerItem* spacer          = nullptr;

     QTermWidget* termWidget      = nullptr;

     Agent* agent = nullptr;

     void createUI();

     void SetFont();
     void SetSettings();

public:
     explicit TerminalWidget(Agent* a, QWidget* w);
     ~TerminalWidget() override;

     QTermWidget* Konsole();

public slots:
     void handleTerminalMenu(const QPoint &pos);
     void onStart();
     void onRestart();
     void onStop();
     void onProgramChanged();
     void recvDataFromSocket(const QByteArray &msg);
};

#endif //TERMINALWIDGET_H
