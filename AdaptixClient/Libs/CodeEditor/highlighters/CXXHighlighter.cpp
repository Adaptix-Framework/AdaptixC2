#include <CXXHighlighter.h>
#include <SyntaxStyle.h>
#include <Language.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextBlock>
#include <QTimer>
#include <QCryptographicHash>

QMap<QString, QSet<QString>> CXXHighlighter::s_typeCache;

CXXHighlighter::CXXHighlighter(QTextDocument* document) :
    StyleSyntaxHighlighter(document),
    m_highlightRules(),
    m_includePattern(QRegularExpression(R"(^\s*#\s*include\s*([<"][^:?"<>\|]+[">]))")),
    m_functionPattern(QRegularExpression(R"(\b([_a-zA-Z][_a-zA-Z0-9]*\s+)?((?:[_a-zA-Z][_a-zA-Z0-9]*\s*::\s*)*[_a-zA-Z][_a-zA-Z0-9]*)(?=\s*\())")),
    m_defTypePattern(QRegularExpression(R"(\b([_a-zA-Z][_a-zA-Z0-9]*)\s+[_a-zA-Z][_a-zA-Z0-9]*\s*[;=])")),
    m_commentStartPattern(QRegularExpression(R"(/\*)")),
    m_commentEndPattern(QRegularExpression(R"(\*/)")),
    m_resolvePending(false),
    m_localTypesDirty(true)
{
    Q_INIT_RESOURCE(codeeditor_resources);
    QFile fl(":/languages/cpp.xml");

    if (!fl.open(QIODevice::ReadOnly))
        return;

    Language language(&fl);

    if (!language.isLoaded())
        return;

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

    // Rebuild local types when document changes
    connect(document, &QTextDocument::contentsChanged, this, [this]() {
        m_localTypesDirty = true;
    });
}

void CXXHighlighter::setFilePath(const QString& path)
{
    m_filePath = path;
    m_lastIncludeHash.clear();
    m_resolvedTypes.clear();
    m_localTypesDirty = true;
}

QString CXXHighlighter::filePath() const
{
    return m_filePath;
}

void CXXHighlighter::clearTypeCache()
{
    s_typeCache.clear();
}

QSet<QString> CXXHighlighter::collectTypesFromText(const QString& text) const
{
    QSet<QString> types;

    QRegularExpression typeDeclPattern( R"(\b(?:class|struct|enum\s+class|enum|union)\s+(?:\w+\s+)*(\w+))" );
    QRegularExpression typedefPattern( R"(\btypedef\s+.+?\s+(\w+)\s*[;=])" );
    QRegularExpression usingPattern( R"(\busing\s+(\w+)\s*=)" );

    auto it = typeDeclPattern.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        QString name = match.captured(1);
        if (!name.isEmpty() && name[0].isUpper())
            types.insert(name);
    }

    it = typedefPattern.globalMatch(text);
    while (it.hasNext()) {
        types.insert(it.next().captured(1));
    }

    it = usingPattern.globalMatch(text);
    while (it.hasNext()) {
        types.insert(it.next().captured(1));
    }

    return types;
}

void CXXHighlighter::rebuildLocalTypes() const
{
    if (!m_localTypesDirty)
        return;

    m_localTypes.clear();
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        QString trimmed = block.text().trimmed();
        if (!trimmed.startsWith("//") && !trimmed.startsWith("/*"))
            m_localTypes.unite(collectTypesFromText(block.text()));
        block = block.next();
    }
    m_localTypesDirty = false;
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

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    QSet<QString> types;

    QRegularExpression typeDeclPattern( R"(\b(?:class|struct|enum\s+class|enum|union)\s+(?:\w+\s+)*(\w+))" );
    QRegularExpression typedefPattern( R"(\btypedef\s+.+?\s+(\w+)\s*[;=])" );
    QRegularExpression usingPattern( R"(\busing\s+(\w+)\s*=)" );

    auto it = typeDeclPattern.globalMatch(content);
    while (it.hasNext()) {
        auto match = it.next();
        QString name = match.captured(1);
        if (!name.isEmpty() && name[0].isUpper())
            types.insert(name);
    }

    it = typedefPattern.globalMatch(content);
    while (it.hasNext()) {
        types.insert(it.next().captured(1));
    }

    it = usingPattern.globalMatch(content);
    while (it.hasNext()) {
        types.insert(it.next().captured(1));
    }

    CXXHighlighter::s_typeCache.insert(filePath, types);
    return types;
}

static void resolveRecursive( const QString& filePath, QSet<QString>& allTypes, QSet<QString>& visited, int maxDepth)
{
    if (maxDepth <= 0)
        return;

    QString absPath = QFileInfo(filePath).absoluteFilePath();
    if (visited.contains(absPath))
        return;

    visited.insert(absPath);

    QSet<QString> types = parseFileTypes(absPath);
    allTypes.unite(types);

    QFile file(absPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    if (file.size() > 500000) {
        file.close();
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    QRegularExpression localInclude(R"xx(#\s*include\s*"([^"]+)")xx");
    QDir fileDir = QFileInfo(absPath).absoluteDir();

    auto incMatch = localInclude.globalMatch(content);
    while (incMatch.hasNext()) {
        auto match = incMatch.next();
        QString incPath = fileDir.absoluteFilePath(match.captured(1));
        QFileInfo incInfo(incPath);
        if (incInfo.exists() && incInfo.isFile())
            resolveRecursive(incInfo.absoluteFilePath(), allTypes, visited, maxDepth - 1);
    }
}

void CXXHighlighter::scheduleIncludeResolution() const
{
    if (m_filePath.isEmpty() || m_resolvePending)
        return;

    QString includeHash;
    QRegularExpression localInclude(R"xx(#\s*include\s*"([^"]+)")xx");
    QTextBlock block = document()->begin();
    while (block.isValid()) {
        auto match = localInclude.match(block.text());
        if (match.hasMatch())
            includeHash += match.captured(1) + "\n";
        block = block.next();
    }

    QByteArray hash = QCryptographicHash::hash( includeHash.toUtf8(), QCryptographicHash::Md5 ).toHex();

    if (hash == m_lastIncludeHash)
        return;

    m_lastIncludeHash = hash;
    m_resolvePending = true;

    auto* self = const_cast<CXXHighlighter*>(this);
    QTimer::singleShot(50, self, [self, localInclude]() {
        QSet<QString> allTypes;

        QFileInfo currentInfo(self->m_filePath);
        QDir currentDir = currentInfo.absoluteDir();
        QSet<QString> visited;
        visited.insert(currentInfo.absoluteFilePath());

        QTextBlock block = self->document()->begin();
        while (block.isValid()) {
            auto match = localInclude.match(block.text());
            if (match.hasMatch()) {
                QString resolved = currentDir.absoluteFilePath(match.captured(1));
                QFileInfo info(resolved);
                if (info.exists() && info.isFile())
                    resolveRecursive(info.absoluteFilePath(), allTypes, visited, 10);
            }
            block = block.next();
        }

        self->m_resolvedTypes = allTypes;
        self->m_resolvePending = false;
        self->rehighlight();
    });
}

void CXXHighlighter::highlightBlock(const QString& text)
{
    if (!syntaxStyle())
        return;

    rebuildLocalTypes();

    QSet<QString> allTypes = m_localTypes;
    allTypes.unite(m_resolvedTypes);

    scheduleIncludeResolution();

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
        for (int i = start; i < start + len; ++i)
            if (commentPositions.contains(i))
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

    for (const auto& typeName : allTypes)
    {
        QRegularExpression typeRegex(QString(R"(\b%1\b)").arg(QRegularExpression::escape(typeName)));
        auto matchIterator = typeRegex.globalMatch(text);
        while (matchIterator.hasNext()) {
            auto match = matchIterator.next();
            if (isInComment(match.capturedStart(), match.capturedLength()))
                continue;

            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat("Type"));
        }
    }

    {
        auto matchIterator = m_includePattern.globalMatch(text);
        while (matchIterator.hasNext())
        {
            auto match = matchIterator.next();
            if (isInComment(match.capturedStart(), match.capturedLength()))
                continue;

            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat("Preprocessor"));
            setFormat(match.capturedStart(1), match.capturedLength(1), syntaxStyle()->getFormat("String"));
        }
    }

    // DLL$Function
    {
        QRegularExpression dllFuncPattern(R"(\b([A-Za-z_][A-Za-z0-9_]*)\$([A-Za-z_][A-Za-z0-9_]*))");
        auto matchIterator = dllFuncPattern.globalMatch(text);
        while (matchIterator.hasNext())
        {
            auto match = matchIterator.next();
            if (isInComment(match.capturedStart(), match.capturedLength()))
                continue;

            setFormat(match.capturedStart(1), match.capturedLength(1), syntaxStyle()->getFormat("Type"));
            setFormat(match.capturedStart(1) + match.capturedLength(1), 1, syntaxStyle()->getFormat("Operator"));
            setFormat(match.capturedStart(2), match.capturedLength(2), syntaxStyle()->getFormat("Function"));
        }
    }

    // FFI declarations
    {
        QRegularExpression ffiPattern( R"(^(\s*)([A-Za-z_][A-Za-z0-9_]*)\$([A-Za-z_][A-Za-z0-9_]*)(\s*:\s*)(cdecl|stdcall)(\s+)(void|i8|i16|u16|i32|u32|i64|u64|ptr|cstr|size_t)(\s*\())");
        auto match = ffiPattern.match(text);
        if (match.hasMatch() && !isInComment(match.capturedStart(), match.capturedLength()))
        {
            setFormat(match.capturedStart(2), match.capturedLength(2), syntaxStyle()->getFormat("Type"));
            setFormat(match.capturedStart(2) + match.capturedLength(2), 1, syntaxStyle()->getFormat("Operator"));
            setFormat(match.capturedStart(3), match.capturedLength(3), syntaxStyle()->getFormat("Function"));
            setFormat(match.capturedStart(5), match.capturedLength(5), syntaxStyle()->getFormat("Keyword"));
            setFormat(match.capturedStart(7), match.capturedLength(7), syntaxStyle()->getFormat("PrimitiveType"));

            QRegularExpression argTypePattern(R"(\b(void|i8|i16|u16|i32|u32|i64|u64|ptr|cstr|size_t)\b)");
            int parenStart = text.indexOf('(', match.capturedEnd(7));
            if (parenStart >= 0)
            {
                QString argsSection = text.mid(parenStart);
                auto argIt = argTypePattern.globalMatch(argsSection);
                while (argIt.hasNext())
                {
                    auto am = argIt.next();
                    if (!isInComment(parenStart + am.capturedStart(), am.capturedLength()))
                        setFormat(parenStart + am.capturedStart(), am.capturedLength(), syntaxStyle()->getFormat("PrimitiveType"));
                }
            }
        }
    }

    // Regular function calls
    {
        auto matchIterator = m_functionPattern.globalMatch(text);
        while (matchIterator.hasNext())
        {
            auto match = matchIterator.next();
            if (isInComment(match.capturedStart(), match.capturedLength()))
                continue;

            setFormat(match.capturedStart(), match.capturedLength(), syntaxStyle()->getFormat("Type"));
            setFormat(match.capturedStart(2), match.capturedLength(2), syntaxStyle()->getFormat("Function"));
        }
    }

    // Type definitions
    {
        auto matchIterator = m_defTypePattern.globalMatch(text);
        while (matchIterator.hasNext())
        {
            auto match = matchIterator.next();
            if (isInComment(match.capturedStart(), match.capturedLength()))
                continue;

            setFormat(match.capturedStart(1), match.capturedLength(1), syntaxStyle()->getFormat("Type"));
        }
    }
}
