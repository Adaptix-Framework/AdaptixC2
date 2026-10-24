#ifndef ADAPTIXCLIENT_ADVISORWIDGET_H
#define ADAPTIXCLIENT_ADVISORWIDGET_H

#include <main.h>
#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements.h>

class AdaptixWidget;

class AdvisorWidget : public DockTab
{
    AdaptixWidget*   adaptixWidget  = nullptr;
    TextEditConsole* advisorOutput  = nullptr;
    QGridLayout*     gridLayout     = nullptr;

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

    void createUI();
    void findAndHighlightAll(const QString& pattern);
    void highlightCurrent() const;

public:
    explicit AdvisorWidget(AdaptixWidget* w);
    ~AdvisorWidget() override;

    void SetUpdatesEnabled(const bool enabled);

    void AddRecommendation(qint64 date, const QString &agentId, const QString &command,
                           const QString &rationale, const QString &lookFor,
                           const QString &priority, const QString &category,
                           const QStringList &alternatives, const QString &opsecNote);
    void Clear() const;

public Q_SLOTS:
    void toggleSearchPanel();
    void handleSearch();
    void handleSearchBackward();
};

#endif //ADAPTIXCLIENT_ADVISORWIDGET_H
