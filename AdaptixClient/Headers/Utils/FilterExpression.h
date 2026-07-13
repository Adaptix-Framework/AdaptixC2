#ifndef ADAPTIXCLIENT_FILTEREXPRESSION_H
#define ADAPTIXCLIENT_FILTEREXPRESSION_H

#include <QRegularExpression>
#include <QString>
#include <memory>

class FilterExpression
{
    struct Node {
        virtual ~Node() = default;
        virtual bool eval(const QString& rowData) const = 0;
    };

    struct OrNode : Node {
        std::unique_ptr<Node> left, right;
        OrNode(std::unique_ptr<Node> l, std::unique_ptr<Node> r) : left(std::move(l)), right(std::move(r)) {}
        bool eval(const QString& rowData) const override { return left->eval(rowData) || right->eval(rowData); }
    };

    struct AndNode : Node {
        std::unique_ptr<Node> left, right;
        AndNode(std::unique_ptr<Node> l, std::unique_ptr<Node> r) : left(std::move(l)), right(std::move(r)) {}
        bool eval(const QString& rowData) const override { return left->eval(rowData) && right->eval(rowData); }
    };

    struct NotNode : Node {
        std::unique_ptr<Node> child;
        explicit NotNode(std::unique_ptr<Node> c) : child(std::move(c)) {}
        bool eval(const QString& rowData) const override { return !child->eval(rowData); }
    };

    struct TermNode : Node {
        QRegularExpression regex;
        explicit TermNode(const QString& term)
            : regex(QRegularExpression::escape(term), QRegularExpression::CaseInsensitiveOption) {}
        bool eval(const QString& rowData) const override { return rowData.contains(regex); }
    };

    static std::unique_ptr<Node> parse(QString e) {
        e = e.trimmed();
        if (e.isEmpty()) return nullptr;

        int depth = 0, lastOr = -1;
        for (int i = e.length() - 1; i >= 0; --i) {
            QChar c = e[i];
            if      (c == ')') ++depth;
            else if (c == '(') --depth;
            else if (depth == 0 && c == '|') { lastOr = i; break; }
        }
        if (lastOr != -1) {
            auto l = parse(e.left(lastOr));
            auto r = parse(e.mid(lastOr + 1));
            if (!l) return r;
            if (!r) return l;
            return std::make_unique<OrNode>(std::move(l), std::move(r));
        }

        depth = 0;
        int lastAnd = -1;
        for (int i = e.length() - 1; i >= 0; --i) {
            QChar c = e[i];
            if      (c == ')') ++depth;
            else if (c == '(') --depth;
            else if (depth == 0 && c == '&') { lastAnd = i; break; }
        }
        if (lastAnd != -1) {
            auto l = parse(e.left(lastAnd));
            auto r = parse(e.mid(lastAnd + 1));
            if (!l) return r;
            if (!r) return l;
            return std::make_unique<AndNode>(std::move(l), std::move(r));
        }

        if (e.startsWith("^(") && e.endsWith(')')) {
            auto child = parse(e.mid(2, e.length() - 3));
            if (!child) return nullptr;
            return std::make_unique<NotNode>(std::move(child));
        }

        if (e.startsWith('(') && e.endsWith(')')) {
            return parse(e.mid(1, e.length() - 2));
        }

        return std::make_unique<TermNode>(e);
    }

    std::unique_ptr<Node> root_;

public:
    FilterExpression() = default;

    void compile(const QString& expr) { root_ = parse(expr); }
    void clear() { root_.reset(); }

    bool isEmpty() const { return !root_; }

    bool evaluate(const QString& rowData) const {
        return root_ ? root_->eval(rowData) : true;
    }
};

#endif
