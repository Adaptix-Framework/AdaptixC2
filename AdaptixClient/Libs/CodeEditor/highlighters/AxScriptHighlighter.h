#pragma once

#include <StyleSyntaxHighlighter.h>
#include <QVector>
#include <QRegularExpression>

struct AxHighlightRule
{
    QRegularExpression pattern;
    QString formatName;
};

class AxScriptHighlighter : public StyleSyntaxHighlighter
{
Q_OBJECT
    QVector<AxHighlightRule> m_highlightRules;
    QRegularExpression m_commentStartPattern;
    QRegularExpression m_commentEndPattern;
    QRegularExpression m_regexPattern;
    QString m_filePath;

public:
    explicit AxScriptHighlighter(QTextDocument* document = nullptr);

    void setFilePath(const QString& path);
    QString filePath() const;

protected:
    void highlightBlock(const QString& text) override;
};
