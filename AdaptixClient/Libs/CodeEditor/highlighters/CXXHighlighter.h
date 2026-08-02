#pragma once

#include <StyleSyntaxHighlighter.h>
#include <QVector>
#include <QRegularExpression>
#include <QSet>
#include <QMap>
#include <QPointer>
#include <QTimer>

struct HighlightRule
{
    QRegularExpression pattern;
    QString formatName;
};

class CXXHighlighter : public StyleSyntaxHighlighter
{
Q_OBJECT
public:
    static constexpr int kLargeFileChars  = 256 * 1024;
    static constexpr int kLargeFileBlocks = 8000;
    static constexpr int kMaxHighlightTypes = 128;
    static constexpr int kTypeScanBatch     = 500;
    static constexpr int kRehighlightBatch  = 400;

    explicit CXXHighlighter(QTextDocument* document = nullptr);

    void setFilePath(const QString& path);
    QString filePath() const;

    void notifyDocumentLoaded();

    bool isLargeFileMode() const { return m_largeFile; }

    static void clearTypeCache();
    static QMap<QString, QSet<QString>> s_typeCache;

protected:
    void highlightBlock(const QString& text) override;

private:
    void detectDocumentScale();
    void rebuildLocalTypes() const; // small docs only (sync)
    QSet<QString> collectTypesFromText(const QString& text) const;
    void scheduleTypeScan();
    void processTypeScanBatch();
    void rebuildTypesPattern();
    void scheduleIncludeResolution();
    void scheduleProgressiveRehighlight();
    void processRehighlightBatch();

    QVector<HighlightRule> m_highlightRules;
    QVector<HighlightRule> m_simpleRules; // large-file subset
    QRegularExpression m_includePattern;
    QRegularExpression m_functionPattern;
    QRegularExpression m_defTypePattern;
    QRegularExpression m_commentStartPattern;
    QRegularExpression m_commentEndPattern;
    QRegularExpression m_typesPattern; // combined \b(Type1|Type2|...)\b

    QString m_filePath;

    mutable QSet<QString> m_localTypes;
    mutable QSet<QString> m_resolvedTypes;
    mutable QString m_lastIncludeHash;
    mutable bool m_resolvePending = false;
    mutable bool m_localTypesDirty = true;

    bool m_largeFile = false;
    int  m_typeScanBlock = 0;
    bool m_typeScanActive = false;
    bool m_typeScanDone = false;
    int  m_rehighlightBlock = 0;
    bool m_rehighlightActive = false;

    QPointer<QTimer> m_typeScanTimer;
    QPointer<QTimer> m_rehighlightTimer;
};
