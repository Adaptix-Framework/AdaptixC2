#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>

class Agent;
class Konsole;
class QTermWidget;
class TerminalWorker;
class AdaptixWidget;

class TerminalWidget : public DockTab
{
     AdaptixWidget* adaptixWidget   = nullptr;
     QGridLayout*   mainGridLayout  = nullptr;
     QHBoxLayout*   topHBoxLayout   = nullptr;
     QLabel*        statusDescLabel = nullptr;
     QLabel*        statusLabel     = nullptr;
     QFrame*        line_1          = nullptr;
     QFrame*        line_2          = nullptr;
     QFrame*        line_3          = nullptr;
     QPushButton*   startButton     = nullptr;
     QPushButton*   stopButton      = nullptr;
     QComboBox*     programComboBox = nullptr;
     QLabel*        keytabLabel     = nullptr;
     QComboBox*     keytabComboBox  = nullptr;
     QLineEdit*     programInput    = nullptr;
     QSpacerItem*   spacer          = nullptr;

     QTermWidget* termWidget      = nullptr;

     QThread*        terminalThread = nullptr;
     TerminalWorker* terminalWorker = nullptr;

     Agent* agent = nullptr;

     void createUI();

     void SetFont();
     void SetSettings();
     void SetKeys();

public:
     explicit TerminalWidget(Agent* a, AdaptixWidget* w);
     ~TerminalWidget() override;

     void setStatus(const QString& text);
     QTermWidget* Konsole();

public Q_SLOTS:
     void handleTerminalMenu(const QPoint &pos);
     void onStart();
     void onRestart();
     void onStop();
     void onProgramChanged();
     void onKeytabChanged();
     void recvDataFromSocket(const QByteArray &msg);
};

#endif
