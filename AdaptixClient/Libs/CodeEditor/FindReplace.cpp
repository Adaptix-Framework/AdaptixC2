#include "FindReplace.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QToolButton>
#include <QLabel>
#include <QTextEdit>
#include <QKeyEvent>
#include <QTextDocument>
#include <QRegularExpression>

FindReplace::FindReplace(QWidget* parent) : QWidget(parent), m_editor(nullptr)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 2, 4, 2);
    mainLayout->setSpacing(2);

    auto* findRow = new QHBoxLayout();
    findRow->setSpacing(4);

    m_findEdit = new QLineEdit(this);
    m_findEdit->setPlaceholderText("Find...");
    findRow->addWidget(m_findEdit);

    m_caseBtn = new QToolButton(this);
    m_caseBtn->setText("Aa");
    m_caseBtn->setToolTip("Case sensitive");
    m_caseBtn->setCheckable(true);
    findRow->addWidget(m_caseBtn);

    m_regexBtn = new QToolButton(this);
    m_regexBtn->setText(".*");
    m_regexBtn->setToolTip("Regex");
    m_regexBtn->setCheckable(true);
    findRow->addWidget(m_regexBtn);

    auto* prevBtn = new QToolButton(this);
    prevBtn->setText("<");
    prevBtn->setToolTip("Previous");
    findRow->addWidget(prevBtn);

    auto* nextBtn = new QToolButton(this);
    nextBtn->setText(">");
    nextBtn->setToolTip("Next");
    findRow->addWidget(nextBtn);

    m_matchLabel = new QLabel(this);
    m_matchLabel->setMinimumWidth(60);
    findRow->addWidget(m_matchLabel);

    m_closeBtn = new QToolButton(this);
    m_closeBtn->setText("X");
    m_closeBtn->setToolTip("Close");
    findRow->addWidget(m_closeBtn);

    mainLayout->addLayout(findRow);

    m_replaceRow = new QWidget(this);
    auto* replaceLayout = new QHBoxLayout(m_replaceRow);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(4);

    m_replaceEdit = new QLineEdit(m_replaceRow);
    m_replaceEdit->setPlaceholderText("Replace...");
    replaceLayout->addWidget(m_replaceEdit);

    auto* replaceBtn = new QToolButton(m_replaceRow);
    replaceBtn->setText("Replace");
    replaceLayout->addWidget(replaceBtn);

    auto* replaceAllBtn = new QToolButton(m_replaceRow);
    replaceAllBtn->setText("All");
    replaceAllBtn->setToolTip("Replace all");
    replaceLayout->addWidget(replaceAllBtn);

    mainLayout->addWidget(m_replaceRow);
    m_replaceRow->setVisible(false);

    setVisible(false);

    connect(m_findEdit, &QLineEdit::returnPressed, this, &FindReplace::findNext);
    connect(m_findEdit, &QLineEdit::textChanged, this, [this]() {
        highlightAll();
        updateMatchCount();
    });
    connect(nextBtn, &QToolButton::clicked, this, &FindReplace::findNext);
    connect(prevBtn, &QToolButton::clicked, this, &FindReplace::findPrev);
    connect(replaceBtn, &QToolButton::clicked, this, &FindReplace::replaceOne);
    connect(replaceAllBtn, &QToolButton::clicked, this, &FindReplace::replaceAll);
    connect(m_closeBtn, &QToolButton::clicked, this, [this]() {
        clearHighlights();
        setVisible(false);
    });
    connect(m_caseBtn, &QToolButton::toggled, this, [this]() {
        highlightAll();
        updateMatchCount();
    });
    connect(m_regexBtn, &QToolButton::toggled, this, [this]() {
        highlightAll();
        updateMatchCount();
    });
}

void FindReplace::setEditor(QTextEdit* editor)
{
    if (m_editor == editor)
        return;
    if (m_editor)
        clearHighlights();
    m_editor = editor;
}

void FindReplace::showFind()
{
    m_replaceRow->setVisible(false);
    setVisible(true);
    m_findEdit->setFocus();
    m_findEdit->selectAll();

    auto cursor = m_editor->textCursor();
    if (cursor.hasSelection())
        m_findEdit->setText(cursor.selectedText());
    highlightAll();
    updateMatchCount();
}

void FindReplace::showReplace()
{
    m_replaceRow->setVisible(true);
    setVisible(true);
    m_findEdit->setFocus();
    m_findEdit->selectAll();

    auto cursor = m_editor->textCursor();
    if (cursor.hasSelection())
        m_findEdit->setText(cursor.selectedText());
    highlightAll();
    updateMatchCount();
}

void FindReplace::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        clearHighlights();
        setVisible(false);
        m_editor->setFocus();
        return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (e->modifiers() & Qt::ShiftModifier)
            findPrev();
        else
            findNext();
        return;
    }
    QWidget::keyPressEvent(e);
}

QTextDocument::FindFlags FindReplace_findFlags(bool caseSensitive)
{
    QTextDocument::FindFlags flags;
    if (caseSensitive)
        flags |= QTextDocument::FindCaseSensitively;
    return flags;
}

void FindReplace::findNext()
{
    QString searchText = m_findEdit->text();
    if (searchText.isEmpty())
        return;

    auto flags = FindReplace_findFlags(m_caseBtn->isChecked());

    bool found;
    if (m_regexBtn->isChecked()) {
        QRegularExpression re(searchText, m_caseBtn->isChecked()
            ? QRegularExpression::NoPatternOption
            : QRegularExpression::CaseInsensitiveOption);
        found = m_editor->document()->find(re, m_editor->textCursor()).hasSelection();
    }

    auto cursor = m_editor->document()->find(searchText, m_editor->textCursor(), flags);
    if (cursor.isNull())
        cursor = m_editor->document()->find(searchText, QTextCursor(m_editor->document()), flags);

    if (!cursor.isNull())
        m_editor->setTextCursor(cursor);
    updateMatchCount();
}

void FindReplace::findPrev()
{
    QString searchText = m_findEdit->text();
    if (searchText.isEmpty())
        return;

    auto flags = FindReplace_findFlags(m_caseBtn->isChecked());
    flags |= QTextDocument::FindBackward;

    auto cursor = m_editor->document()->find(searchText, m_editor->textCursor(), flags);
    if (cursor.isNull()) {
        QTextCursor endCursor(m_editor->document());
        endCursor.movePosition(QTextCursor::End);
        cursor = m_editor->document()->find(searchText, endCursor, flags);
    }

    if (!cursor.isNull())
        m_editor->setTextCursor(cursor);
    updateMatchCount();
}

void FindReplace::replaceOne()
{
    auto cursor = m_editor->textCursor();
    if (cursor.hasSelection())
        cursor.insertText(m_replaceEdit->text());
    findNext();
}

void FindReplace::replaceAll()
{
    QString searchText = m_findEdit->text();
    QString replaceText = m_replaceEdit->text();
    if (searchText.isEmpty())
        return;

    auto flags = FindReplace_findFlags(m_caseBtn->isChecked());

    QTextCursor cursor(m_editor->document());
    cursor.beginEditBlock();

    int count = 0;
    while (true)
    {
        auto found = m_editor->document()->find(searchText, cursor, flags);
        if (found.isNull())
            break;

        found.insertText(replaceText);
        cursor = found;
        count++;
    }

    cursor.endEditBlock();
    m_matchLabel->setText(QString("%1 replaced").arg(count));
}

void FindReplace::highlightAll()
{
    clearHighlights();

    QString searchText = m_findEdit->text();
    if (searchText.isEmpty())
        return;

    QList<QTextEdit::ExtraSelection> extras;

    auto flags = FindReplace_findFlags(m_caseBtn->isChecked());
    QTextCursor cursor(m_editor->document());

    while (true) {
        cursor = m_editor->document()->find(searchText, cursor, flags);
        if (cursor.isNull())
            break;

        QTextEdit::ExtraSelection sel;
        sel.cursor = cursor;
        sel.format.setBackground(QColor(255, 255, 0, 100));
        extras.append(sel);
    }

    m_editor->setExtraSelections(extras);
}

void FindReplace::clearHighlights()
{
    m_editor->setExtraSelections({});
}

void FindReplace::updateMatchCount()
{
    QString searchText = m_findEdit->text();
    if (searchText.isEmpty()) {
        m_matchLabel->clear();
        return;
    }

    auto flags = FindReplace_findFlags(m_caseBtn->isChecked());
    int count = 0;
    QTextCursor cursor(m_editor->document());
    while (true) {
        cursor = m_editor->document()->find(searchText, cursor, flags);
        if (cursor.isNull())
            break;
        count++;
    }

    m_matchLabel->setText(QString("%1 matches").arg(count));
}
