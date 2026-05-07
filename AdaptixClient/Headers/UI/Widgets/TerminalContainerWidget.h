#ifndef TERMINALCONTAINERWIDGET_H
#define TERMINALCONTAINERWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements.h>

enum TerminalMode {
    TerminalModePTY,
    TerminalModeShell
};

class Agent;
class QTermWidget;
class TerminalWorker;
class AdaptixWidget;

class TerminalTab : public QWidget
{
Q_OBJECT

    QWidget*       topWidget       = nullptr;
    QGridLayout*   mainGridLayout  = nullptr;
    QHBoxLayout*   topHBoxLayout   = nullptr;
    QLabel*        statusDescLabel = nullptr;
    QLabel*        statusLabel     = nullptr;
    QFrame*        line_1          = nullptr;
    QFrame*        line_2          = nullptr;
    QFrame*        line_3          = nullptr;
    QFrame*        line_4          = nullptr;
    QPushButton*   startButton     = nullptr;
    QPushButton*   stopButton      = nullptr;
    QComboBox*     programComboBox = nullptr;
    QLabel*        keytabLabel     = nullptr;
    QComboBox*     keytabComboBox  = nullptr;
    QLineEdit*     programInput    = nullptr;
    QSpacerItem*   spacer          = nullptr;

    TerminalMode   terminalMode    = TerminalModePTY;
    QByteArray     shellInputBuffer;

    QCheckBox*     smartOutputCheckBox = nullptr;
    bool           smartOutputEnabled  = false;
    QByteArray     lastSentCommand;
    QByteArray     outputBuffer;
    QByteArray     lastPrompt;
    int            filterPhase = 0;

    QByteArray processSmartOutput(const QByteArray &data);

    QTermWidget*    termWidget      = nullptr;
    QThread*        terminalThread  = nullptr;
    TerminalWorker* terminalWorker  = nullptr;

    AdaptixWidget* adaptixWidget   = nullptr;
    Agent*         agent           = nullptr;

    void createUI();
    void SetFont();
    void SetSettings();
    void SetKeys();

public:
    explicit TerminalTab(Agent* a, AdaptixWidget* w, TerminalMode mode, QWidget* parent = nullptr);
    ~TerminalTab() override;

    void setStatus(const QString& text);
    QTermWidget* Konsole();
    bool isRunning() const;

public Q_SLOTS:
    void handleTerminalMenu(const QPoint &pos);
    void onStart();
    void onStop();
    void onProgramChanged();
    void onKeytabChanged();
    void recvDataFromSocket(const QByteArray &msg);
    void sendDataToSocket(const char* data, int size);
};




class TerminalContainerWidget : public DockTab
{
Q_OBJECT

    VerticalTabWidget* tabWidget  = nullptr;
    QVBoxLayout*       mainLayout = nullptr;

    AdaptixWidget* adaptixWidget = nullptr;
    Agent*         agent         = nullptr;
    TerminalMode   terminalMode  = TerminalModePTY;
    int            tabCounter    = 0;

public:
    explicit TerminalContainerWidget(Agent* a, AdaptixWidget* w, TerminalMode mode = TerminalModePTY);
    ~TerminalContainerWidget() override;

    void addNewTerminal();
    void closeTab(int index);

private Q_SLOTS:
    void onTabCloseRequested(int index);
};

#endif
