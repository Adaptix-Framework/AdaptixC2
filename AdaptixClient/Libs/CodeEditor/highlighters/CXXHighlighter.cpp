#include <CXXHighlighter.h>
#include <SyntaxStyle.h>
#include <Language.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextBlock>
#include <QCryptographicHash>
#include <QBitArray>
#include <algorithm>

QMap<QString, QSet<QString>> CXXHighlighter::s_typeCache;

CXXHighlighter::CXXHighlighter(QTextDocument* document) :
    StyleSyntaxHighlighter(document),
    m_includePattern(QRegularExpression(R"(^\s*#\s*include\s*([<"][^:?"<>\|]+[">]))")),
    m_functionPattern(QRegularExpression(R"(\b([_a-zA-Z][_a-zA-Z0-9]*\s+)?((?:[_a-zA-Z][_a-zA-Z0-9]*\s*::\s*)*[_a-zA-Z][_a-zA-Z0-9]*)(?=\s*\())")),
    m_defTypePattern(QRegularExpression(R"(\b([_a-zA-Z][_a-zA-Z0-9]*)\s+[_a-zA-Z][_a-zA-Z0-9]*\s*[;=])")),
    m_commentStartPattern(QRegularExpression(R"(/\*)")),
    m_commentEndPattern(QRegularExpression(R"(\*/)"))
{
    Q_INIT_RESOURCE(codeeditor_resources);
    QFile fl(":/languages/cpp.xml");

    if (fl.open(QIODevice::ReadOnly)) {
        Language language(&fl);
        if (language.isLoaded()) {
            auto keys = language.keys();
            for (auto&& key : keys) {
                auto names = language.names(key);
                for (auto&& name : names) {
                    m_highlightRules.append({
                        QRegularExpression(QString(R"(\b%1\b)").arg(name)),
                        key
                    });
                }
            }
        }
    }

    m_highlightRules.append({
        QRegularExpression(R"((?<=\b|\s|^)(?i)(?:(?:(?:(?:(?:\d+(?:'\d+)*)?\.(?:\d+(?:'\d+)*)(?:e[+-]?(?:\d+(?:'\d+)*))?)|(?:(?:\d+(?:'\d+)*)\.(?:e[+-]?(?:\d+(?:'\d+)*))?)|(?:(?:\d+(?:'\d+)*)(?:e[+-]?(?:\d+(?:'\d+)*)))|(?:0x(?:[0-9a-f]+(?:'[0-9a-f]+)*)?\.(?:[0-9a-f]+(?:'[0-9a-f]+)*)(?:p[+-]?(?:\d+(?:'\d+)*)))|(?:0x(?:[0-9a-f]+(?:'[0-9a-f]+)*)\.?(?:p[+-]?(?:\d+(?:'\d+)*))))[lf]?)|(?:(?:(?:[1-9]\d*(?:'\d+)*)|(?:0[0-7]*(?:'[0-7]+)*)|(?:0x[0-9a-f]+(?:'[0-9a-f]+)*)|(?:0b[01]+(?:'[01]+)*))(?:u?l{0,2}|l{0,2}u?)))(?=\b|\s|$))"),
        "Number"
    });
    m_highlightRules.append({QRegularExpression(R"("[^\n"]*")"), "String"});
    m_highlightRules.append({QRegularExpression(R"('[^\n']*')"), "String"});
    m_highlightRules.append({QRegularExpression(R"(#[a-zA-Z_]+)"), "Preprocessor"});
    m_highlightRules.append({QRegularExpression(R"(//[^\n]*)"), "Comment"});
    m_highlightRules.append({
        QRegularExpression(R"(\b([A-Za-z_][A-Za-z0-9_]*)\$([A-Za-z_][A-Za-z0-9_]*))"),
        "Function"
    });
    m_highlightRules.append({
        QRegularExpression(R"(\b(?:__stdcall|__cdecl|__fastcall|__thiscall|__vectorcall|WINAPI|CALLBACK|APIENTRY|PASCAL|FARPROC|HMODULE|HINSTANCE|HRESULT|NTSTATUS|BOOL|DWORD|WORD|BYTE|LPSTR|LPCSTR|LPWSTR|LPCWSTR|HANDLE|HWND|HDC|HKEY|LPVOID|LPCVOID)\b)"),
        "Type"
    });

    m_simpleRules.append({QRegularExpression(R"(\b(?:0[xX][0-9A-Fa-f]+|\d+\.?\d*)\b)"), "Number"});
    m_simpleRules.append({QRegularExpression(R"("[^\n"]*")"), "String"});
    m_simpleRules.append({QRegularExpression(R"('[^\n']*')"), "String"});
    m_simpleRules.append({QRegularExpression(R"(#[a-zA-Z_]+)"), "Preprocessor"});
    m_simpleRules.append({QRegularExpression(R"(//[^\n]*)"), "Comment"});
    m_simpleRules.append({QRegularExpression(R"(\b(?:if|else|for|while|do|switch|case|break|continue|return|goto|sizeof|typedef|struct|class|enum|union|const|static|extern|volatile|inline|void|int|char|short|long|float|double|signed|unsigned|bool|true|false|nullptr|NULL|namespace|using|template|typename|public|private|protected|virtual|override|final|noexcept|auto|decltype|constexpr|include|define|ifdef|ifndef|endif|pragma|error|warning)\b)"), "Keyword" });
    m_simpleRules.append({QRegularExpression(R"(\b(?:BOOL|DWORD|WORD|BYTE|HANDLE|HWND|HRESULT|LPVOID|LPCVOID|LPSTR|LPCSTR|LPWSTR|LPCWSTR|NTSTATUS|HMODULE|HINSTANCE)\b)"), "Type" });

    if (document) {
        connect(document, &QTextDocument::contentsChanged, this, [this]() {
            m_localTypesDirty = true;
            if (!m_largeFile && m_typeScanDone)
                scheduleTypeScan();
        });
    }

    m_typeScanTimer = new QTimer(this);
    m_typeScanTimer->setSingleShot(true);
    connect(m_typeScanTimer, &QTimer::timeout, this, &CXXHighlighter::processTypeScanBatch);

    m_rehighlightTimer = new QTimer(this);
    m_rehighlightTimer->setSingleShot(true);
    connect(m_rehighlightTimer, &QTimer::timeout, this, &CXXHighlighter::processRehighlightBatch);
}

void CXXHighlighter::setFilePath(const QString& path)
{
    m_filePath = path;
    m_lastIncludeHash.clear();
    m_resolvedTypes.clear();
    m_localTypes.clear();
    m_localTypesDirty = true;
    m_typeScanDone = false;
    m_typeScanBlock = 0;
    m_typeScanActive = false;
    m_typesPattern = QRegularExpression();
    m_largeFile = false;
}

QString CXXHighlighter::filePath() const
{
    return m_filePath;
}

void CXXHighlighter::clearTypeCache()
{
    s_typeCache.clear();
}

void CXXHighlighter::notifyDocumentLoaded()
{
    detectDocumentScale();
    m_localTypes.clear();
    m_resolvedTypes.clear();
    m_localTypesDirty = true;
    m_typeScanDone = false;
    m_typeScanBlock = 0;
    m_typeScanActive = false;
    m_typesPattern = QRegularExpression();
    m_rehighlightActive = false;
    m_rehighlightBlock = 0;

    if (m_largeFile) {
        return;
    }

    scheduleTypeScan();
}

void CXXHighlighter::detectDocumentScale()
{
    m_largeFile = false;
    auto* doc = document();
    if (!doc)
        return;
    if (doc->characterCount() >= kLargeFileChars || doc->blockCount() >= kLargeFileBlocks)
        m_largeFile = true;
}

QSet<QString> CXXHighlighter::collectTypesFromText(const QString& text) const
{
    QSet<QString> types;

    static const QRegularExpression typeDeclPattern(R"(\b(?:class|struct|enum\s+class|enum|union|interface)\s+(?:\w+\s+)*(\w+))");
    static const QRegularExpression typedefPattern(R"(\btypedef\s+.+?\s+(\w+)\s*[;=])");
    static const QRegularExpression usingPattern(R"(\busing\s+(\w+)\s*=)");

    auto it = typeDeclPattern.globalMatch(text);
    while (it.hasNext()) {
        QString name = it.next().captured(1);
        if (!name.isEmpty() && name[0].isUpper())
            types.insert(name);
    }

    it = typedefPattern.globalMatch(text);
    while (it.hasNext())
        types.insert(it.next().captured(1));

    it = usingPattern.globalMatch(text);
    while (it.hasNext())
        types.insert(it.next().captured(1));

    return types;
}

void CXXHighlighter::rebuildLocalTypes() const
{
    if (!m_localTypesDirty || m_typeScanActive)
        return;

    m_localTypes.clear();
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        const QString trimmed = block.text().trimmed();
        if (!trimmed.startsWith(QLatin1String("//")) && !trimmed.startsWith(QLatin1String("/*")))
            m_localTypes.unite(collectTypesFromText(block.text()));
        block = block.next();
    }
    m_localTypesDirty = false;
}

void CXXHighlighter::scheduleTypeScan()
{
    if (m_largeFile || !document())
        return;
    if (m_typeScanActive)
        return;
    m_typeScanActive = true;
    m_typeScanBlock = 0;
    m_localTypes.clear();
    m_typeScanDone = false;
    if (m_typeScanTimer)
        m_typeScanTimer->start(0);
}

void CXXHighlighter::processTypeScanBatch()
{
    auto* doc = document();
    if (!doc || m_largeFile) {
        m_typeScanActive = false;
        return;
    }

    int processed = 0;
    QTextBlock block = doc->findBlockByNumber(m_typeScanBlock);
    while (block.isValid() && processed < kTypeScanBatch) {
        const QString trimmed = block.text().trimmed();
        if (!trimmed.startsWith(QLatin1String("//")) && !trimmed.startsWith(QLatin1String("/*")))
            m_localTypes.unite(collectTypesFromText(block.text()));
        block = block.next();
        ++m_typeScanBlock;
        ++processed;
    }

    if (block.isValid()) {
        if (m_typeScanTimer)
            m_typeScanTimer->start(0); // yield to event loop between batches
        return;
    }

    m_typeScanActive = false;
    m_typeScanDone = true;
    m_localTypesDirty = false;
    rebuildTypesPattern();
    scheduleIncludeResolution();
    scheduleProgressiveRehighlight();
}

void CXXHighlighter::rebuildTypesPattern()
{
    QStringList names = m_localTypes.values();
    names += m_resolvedTypes.values();
    names.removeDuplicates();
    std::sort(names.begin(), names.end());
    if (names.size() > kMaxHighlightTypes)
        names = names.mid(0, kMaxHighlightTypes);

    if (names.isEmpty()) {
        m_typesPattern = QRegularExpression();
        return;
    }

    QStringList escaped;
    escaped.reserve(names.size());
    for (const QString& n : names)
        escaped.append(QRegularExpression::escape(n));

    m_typesPattern = QRegularExpression(QStringLiteral("\\b(?:%1)\\b").arg(escaped.join(QLatin1Char('|'))));
}

static QSet<QString> parseFileTypes(const QString& filePath)
{
    auto cacheIt = CXXHighlighter::s_typeCache.find(filePath);
    if (cacheIt != CXXHighlighter::s_typeCache.end())
        return cacheIt.value();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    if (file.size() > 500000) {
        file.close();
        return {};
    }

    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    QSet<QString> types;
    static const QRegularExpression typeDeclPattern(R"(\b(?:class|struct|enum\s+class|enum|union|interface)\s+(?:\w+\s+)*(\w+))");
    static const QRegularExpression typedefPattern(R"(\btypedef\s+.+?\s+(\w+)\s*[;=])");
    static const QRegularExpression usingPattern(R"(\busing\s+(\w+)\s*=)");

    auto it = typeDeclPattern.globalMatch(content);
    while (it.hasNext()) {
        QString name = it.next().captured(1);
        if (!name.isEmpty() && name[0].isUpper())
            types.insert(name);
    }
    it = typedefPattern.globalMatch(content);
    while (it.hasNext())
        types.insert(it.next().captured(1));
    it = usingPattern.globalMatch(content);
    while (it.hasNext())
        types.insert(it.next().captured(1));

    CXXHighlighter::s_typeCache.insert(filePath, types);
    return types;
}

static void resolveRecursive(const QString& filePath, QSet<QString>& allTypes, QSet<QString>& visited, int maxDepth)
{
    if (maxDepth <= 0)
        return;

    const QString absPath = QFileInfo(filePath).absoluteFilePath();
    if (visited.contains(absPath))
        return;
    visited.insert(absPath);

    allTypes.unite(parseFileTypes(absPath));

    QFile file(absPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    if (file.size() > 500000) {
        file.close();
        return;
    }
    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    static const QRegularExpression localInclude(R"xx(#\s*include\s*"([^"]+)")xx");
    const QDir fileDir = QFileInfo(absPath).absoluteDir();
    auto incMatch = localInclude.globalMatch(content);
    while (incMatch.hasNext()) {
        const QString incPath = fileDir.absoluteFilePath(incMatch.next().captured(1));
        QFileInfo incInfo(incPath);
        if (incInfo.exists() && incInfo.isFile())
            resolveRecursive(incInfo.absoluteFilePath(), allTypes, visited, maxDepth - 1);
    }
}

void CXXHighlighter::scheduleIncludeResolution()
{
    if (m_largeFile || m_filePath.isEmpty() || m_resolvePending || !document())
        return;

    QString includeHash;
    static const QRegularExpression localInclude(R"xx(#\s*include\s*"([^"]+)")xx");
    int n = 0;
    for (QTextBlock block = document()->begin(); block.isValid() && n < 2000; block = block.next(), ++n) {
        auto match = localInclude.match(block.text());
        if (match.hasMatch())
            includeHash += match.captured(1) + QLatin1Char('\n');
    }

    const QByteArray hash = QCryptographicHash::hash(includeHash.toUtf8(), QCryptographicHash::Md5).toHex();
    if (hash == m_lastIncludeHash)
        return;
    m_lastIncludeHash = QString::fromLatin1(hash);
    m_resolvePending = true;

    QTimer::singleShot(50, this, [this]() {
        static const QRegularExpression incRe(R"xx(#\s*include\s*"([^"]+)")xx");
        QSet<QString> allTypes;
        QFileInfo currentInfo(m_filePath);
        QDir currentDir = currentInfo.absoluteDir();
        QSet<QString> visited;
        visited.insert(currentInfo.absoluteFilePath());

        int n = 0;
        if (document()) {
            for (QTextBlock block = document()->begin(); block.isValid() && n < 2000; block = block.next(), ++n) {
                auto match = incRe.match(block.text());
                if (!match.hasMatch())
                    continue;
                const QString resolved = currentDir.absoluteFilePath(match.captured(1));
                QFileInfo info(resolved);
                if (info.exists() && info.isFile())
                    resolveRecursive(info.absoluteFilePath(), allTypes, visited, 5);
            }
        }

        m_resolvedTypes = allTypes;
        m_resolvePending = false;
        rebuildTypesPattern();
        scheduleProgressiveRehighlight();
    });
}

void CXXHighlighter::scheduleProgressiveRehighlight()
{
    if (!document() || m_rehighlightActive)
        return;
    m_rehighlightActive = true;
    m_rehighlightBlock = 0;
    if (m_rehighlightTimer)
        m_rehighlightTimer->start(0);
}

void CXXHighlighter::processRehighlightBatch()
{
    auto* doc = document();
    if (!doc) {
        m_rehighlightActive = false;
        return;
    }

    int processed = 0;
    QTextBlock block = doc->findBlockByNumber(m_rehighlightBlock);
    while (block.isValid() && processed < kRehighlightBatch) {
        rehighlightBlock(block);
        block = block.next();
        ++m_rehighlightBlock;
        ++processed;
    }

    if (block.isValid()) {
        if (m_rehighlightTimer)
            m_rehighlightTimer->start(0);
        return;
    }
    m_rehighlightActive = false;
}

void CXXHighlighter::highlightBlock(const QString& text)
{
    if (!syntaxStyle())
        return;

    const QVector<HighlightRule>& rules = m_largeFile ? m_simpleRules : m_highlightRules;

    QBitArray commentMask(text.size(), false);
    {
        static const QRegularExpression lineCommentPattern(R"(//[^\n]*)");
        auto it = lineCommentPattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            for (int i = match.capturedStart(); i < match.capturedEnd(); ++i)
                commentMask.setBit(i);
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
        } else {
            mlEnd = match.capturedEnd();
        }
        for (int i = mlStart; i < mlEnd && i < text.size(); ++i)
            commentMask.setBit(i);
        mlStart = text.indexOf(m_commentStartPattern, mlEnd);
    }

    for (int i = 0; i < text.size();) {
        if (!commentMask.testBit(i)) {
            ++i;
            continue;
        }
        int start = i;
        while (i < text.size() && commentMask.testBit(i))
            ++i;
        setFormat(start, i - start, syntaxStyle()->getFormat(QStringLiteral("Comment")));
    }

    auto isInComment = [&](int start, int len) -> bool {
        const int end = qMin(start + len, text.size());
        for (int j = start; j < end; ++j) {
            if (commentMask.testBit(j))
                return true;
        }
        return false;
    };

    for (const auto& rule : rules) {
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

    if (m_largeFile)
        return;

    if (m_typesPattern.isValid() && !m_typesPattern.pattern().isEmpty()) {
        auto it = m_typesPattern.globalMatch(text);
        while (it.hasNext()) {
            auto match = it.next();
            if (isInComment(match.capturedStart(), match.capturedLength()))
                continue;
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Type")));
        }
    }

    {
        auto matchIterator = m_includePattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            auto match = matchIterator.next();
            if (isInComment(match.capturedStart(), match.capturedLength()))
                continue;
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Preprocessor")));
            setFormat(match.capturedStart(1), match.capturedLength(1), syntaxStyle()->getFormat(QStringLiteral("String")));
        }
    }

    {
        auto matchIterator = m_functionPattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            auto match = matchIterator.next();
            if (isInComment(match.capturedStart(), match.capturedLength()))
                continue;
            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat(QStringLiteral("Type")));
            setFormat(match.capturedStart(2), match.capturedLength(2), syntaxStyle()->getFormat(QStringLiteral("Function")));
        }
    }

    {
        auto matchIterator = m_defTypePattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            auto match = matchIterator.next();
            if (isInComment(match.capturedStart(), match.capturedLength()))
                continue;
            setFormat(match.capturedStart(1), match.capturedLength(1), syntaxStyle()->getFormat(QStringLiteral("Type")));
        }
    }
}
