#include <AxScriptHighlighter.h>
#include <SyntaxStyle.h>
#include <Language.h>
#include <QFile>

AxScriptHighlighter::AxScriptHighlighter(QTextDocument* document) :
    StyleSyntaxHighlighter(document),
    m_highlightRules(),
    m_commentStartPattern(QRegularExpression(R"(/\*)")),
    m_commentEndPattern(QRegularExpression(R"(\*/)")),
    m_regexPattern(QRegularExpression(R"((?<!\w)(/[^/\n*][^/\n]*/[gimsuvy]?))"))
{
    Q_INIT_RESOURCE(codeeditor_resources);
    QFile fl(":/languages/axscript.xml");

    if (!fl.open(QIODevice::ReadOnly))
        return;

    Language language(&fl);

    if (!language.isLoaded())
        return;

    auto keys = language.keys();
    for (auto&& key : keys) {
        auto names = language.names(key);
        for (auto&& name : names) {
            if (name.contains('.')) {
                m_highlightRules.append({
                    QRegularExpression(QString(R"(\b%1\b)").arg(QRegularExpression::escape(name))),
                    key
                });
            }
            else {
                m_highlightRules.append({
                    QRegularExpression(QString(R"(\b%1\b)").arg(name)),
                    key
                });
            }
        }
    }

    // Numbers
    m_highlightRules.append({
        QRegularExpression(R"(\b(?:0[xX][0-9a-fA-F]+|0[oO][0-7]+|0[bB][01]+|\d+\.?\d*(?:[eE][+-]?\d+)?)\b)"),
        "Number"
    });

    // Template literals `${...}`
    m_highlightRules.append({
        QRegularExpression(R"(`[^`]*`)"),
        "String"
    });

    // Double-quoted strings
    m_highlightRules.append({
        QRegularExpression(R"("[^\n"]*")"),
        "String"
    });

    // Single-quoted strings
    m_highlightRules.append({
        QRegularExpression(R"('[^\n']*')"),
        "String"
    });

    // Single-line comments
    m_highlightRules.append({
        QRegularExpression(R"(//[^\n]*)"),
        "Comment"
    });

    // Arrow functions
    m_highlightRules.append({
        QRegularExpression(R"(=>)"),
        "Operator"
    });
}

void AxScriptHighlighter::setFilePath(const QString& path)
{
    m_filePath = path;
}

QString AxScriptHighlighter::filePath() const
{
    return m_filePath;
}

void AxScriptHighlighter::highlightBlock(const QString& text)
{
    QSet<int> commentPositions;

    {
        QRegularExpression lineCommentPattern(R"(//[^\n]*)");
        auto it = lineCommentPattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            for (int i = match.capturedStart(); i < match.capturedEnd(); ++i)
                commentPositions.insert(i);
        }
    }

    setCurrentBlockState(0);
    int mlStart = 0;
    if (previousBlockState() != 1)
        mlStart = text.indexOf(m_commentStartPattern);

    while (mlStart >= 0) {
        auto match = m_commentEndPattern.match(text, mlStart);
        int mlEnd;
        if (match.capturedStart() == -1) {
            setCurrentBlockState(1);
            mlEnd = text.length();
        }
        else {
            mlEnd = match.capturedEnd();
        }
        for (int i = mlStart; i < mlEnd; ++i)
            commentPositions.insert(i);
        mlStart = text.indexOf(m_commentStartPattern, mlEnd);
    }

    QList<int> sorted = commentPositions.values();
    std::sort(sorted.begin(), sorted.end());
    int i = 0;
    while (i < sorted.size()) {
        int start = sorted[i];
        int end = start;
        while (i + 1 < sorted.size() && sorted[i + 1] == end + 1) {
            ++i;
            ++end;
        }
        setFormat(start, end - start + 1, syntaxStyle()->getFormat("Comment"));
        ++i;
    }

    auto isInComment = [&](int start, int len) -> bool {
        for (int j = start; j < start + len; ++j)
            if (commentPositions.contains(j))
                return true;
        return false;
    };

    for (auto& rule : m_highlightRules) {
        if (rule.formatName == "Comment")
            continue;

        auto matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            auto match = matchIterator.next();
            if (isInComment(match.capturedStart(), match.capturedLength()))
                continue;

            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(rule.formatName));
        }
    }

    {
        auto it = m_regexPattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            if (isInComment(match.capturedStart(), match.capturedLength()))
                continue;

            auto existingFormat = format(match.capturedStart());
            if (existingFormat.hasProperty(QTextFormat::ForegroundBrush) && existingFormat.foreground().style() != Qt::NoBrush)
                continue;

            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat("String"));
        }
    }
}
