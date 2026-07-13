#include <Client/FilterExprParser.h>
#include <QString>

bool TermNode::evaluate(const QString& rowData) const
{
    return rowData.contains(value, Qt::CaseInsensitive);
}

bool AndNode::evaluate(const QString& rowData) const
{
    return left->evaluate(rowData) && right->evaluate(rowData);
}

bool OrNode::evaluate(const QString& rowData) const
{
    return left->evaluate(rowData) || right->evaluate(rowData);
}

bool NotNode::evaluate(const QString& rowData) const
{
    return !child->evaluate(rowData);
}

namespace {

struct Parser {
    const QChar* data;
    int          len;
    int          pos;
    int          depth;

    explicit Parser(const QString& s)
        : data(s.constData()), len(s.length()), pos(0), depth(0) {}

    void skipWS() {
        while (pos < len && data[pos].isSpace())
            ++pos;
    }

    //  ('&' | '|') term
    std::unique_ptr<ExprNode> parseExpr() {
        if (++depth > 16) { --depth; return nullptr; }

        auto left = parseTerm();
        if (!left) { --depth; return nullptr; }

        while (true) {
            skipWS();
            if (pos >= len) break;
            QChar c = data[pos];
            if (c != '&' && c != '|') break;
            ++pos;

            auto right = parseTerm();
            if (!right) { --depth; return nullptr; }

            if (c == '&')
                left = std::make_unique<AndNode>(std::move(left), std::move(right));
            else
                left = std::make_unique<OrNode>(std::move(left), std::move(right));
        }

        --depth;
        return left;
    }

    // '^' '(' expr ')' | '(' expr ')' | word
    std::unique_ptr<ExprNode> parseTerm() {
        skipWS();
        if (pos >= len) return nullptr;

        QChar c = data[pos];

        if (c == '^') {
            ++pos;
            skipWS();
            if (pos >= len || data[pos] != '(') return nullptr;
            ++pos;
            auto child = parseExpr();
            if (!child) return nullptr;
            skipWS();
            if (pos >= len || data[pos] != ')') return nullptr;
            ++pos;
            return std::make_unique<NotNode>(std::move(child));
        }

        if (c == '(') {
            ++pos;
            auto child = parseExpr();
            if (!child) return nullptr;
            skipWS();
            if (pos >= len || data[pos] != ')') return nullptr;
            ++pos;
            return child;
        }

        return parseWord();
    }

    std::unique_ptr<ExprNode> parseWord() {
        skipWS();
        int start = pos;
        while (pos < len) {
            QChar ch = data[pos];
            if (ch.isSpace() || ch == '&' || ch == '|' || ch == '^' || ch == '(' || ch == ')')
                break;
            ++pos;
        }
        if (pos == start) return nullptr;
        return std::make_unique<TermNode>(QString(data + start, pos - start));
    }
};

}

std::unique_ptr<ExprNode> FilterExprParser::compile(const QString& text)
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.length() > 512)
        return nullptr;

    Parser p(trimmed);
    auto node = p.parseExpr();
    p.skipWS();
    if (p.pos < p.len)
        return nullptr;

    return node;
}
