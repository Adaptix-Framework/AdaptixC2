#ifndef ADAPTIXCLIENT_CUSTOMELEMENTS_H
#define ADAPTIXCLIENT_CUSTOMELEMENTS_H

#include <main.h>
#include <QTextLayout>
#include <QToolTip>
#include <QHeaderView>
#include <QStackedWidget>

class QTimer;
class QMutex;


class VerticalTabBar : public QWidget
{
Q_OBJECT
public:
    explicit VerticalTabBar(QWidget *parent = nullptr);

    int addTab(const QString &text);
    void removeTab(int index);
    int count() const { return m_tabs.size(); }
    int currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);
    void setTabsClosable(bool closable) { m_closable = closable; update(); }
    void setShowAddButton(bool show) { m_showAddButton = show; update(); }
    QString tabText(int index) const;

Q_SIGNALS:
    void currentChanged(int index);
    void tabCloseRequested(int index);
    void addTabRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    QSize sizeHint() const override;

private:
    struct TabInfo { QString text; };
    QVector<TabInfo> m_tabs;
    int m_currentIndex = -1;
    int m_hoveredIndex = -1;
    int m_hoveredCloseButton = -1;
    bool m_closable = false;
    int m_tabHeight = 28;
    int m_tabWidth = 32;
    bool m_showAddButton = false;
    bool m_addButtonHovered = false;

    int tabAt(const QPoint &pos) const;
    QRect closeButtonRect(int index) const;
    QRect addButtonRect() const;
};

class VerticalTabWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VerticalTabWidget(QWidget *parent = nullptr);

    int addTab(QWidget *widget, const QString &label);
    void removeTab(int index);
    int count() const { return m_tabBar->count(); }
    int currentIndex() const { return m_tabBar->currentIndex(); }
    void setCurrentIndex(int index);
    QWidget *widget(int index) const;
    void setTabsClosable(bool closable) { m_tabBar->setTabsClosable(closable); }
    VerticalTabBar *tabBar() const { return m_tabBar; }
    void setCornerWidget(QWidget *widget);

Q_SIGNALS:
    void currentChanged(int index);
    void tabCloseRequested(int index);

private:
    VerticalTabBar *m_tabBar;
    QStackedWidget *m_stack;
    QWidget *m_cornerWidget = nullptr;
};


class BoldHeaderView : public QHeaderView
{
    Q_OBJECT
public:
    explicit BoldHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
};



class CardListWidget : public QListWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor itemBackground READ itemBackground WRITE setItemBackground)
    Q_PROPERTY(QColor itemBackgroundHover READ itemBackgroundHover WRITE setItemBackgroundHover)
    Q_PROPERTY(QColor itemBackgroundSelected READ itemBackgroundSelected WRITE setItemBackgroundSelected)
    Q_PROPERTY(QColor titleColor READ titleColor WRITE setTitleColor)
    Q_PROPERTY(QColor titleColorSelected READ titleColorSelected WRITE setTitleColorSelected)
    Q_PROPERTY(QColor subtitleColor READ subtitleColor WRITE setSubtitleColor)
    Q_PROPERTY(QColor subtitleColorSelected READ subtitleColorSelected WRITE setSubtitleColorSelected)

    QColor m_itemBackground = QColor(42, 42, 42);
    QColor m_itemBackgroundHover = QColor(50, 50, 50);
    QColor m_itemBackgroundSelected = QColor(11, 89, 45);
    QColor m_titleColor = QColor(190, 190, 190);
    QColor m_titleColorSelected = QColor(200, 200, 200);
    QColor m_subtitleColor = QColor(140, 140, 140);
    QColor m_subtitleColorSelected = QColor(180, 180, 180);

public:
    enum DataRole { TitleRole = Qt::UserRole, TextRole = Qt::UserRole + 1 };

    explicit CardListWidget(QWidget *parent = nullptr);

    void addCard(const QString &title, const QString &text);
    void updateColorsFromPalette();

    QColor itemBackground() const { return m_itemBackground; }
    void setItemBackground(const QColor &color) { m_itemBackground = color; }

    QColor itemBackgroundHover() const { return m_itemBackgroundHover; }
    void setItemBackgroundHover(const QColor &color) { m_itemBackgroundHover = color; }

    QColor itemBackgroundSelected() const { return m_itemBackgroundSelected; }
    void setItemBackgroundSelected(const QColor &color) { m_itemBackgroundSelected = color; }

    QColor titleColor() const { return m_titleColor; }
    void setTitleColor(const QColor &color) { m_titleColor = color; }

    QColor titleColorSelected() const { return m_titleColorSelected; }
    void setTitleColorSelected(const QColor &color) { m_titleColorSelected = color; }

    QColor subtitleColor() const { return m_subtitleColor; }
    void setSubtitleColor(const QColor &color) { m_subtitleColor = color; }

    QColor subtitleColorSelected() const { return m_subtitleColorSelected; }
    void setSubtitleColorSelected(const QColor &color) { m_subtitleColorSelected = color; }
};

class CardListDelegate : public QStyledItemDelegate
{
public:
    explicit CardListDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};




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
    explicit PaddingDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_padding(4) {}
    explicit PaddingDelegate(int padding, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_padding(padding) {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        return QSize(size.width() + m_padding * 2, size.height() + m_padding);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        QVariant bgVar = index.data(Qt::BackgroundRole);
        if (bgVar.isValid())
            painter->fillRect(opt.rect, bgVar.value<QBrush>());

        opt.state &= ~QStyle::State_HasFocus;

        const QWidget* widget = option.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

        if (!opt.icon.isNull()) {
            QRect iconRect = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, widget);
            opt.icon.paint(painter, iconRect, opt.decorationAlignment,
                          opt.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled,
                          opt.state & QStyle::State_Open ? QIcon::On : QIcon::Off);
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





class SpinTable : public QWidget {
Q_OBJECT
public:
    QGridLayout*  layout      = nullptr;
    QTableWidget* table       = nullptr;
    QPushButton*  buttonAdd   = nullptr;
    QPushButton*  buttonClear = nullptr;

    SpinTable(int rows, int clomuns, QWidget* parent);

    explicit SpinTable(QWidget* parent = nullptr) { SpinTable(0,0,parent); }
    ~SpinTable() override = default;
};





class ClickableLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickableLabel(const QString &label, QWidget *parent = nullptr) : QLabel(label, parent) {}

    Q_SIGNALS:
        void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            Q_EMIT clicked();
        }
        QLabel::mousePressEvent(event);
    }
};




class TextEditConsole : public QTextEdit {
Q_OBJECT
    struct FormattedChunk {
        QString text;
        QTextCharFormat format;
    };

    QTextCursor cachedCursor;
    int  maxLines    = 50000;
    bool autoScroll  = false;
    bool noWrap      = true;
    bool syncMode    = false;
    int appendCount  = 0;

    QString pendingText;
    QList<FormattedChunk> pendingFormatted;
    QTimer* batchTimer = nullptr;
    QMutex* batchMutex = nullptr;
    static constexpr int BATCH_INTERVAL_MS = 100;
    static constexpr int MAX_BATCH_SIZE = 64 * 1024;

    void trimExcessLines();
    void createContextMenu(const QPoint &pos);
    void setBufferSize(int size);

private Q_SLOTS:
    void flushPendingText();

public:
    explicit TextEditConsole(QWidget* parent = nullptr, int maxLines = 50000, bool noWrap = true, bool autoScroll = false);

    void appendPlain(const QString& text);
    void appendFormatted(const QString& text, const std::function<void(QTextCharFormat&)> &styleFn);

    void appendColor(const QString& text, QColor color);
    void appendBold(const QString& text);
    void appendUnderline(const QString& text);
    void appendColorBold(const QString& text, QColor color);
    void appendColorUnderline(const QString& text, QColor color);

    void setMaxLines(int lines);
    void setAutoScrollEnabled(bool enabled);
    bool isAutoScrollEnabled() const;
    bool isNoWrapEnabled() const;

    void setSyncMode(bool enabled);
    void flushAll();

Q_SIGNALS:
    void ctx_find();
    void ctx_history();
};

#endif
