package filter

import (
	"errors"
	"fmt"
	"strings"
	"unicode"
)

const (
	maxExprLen = 512
	maxDepth   = 16
)

type FilterNode interface {
	toSQL(cols []string, args *[]any) string
}

func ParseFilterExpr(expr string) (FilterNode, error) {
	expr = strings.TrimSpace(expr)
	if expr == "" {
		return nil, nil
	}
	if len([]rune(expr)) > maxExprLen {
		return nil, fmt.Errorf("filter: expression too long (max %d)", maxExprLen)
	}

	p := &parser{runes: []rune(expr)}
	node, err := p.parseExpr(0)
	if err != nil {
		return nil, err
	}
	if p.pos < len(p.runes) {
		return nil, fmt.Errorf("filter: unexpected character %q at position %d", string(p.runes[p.pos]), p.pos)
	}
	return node, nil
}

func ToSQL(node FilterNode, cols []string) (string, []any) {
	if node == nil || len(cols) == 0 {
		return "", nil
	}
	var args []any
	sql := node.toSQL(cols, &args)
	return sql, args
}

type termNode struct{ value string }

func (n *termNode) toSQL(cols []string, args *[]any) string {
	if len(cols) == 0 {
		return "1=0"
	}
	parts := make([]string, len(cols))
	for i, col := range cols {
		if strings.ContainsAny(col, " (") {
			parts[i] = col + ` LIKE ?`
		} else {
			parts[i] = `"` + col + `" LIKE ?`
		}
		*args = append(*args, "%"+n.value+"%")
	}
	if len(parts) == 1 {
		return parts[0]
	}
	return "(" + strings.Join(parts, " OR ") + ")"
}

type andNode struct{ left, right FilterNode }

func (n *andNode) toSQL(cols []string, args *[]any) string {
	return "(" + n.left.toSQL(cols, args) + " AND " + n.right.toSQL(cols, args) + ")"
}

type orNode struct{ left, right FilterNode }

func (n *orNode) toSQL(cols []string, args *[]any) string {
	return "(" + n.left.toSQL(cols, args) + " OR " + n.right.toSQL(cols, args) + ")"
}

type notNode struct{ child FilterNode }

func (n *notNode) toSQL(cols []string, args *[]any) string {
	return "(NOT " + n.child.toSQL(cols, args) + ")"
}

type parser struct {
	runes []rune
	pos   int
}

var (
	errEmptyExpr     = errors.New("filter: empty expression inside group")
	errUnclosedParen = errors.New("filter: unclosed '('")
	errDepthExceeded = fmt.Errorf("filter: nesting depth exceeds %d", maxDepth)
)

func (p *parser) peek() (rune, bool) {
	p.skipWS()
	if p.pos >= len(p.runes) {
		return 0, false
	}
	return p.runes[p.pos], true
}

func (p *parser) skipWS() {
	for p.pos < len(p.runes) && unicode.IsSpace(p.runes[p.pos]) {
		p.pos++
	}
}

func (p *parser) parseExpr(depth int) (FilterNode, error) {
	if depth > maxDepth {
		return nil, errDepthExceeded
	}

	left, err := p.parseTerm(depth)
	if err != nil {
		return nil, err
	}

	for {
		ch, ok := p.peek()
		if !ok {
			break
		}
		if ch == ')' {
			break
		}
		op := '&'
		if ch == '&' || ch == '|' {
			op = ch
			p.pos++
		}
		right, err := p.parseTerm(depth)
		if err != nil {
			return nil, err
		}

		if op == '&' {
			left = &andNode{left, right}
		} else {
			left = &orNode{left, right}
		}
	}

	return left, nil
}

func (p *parser) parseTerm(depth int) (FilterNode, error) {
	ch, ok := p.peek()
	if !ok {
		return nil, errEmptyExpr
	}

	if ch == '^' {
		p.pos++
		next, ok2 := p.peek()
		if !ok2 || next != '(' {
			return nil, errors.New("filter: expected '(' after '^'")
		}
		p.pos++
		child, err := p.parseExpr(depth + 1)
		if err != nil {
			return nil, err
		}
		if err := p.expectCloseParen(); err != nil {
			return nil, err
		}
		return &notNode{child}, nil
	}

	if ch == '(' {
		p.pos++
		child, err := p.parseExpr(depth + 1)
		if err != nil {
			return nil, err
		}
		if err := p.expectCloseParen(); err != nil {
			return nil, err
		}
		return child, nil
	}

	return p.parseWord()
}

func (p *parser) expectCloseParen() error {
	p.skipWS()
	if p.pos >= len(p.runes) || p.runes[p.pos] != ')' {
		return errUnclosedParen
	}
	p.pos++
	return nil
}

func (p *parser) parseWord() (FilterNode, error) {
	p.skipWS()
	start := p.pos
	for p.pos < len(p.runes) {
		ch := p.runes[p.pos]
		if unicode.IsSpace(ch) || ch == '&' || ch == '|' || ch == '^' || ch == '(' || ch == ')' {
			break
		}
		p.pos++
	}
	if p.pos == start {
		if p.pos < len(p.runes) {
			return nil, fmt.Errorf("filter: unexpected character %q", string(p.runes[p.pos]))
		}
		return nil, errEmptyExpr
	}
	return &termNode{value: string(p.runes[start:p.pos])}, nil
}
