#include "SearchBar.h"

#include <QMenu>
#include <QPalette>
#include <QKeyEvent>
#include <QHBoxLayout>

SearchBar::SearchBar(QWidget *parent) : QWidget(parent)
{
    setAutoFillBackground(true);

    closeButton = new QToolButton(this);
    closeButton->setText("X");

    findLabel = new QLabel(tr("Find:"), this);

    searchTextEdit = new QLineEdit(this);

    findPreviousButton = new QToolButton(this);
    findPreviousButton->setText("<");

    findNextButton = new QToolButton(this);
    findNextButton->setText(">");

    optionsButton = new QToolButton(this);
    optionsButton->setText("...");
    optionsButton->setPopupMode(QToolButton::InstantPopup);

    layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(closeButton);
    layout->addWidget(findLabel);
    layout->addWidget(searchTextEdit);
    layout->addWidget(findPreviousButton);
    layout->addWidget(findNextButton);
    layout->addWidget(optionsButton);
    setLayout(layout);

    connect(closeButton, &QToolButton::clicked, this, &SearchBar::hide);
    connect(searchTextEdit, &QLineEdit::textChanged, this, &SearchBar::searchCriteriaChanged);
    connect(findPreviousButton, &QToolButton::clicked, this, &SearchBar::findPrevious);
    connect(findNextButton, &QToolButton::clicked, this, &SearchBar::findNext);

    connect(this, &SearchBar::searchCriteriaChanged, this, &SearchBar::clearBackgroundColor);

    optionsMenu = new QMenu(optionsButton);
    optionsButton->setMenu(optionsMenu);

    m_matchCaseMenuEntry = optionsMenu->addAction(tr("Match case"));
    m_matchCaseMenuEntry->setCheckable(true);
    m_matchCaseMenuEntry->setChecked(true);
    connect(m_matchCaseMenuEntry, &QAction::toggled, this, &SearchBar::searchCriteriaChanged);

    m_useRegularExpressionMenuEntry = optionsMenu->addAction(tr("Regular expression"));
    m_useRegularExpressionMenuEntry->setCheckable(true);
    connect(m_useRegularExpressionMenuEntry, &QAction::toggled, this, &SearchBar::searchCriteriaChanged);

    m_highlightMatchesMenuEntry = optionsMenu->addAction(tr("Highlight all matches"));
    m_highlightMatchesMenuEntry->setCheckable(true);
    m_highlightMatchesMenuEntry->setChecked(true);
    connect(m_highlightMatchesMenuEntry, &QAction::toggled, this, &SearchBar::highlightMatchesChanged);

    retranslateUi();
}

SearchBar::~SearchBar() {}

QString SearchBar::searchText() {
    return searchTextEdit->text();
}

bool SearchBar::useRegularExpression() {
    return m_useRegularExpressionMenuEntry->isChecked();
}

bool SearchBar::matchCase() {
    return m_matchCaseMenuEntry->isChecked();
}

bool SearchBar::highlightAllMatches() {
    return m_highlightMatchesMenuEntry->isChecked();
}

void SearchBar::show() {
    QWidget::show();
    searchTextEdit->setFocus();
    searchTextEdit->selectAll();
}

void SearchBar::hide() {
    QWidget::hide();
    if (QWidget *p = parentWidget()) {
        p->setFocus(Qt::OtherFocusReason);
    }
}

void SearchBar::noMatchFound() {
    QPalette palette;
    palette.setColor(searchTextEdit->backgroundRole(), QColor(255, 128, 128));
    searchTextEdit->setPalette(palette);
}

void SearchBar::keyReleaseEvent(QKeyEvent* keyEvent) {
    if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
        if (keyEvent->modifiers() == Qt::ShiftModifier) {
            emit findPrevious();
        } else {
            emit findNext();
        }
    } else if (keyEvent->key() == Qt::Key_Escape) {
        hide();
    }
}

void SearchBar::clearBackgroundColor() {
    searchTextEdit->setPalette(QWidget::window()->palette());
}

void SearchBar::setText(const QString &text) {
    searchTextEdit->setText(text);
}

void SearchBar::retranslateUi() {
    findLabel->setText(tr("Find:"));
    m_matchCaseMenuEntry->setText(tr("Match case"));
    m_useRegularExpressionMenuEntry->setText(tr("Regular expression"));
    m_highlightMatchesMenuEntry->setText(tr("Highlight all matches"));
}
