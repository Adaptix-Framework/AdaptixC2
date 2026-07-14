#ifndef ADAPTIXCLIENT_DELEGATES_H
#define ADAPTIXCLIENT_DELEGATES_H

#include <main.h>
#include <QProxyStyle>
#include <QTextLayout>
#include <QToolTip>
#include <QTreeView>

class ListDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QLineEdit* editor = new QLineEdit(parent);
        editor->setContentsMargins(1, 1, 1, 1);
        return editor;
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        return QSize(size.width(), size.height() + 4);
    }
};


class PaddingDelegate : public QStyledItemDelegate {
public:
    explicit PaddingDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent), m_padding(4) {}
    explicit PaddingDelegate(int padding, QObject* parent = nullptr) : QStyledItemDelegate(parent), m_padding(padding) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        int hPad = (index.column() == 0) ? m_padding : m_padding * 2;

        int indentExtra = 0;
        if (index.column() == 0) {
            if (const auto* tree = qobject_cast<const QTreeView*>(option.widget)) {
                int depth = 0;
                for (QModelIndex p = index.parent(); p.isValid(); p = p.parent())
                    ++depth;
                const int steps = tree->rootIsDecorated() ? (depth + 1) : depth;
                indentExtra = steps * tree->indentation();
            }
        }

        return QSize(size.width() + hPad + indentExtra, size.height() + m_padding);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QVariant bgVar = index.data(Qt::BackgroundRole);
        if (bgVar.isValid()) {
            painter->fillRect(opt.rect, bgVar.value<QBrush>());
            opt.backgroundBrush = Qt::NoBrush;
            opt.state &= ~QStyle::State_MouseOver;
        }

        opt.state &= ~QStyle::State_HasFocus;

        const QWidget* widget = option.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

        if (!opt.icon.isNull()) {
            QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, widget);
            opt.icon.paint(painter, iconRect, opt.decorationAlignment, opt.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled, opt.state & QStyle::State_Open ? QIcon::On : QIcon::Off);
        }

        if (!opt.text.isEmpty()) {
            QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
            textRect.adjust(m_padding, 0, -m_padding, 0);
            painter->save();

            if (opt.state & QStyle::State_Selected)
                painter->setPen(opt.palette.highlightedText().color());
            else {
                QVariant fgVar = index.data(Qt::ForegroundRole);
                if (fgVar.isValid())
                    painter->setPen(fgVar.value<QColor>());
                else
                    painter->setPen(opt.palette.text().color());
            }

            painter->setFont(opt.font);
            QString elidedText = opt.fontMetrics.elidedText(opt.text, Qt::ElideRight, textRect.width());
            painter->drawText(textRect, Qt::AlignVCenter | int(opt.displayAlignment & Qt::AlignHorizontal_Mask), elidedText);
            painter->restore();
        }
    }

protected:
    int m_padding;
};

class WrapAnywhereDelegate : public PaddingDelegate {
    static constexpr int maxLines = 5;
public:
    explicit WrapAnywhereDelegate(QObject* parent = nullptr) : PaddingDelegate(parent) {}
    explicit WrapAnywhereDelegate(int padding, QObject* parent = nullptr) : PaddingDelegate(padding, parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Background (same as PaddingDelegate)
        QVariant bgVar = index.data(Qt::BackgroundRole);
        if (bgVar.isValid())
            painter->fillRect(opt.rect, bgVar.value<QBrush>());

        opt.state &= ~QStyle::State_HasFocus;

        // Draw everything except text using style
        const QWidget* widget = option.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();

        // Draw background/selection
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

        // Draw text with WrapAnywhere (max 5 lines)
        painter->save();
        QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
        textRect.adjust(m_padding, 0, -m_padding, 0);

        QString text = opt.text;
        QFontMetrics fm(opt.font);
        int lineHeight = fm.height();
        int maxHeight = lineHeight * maxLines;

        // Truncate text if it exceeds maxLines
        QString displayText = text;
        QTextOption layoutOpt(Qt::Alignment(opt.displayAlignment));
        layoutOpt.setWrapMode(QTextOption::WrapAnywhere);
        QTextLayout layout(text, opt.font);
        layout.setTextOption(layoutOpt);
        layout.beginLayout();
        int lineCount = 0;
        int lastLineEnd = 0;
        while (lineCount < maxLines) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) break;
            line.setLineWidth(textRect.width());
            lastLineEnd = line.textStart() + line.textLength();
            lineCount++;
        }
        layout.endLayout();

        bool truncated = (lastLineEnd < text.length());
        if (truncated) {
            displayText = text.left(lastLineEnd).trimmed() + "...";
        }

        QTextOption textOption;
        textOption.setWrapMode(QTextOption::WrapAnywhere);
        textOption.setAlignment(Qt::Alignment(opt.displayAlignment));

        if (opt.state & QStyle::State_Selected)
            painter->setPen(opt.palette.highlightedText().color());
        else {
            QVariant fgVar = index.data(Qt::ForegroundRole);
            if (fgVar.isValid())
                painter->setPen(fgVar.value<QColor>());
            else
                painter->setPen(opt.palette.text().color());
        }

        painter->setFont(opt.font);
        painter->drawText(textRect, displayText, textOption);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QString text = index.data(Qt::DisplayRole).toString();
        int width = opt.rect.width() > 0 ? opt.rect.width() - 8 : 100;

        QFontMetrics fm(opt.font);
        int lineHeight = fm.height();
        QRect bound = fm.boundingRect(QRect(0, 0, width, 10000), Qt::TextWrapAnywhere, text);

        int maxHeight = lineHeight * maxLines + 8;
        return QSize(opt.rect.width(), qMin(bound.height() + 8, maxHeight));
    }

    QString displayText(const QVariant& value, const QLocale& locale) const override {
        return PaddingDelegate::displayText(value, locale);
    }

    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option, const QModelIndex& index) override {
        if (event->type() == QEvent::ToolTip) {
            QString text = index.data(Qt::DisplayRole).toString();
            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);

            QFontMetrics fm(opt.font);
            int width = opt.rect.width() - 8;
            if (width <= 0) width = 100;

            QTextOption layoutOpt;
            layoutOpt.setWrapMode(QTextOption::WrapAnywhere);
            QTextLayout layout(text, opt.font);
            layout.setTextOption(layoutOpt);
            layout.beginLayout();
            int lineCount = 0;
            while (true) {
                QTextLine line = layout.createLine();
                if (!line.isValid()) break;
                line.setLineWidth(width);
                lineCount++;
            }
            layout.endLayout();

            if (lineCount > maxLines) {
                // Wrap text manually to limit tooltip width
                QFontMetrics tooltipFm(QToolTip::font());
                QString wrappedText;
                int maxWidth = 1000;
                int pos = 0;
                while (pos < text.length()) {
                    int lineEnd = text.indexOf('\n', pos);
                    if (lineEnd == -1) lineEnd = text.length();
                    QString line = text.mid(pos, lineEnd - pos);

                    while (!line.isEmpty()) {
                        QString chunk;
                        int i = 1;
                        while (i <= line.length() && tooltipFm.horizontalAdvance(line.left(i)) < maxWidth)
                            i++;
                        chunk = line.left(i - 1);
                        if (chunk.isEmpty()) chunk = line.left(1);
                        wrappedText += chunk.toHtmlEscaped() + "<br>";
                        line = line.mid(chunk.length());
                    }
                    pos = lineEnd + 1;
                }
                if (wrappedText.endsWith("<br>"))
                    wrappedText.chop(4);
                QToolTip::showText(event->globalPos(), wrappedText, view);
                return true;
            }
        }
        return PaddingDelegate::helpEvent(event, view, option, index);
    }
};

class TreeIndentStyle : public QProxyStyle {
public:
    explicit TreeIndentStyle(int indent, QStyle* base = nullptr) : QProxyStyle(base), m_indent(indent) {}

    int pixelMetric(PixelMetric m, const QStyleOption* opt, const QWidget* w) const override {
        if (m == PM_TreeViewIndentation)
            return m_indent;
        return QProxyStyle::pixelMetric(m, opt, w);
    }
private:
    int m_indent;
};

class ColorAwareTreeView : public QTreeView {
    Q_OBJECT
public:
    using QTreeView::QTreeView;

    void setIndentGuides(bool enabled, const QColor& color = QColor()) {
        m_guidesEnabled = enabled;
        m_guideColor    = color;
        if (viewport()) viewport()->update();
    }

    bool indentGuidesEnabled() const { return m_guidesEnabled; }

protected:
    void drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const override {
        const int y = options.rect.y();
        const int h = options.rect.height();
        const QRect fullRow(0, y, viewport()->width(), h);

        QVariant bgVar = index.data(Qt::BackgroundRole);
        if (bgVar.isValid()) {
            painter->fillRect(fullRow, bgVar.value<QBrush>());
        } else {
            const bool alternate = (options.features & QStyleOptionViewItem::Alternate);
            painter->fillRect(fullRow, palette().color(alternate ? QPalette::AlternateBase : QPalette::Base));
        }

        QTreeView::drawRow(painter, options, index);
    }

    void drawBranches(QPainter* painter, const QRect& rect, const QModelIndex& index) const override {
        QVariant bgVar = index.data(Qt::BackgroundRole);
        if (bgVar.isValid())
            painter->fillRect(rect, bgVar.value<QBrush>());

        if (rect.width() <= 0)
            return;

        const int step = indentation();

        int depth = 0;
        QModelIndex tmp = index.parent();
        while (tmp.isValid()) { depth++; tmp = tmp.parent(); }

        // Arrow
        {
            QStyleOptionViewItem arrowOpt;
            arrowOpt.initFrom(this);
            arrowOpt.rect = QRect(rect.left() + depth * step, rect.top(), step, rect.height());
            arrowOpt.state = QStyle::State_Enabled;
            if (model() && model()->hasChildren(index))
                arrowOpt.state |= QStyle::State_Children;
            if (isExpanded(index))
                arrowOpt.state |= QStyle::State_Open;
            if (selectionModel() && selectionModel()->isSelected(index))
                arrowOpt.state |= QStyle::State_Selected;
            style()->drawPrimitive(QStyle::PE_IndicatorBranch, &arrowOpt, painter, this);
        }

        // Tree lines
        if (m_guidesEnabled && depth > 0) {
            painter->save();

            QColor c;
            if (m_guideColor.isValid()) {
                c = m_guideColor;
            } else {
                const bool selected = selectionModel() && selectionModel()->isSelected(index);
                if (selected) {
                    c = palette().highlightedText().color();
                } else {
                    QVariant fgVar = index.data(Qt::ForegroundRole);
                    c = fgVar.isValid() ? fgVar.value<QColor>() : palette().text().color();
                }
                c.setAlpha(180);
            }
            painter->setPen(QPen(c, 1));

            const int midY = rect.top() + rect.height() / 2;
            const int cx = step / 2;

            QList<QModelIndex> ancestors;
            for (QModelIndex a = index.parent(); a.isValid(); a = a.parent())
                ancestors.prepend(a);

            for (int d = 0; d < depth; ++d) {
                QModelIndex nextInChain = (d + 1 < ancestors.size()) ? ancestors[d + 1] : index;
                bool nextIsLast = !nextInChain.sibling(nextInChain.row() + 1, 0).isValid();
                int vx = rect.left() + d * step + cx;

                if (d == depth - 1) {
                    if (nextIsLast)
                        painter->drawLine(vx, rect.top(), vx, midY);   // closing corner └─
                    else
                        painter->drawLine(vx, rect.top(), vx, rect.bottom()); // ├─
                } else {
                    if (!nextIsLast)
                        painter->drawLine(vx, rect.top(), vx, rect.bottom());
                }
            }

            int tailFrom = rect.left() + (depth - 1) * step + cx;
            int tailTo   = rect.left() + depth * step;
            if (tailTo > tailFrom)
                painter->drawLine(tailFrom, midY, tailTo, midY);

            {
                const int arrowSize = 3;
                QPolygon arrow;
                arrow << QPoint(tailTo - arrowSize, midY - arrowSize)
                      << QPoint(tailTo,             midY)
                      << QPoint(tailTo - arrowSize, midY + arrowSize);
                painter->setBrush(c);
                painter->setPen(Qt::NoPen);
                painter->drawPolygon(arrow);
            }

            painter->restore();
        }
    }

private:
    bool   m_guidesEnabled{false};
    QColor m_guideColor;
};

#endif
