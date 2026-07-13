#ifndef ADAPTIXCLIENT_FILTEREXPRPARSER_H
#define ADAPTIXCLIENT_FILTEREXPRPARSER_H

#include <QString>
#include <memory>

struct ExprNode {
    virtual ~ExprNode() = default;
    virtual bool evaluate(const QString& rowData) const = 0;
};

struct TermNode : ExprNode {
    QString value;
    explicit TermNode(QString v) : value(std::move(v)) {}
    bool evaluate(const QString& rowData) const override;
};

struct AndNode : ExprNode {
    std::unique_ptr<ExprNode> left, right;
    AndNode(std::unique_ptr<ExprNode> l, std::unique_ptr<ExprNode> r)
        : left(std::move(l)), right(std::move(r)) {}
    bool evaluate(const QString& rowData) const override;
};

struct OrNode : ExprNode {
    std::unique_ptr<ExprNode> left, right;
    OrNode(std::unique_ptr<ExprNode> l, std::unique_ptr<ExprNode> r)
        : left(std::move(l)), right(std::move(r)) {}
    bool evaluate(const QString& rowData) const override;
};

struct NotNode : ExprNode {
    std::unique_ptr<ExprNode> child;
    explicit NotNode(std::unique_ptr<ExprNode> c) : child(std::move(c)) {}
    bool evaluate(const QString& rowData) const override;
};

class FilterExprParser {
public:
    static std::unique_ptr<ExprNode> compile(const QString& text);
};

#endif
