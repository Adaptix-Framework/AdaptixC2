#pragma once

#include <QObject>
#include <QTextEdit>
#include <QVector>
#include <QColor>

class SyntaxStyle;

class ErrorIndicator : public QObject
{
Q_OBJECT

public:
    struct ErrorInfo
    {
        int line;
        int column;
        int length;
        QString message;
        bool isWarning;
    };

    explicit ErrorIndicator(QTextEdit* editor, QObject* parent = nullptr);

    void setSyntaxStyle(SyntaxStyle* style);
    SyntaxStyle* syntaxStyle() const;

    void setErrors(const QVector<ErrorInfo>& errors);
    void clearErrors();
    QVector<ErrorInfo> errors() const;

    QVector<QTextEdit::ExtraSelection> buildExtraSelections() const;

private:
    QTextEdit* m_editor;
    SyntaxStyle* m_syntaxStyle;
    QVector<ErrorInfo> m_errors;
};
