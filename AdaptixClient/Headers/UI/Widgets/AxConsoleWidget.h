#ifndef AXCONSOLEWIDGET_H
#define AXCONSOLEWIDGET_H

#include <main.h>
#include <QPointer>
#include <UI/Widgets/AbstractDock.h>

class AdaptixWidget;
class AxScriptManager;
class TextEditConsole;
class ClickableLabel;
class KPH_ConsoleInput;

class AxConsoleWidget : public DockTab
{
    AdaptixWidget*   adaptixWidget = nullptr;
    QPointer<AxScriptManager> scriptManager;

    QGridLayout*     MainGridLayout = nullptr;
    QLabel*          CmdLabel       = nullptr;
    QLineEdit*       InputLineEdit  = nullptr;
    TextEditConsole* OutputTextEdit = nullptr;
    QPushButton*     ResetButton    = nullptr;

    QWidget*        searchWidget   = nullptr;
    QHBoxLayout*    searchLayout   = nullptr;
    ClickableLabel* prevButton     = nullptr;
    ClickableLabel* nextButton     = nullptr;
    QLabel*         searchLabel    = nullptr;
    QLineEdit*      searchLineEdit = nullptr;
    ClickableLabel* hideButton     = nullptr;
    QSpacerItem*    spacer         = nullptr;
    QShortcut*      shortcutSearch = nullptr;

    int  currentIndex = -1;
    QVector<QTextEdit::ExtraSelection> allSelections;

    KPH_ConsoleInput* kphInputLineEdit = nullptr;

    void createUI();
    void findAndHighlightAll(const QString& pattern);
    void highlightCurrent() const;

public:
    explicit AxConsoleWidget(AxScriptManager* m, AdaptixWidget* w);
    ~AxConsoleWidget() override;

    void SetUpdatesEnabled(const bool enabled);

    void OutputClear() const;
    void InputFocus() const;
    void AddToHistory(const QString& command);
    void PrintMessage(const QString& message);
    void PrintError(const QString& message);

public Q_SLOTS:
    void processInput();
    void toggleSearchPanel();
    void handleSearch();
    void handleSearchBackward();
    void handleShowHistory();
    void onResetScript();
};

#endif
