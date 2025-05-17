#ifndef _SEARCHBAR_H
#define	_SEARCHBAR_H

#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QAction>
#include <QHBoxLayout>

#include "HistorySearch.h"

class SearchBar : public QWidget {
    Q_OBJECT

public:
    SearchBar(QWidget* parent = nullptr);
    ~SearchBar() override;
    void show();
    QString searchText();
    bool useRegularExpression();
    bool matchCase();
    bool highlightAllMatches();
    void setText(const QString &text);
    void retranslateUi();

public slots:
    void noMatchFound();
    void hide();

signals:
    void searchCriteriaChanged();
    void highlightMatchesChanged(bool highlightMatches);
    void findNext();
    void findPrevious();

protected:
    void keyReleaseEvent(QKeyEvent* keyEvent) override;

private slots:
    void clearBackgroundColor();

private:
    QToolButton *closeButton;
    QLabel *findLabel;
    QLineEdit *searchTextEdit;
    QToolButton *findPreviousButton;
    QToolButton *findNextButton;
    QToolButton *optionsButton;
    QHBoxLayout *layout;

    QMenu   *optionsMenu;
    QAction *m_matchCaseMenuEntry;
    QAction *m_useRegularExpressionMenuEntry;
    QAction *m_highlightMatchesMenuEntry;
};


#endif	/* _SEARCHBAR_H */
