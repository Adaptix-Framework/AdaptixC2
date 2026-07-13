#include <ErrorIndicator.h>
#include <SyntaxStyle.h>
#include <QTextBlock>
#include <QAbstractTextDocumentLayout>

ErrorIndicator::ErrorIndicator(QTextEdit* editor, QObject* parent) : QObject(parent), m_editor(editor), m_syntaxStyle(nullptr), m_errors(){}

void ErrorIndicator::setSyntaxStyle(SyntaxStyle* style)
{
    m_syntaxStyle = style;
}

SyntaxStyle* ErrorIndicator::syntaxStyle() const
{
    return m_syntaxStyle;
}

void ErrorIndicator::setErrors(const QVector<ErrorInfo>& errors)
{
    m_errors = errors;
}

void ErrorIndicator::clearErrors()
{
    m_errors.clear();
}

QVector<ErrorIndicator::ErrorInfo> ErrorIndicator::errors() const
{
    return m_errors;
}

QVector<QTextEdit::ExtraSelection> ErrorIndicator::buildExtraSelections() const
{
    QVector<QTextEdit::ExtraSelection> selections;

    if (!m_syntaxStyle || !m_editor)
        return selections;

    auto doc = m_editor->document();

    for (auto& error : m_errors) {
        auto block = doc->findBlockByNumber(error.line - 1);
        if (!block.isValid())
            continue;

        QTextEdit::ExtraSelection selection;

        if (error.isWarning)
            selection.format = m_syntaxStyle->getFormat("Warning");
        else
            selection.format = m_syntaxStyle->getFormat("Error");

        selection.cursor = QTextCursor(block);
        selection.cursor.setPosition(block.position() + qMax(0, error.column - 1));

        int len = error.length > 0 ? error.length : block.text().length() - qMax(0, error.column - 1);
        selection.cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, len);

        selection.format.setToolTip(error.message);

        selections.append(selection);
    }

    return selections;
}
