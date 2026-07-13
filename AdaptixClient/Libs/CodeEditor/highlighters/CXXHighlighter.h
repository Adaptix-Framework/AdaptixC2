#pragma once

#include <StyleSyntaxHighlighter.h>
#include <QVector>
#include <QRegularExpression>
#include <QSet>
#include <QMap>

struct HighlightRule
{
    QRegularExpression pattern;
    QString formatName;
};

class CXXHighlighter : public StyleSyntaxHighlighter
{
Q_OBJECT
    void rebuildLocalTypes() const;
    QSet<QString> collectTypesFromText(const QString& text) const;
    void scheduleIncludeResolution() const;

    QVector<HighlightRule> m_highlightRules;
    QRegularExpression m_includePattern;
    QRegularExpression m_functionPattern;
    QRegularExpression m_defTypePattern;
    QRegularExpression m_commentStartPattern;
    QRegularExpression m_commentEndPattern;

    QString m_filePath;

    mutable QSet<QString> m_localTypes;
    mutable QSet<QString> m_resolvedTypes;
    mutable QString m_lastIncludeHash;
    mutable bool m_resolvePending;
    mutable bool m_localTypesDirty;

public:
    explicit CXXHighlighter(QTextDocument* document = nullptr);

    void setFilePath(const QString& path);
    QString filePath() const;

    static void clearTypeCache();
    static QMap<QString, QSet<QString>> s_typeCache;

protected:
    void highlightBlock(const QString& text) override;
};
