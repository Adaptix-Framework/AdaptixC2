#ifndef ADAPTIXCLIENT_SEARCHPANEL_H
#define ADAPTIXCLIENT_SEARCHPANEL_H

#include <main.h>
#include <QPointer>
#include <Utils/CustomElements/ClickableLabel.h>

#include <oclero/qlementine/widgets/LineEdit.hpp>

class SearchPanel : public QWidget {
Q_OBJECT
    QHBoxLayout*                  layout         = nullptr;
    QFrame*                       chrome         = nullptr;
    QHBoxLayout*                  chromeLayout   = nullptr;
    QToolButton*                  prevButton     = nullptr;
    QToolButton*                  nextButton     = nullptr;
    QLabel*                       searchLabel    = nullptr;
    oclero::qlementine::LineEdit* searchLineEdit = nullptr;
    QToolButton*                  historyButton  = nullptr;
    QToolButton*                  hideButton     = nullptr;

    QPointer<QPlainTextEdit>           target;
    QVector<QTextEdit::ExtraSelection> selections;
    int                                currentIndex = -1;
    QString                            scopeHint;
    bool                               historySearchEnabled = false;

    void findAndHighlightAll(const QString& pattern);
    void highlightCurrent();
    void applyChromeStyle();

public:
    explicit SearchPanel(QPlainTextEdit* target, QWidget* parent = nullptr);

    void setTarget(QPlainTextEdit* newTarget);
    void clearSelections();
    void setScopeHint(const QString& hint);
    QString currentQuery() const;
    void highlightLocalQuery(const QString& pattern);

    void setHistorySearchEnabled(bool enabled);

public Q_SLOTS:
    void toggle();
    void searchNext();
    void searchPrevious();

Q_SIGNALS:
    void historySearchRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

#endif // ADAPTIXCLIENT_SEARCHPANEL_H
