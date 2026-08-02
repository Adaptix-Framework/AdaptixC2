#include <Utils/CustomElements/CardListWidget.h>

CardListWidget::CardListWidget(QWidget *parent) : QListWidget(parent)
{
    setMouseTracking(true);
    setSpacing(1);
    setItemDelegate(new CardListDelegate(this));
    setMinimumHeight(0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    updateColorsFromPalette();
}

QSize CardListWidget::sizeHint() const
{
    return QSize(200, 48);
}

QSize CardListWidget::minimumSizeHint() const
{
    return QSize(0, 0);
}

void CardListWidget::updateColorsFromPalette()
{
    const QPalette &pal = palette();
    m_itemBackground = pal.color(QPalette::AlternateBase);
    m_itemBackgroundHover = pal.color(QPalette::Mid);
    m_itemBackgroundSelected = pal.color(QPalette::Highlight);
    m_titleColor = pal.color(QPalette::Text);
    m_titleColorSelected = pal.color(QPalette::HighlightedText);
    m_subtitleColor = pal.color(QPalette::Text);
    m_subtitleColor.setAlpha(160);
    m_subtitleColorSelected = pal.color(QPalette::HighlightedText);
    m_subtitleColorSelected.setAlpha(200);
}

void CardListWidget::addCard(const QString &title, const QString &text)
{
    auto* item = new QListWidgetItem(this);
    item->setData(TitleRole, title);
    item->setData(TextRole, text);
}

CardListDelegate::CardListDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void CardListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    QString title = index.data(CardListWidget::TitleRole).toString();
    QString text = index.data(CardListWidget::TextRole).toString();

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHovered = option.state & QStyle::State_MouseOver;

    auto* cardList = qobject_cast<const CardListWidget*>(option.widget);

    constexpr int cardMargin = 3;
    QRect cardRect = option.rect.adjusted(cardMargin, 2, -cardMargin, -2);

    QColor bgColor, titleColor, subtitleColor;

    if (cardList) {
        if (isSelected) {
            bgColor = cardList->itemBackgroundSelected();
            titleColor = cardList->titleColorSelected();
            subtitleColor = cardList->subtitleColorSelected();
        } else if (isHovered) {
            bgColor = cardList->itemBackgroundHover();
            titleColor = cardList->titleColor();
            subtitleColor = cardList->subtitleColor();
        } else {
            bgColor = cardList->itemBackground();
            titleColor = cardList->titleColor();
            subtitleColor = cardList->subtitleColor();
        }
    } else {
        bgColor = isSelected ? option.palette.highlight().color() : option.palette.mid().color();
        if (!isSelected) bgColor.setAlpha(40);
        titleColor = isSelected ? option.palette.highlightedText().color() : option.palette.text().color();
        subtitleColor = titleColor;
        subtitleColor.setAlpha(isSelected ? 200 : 140);
    }

    painter->fillRect(cardRect, bgColor);

    constexpr int hPadding = 12;
    constexpr int vPadding = 10;
    constexpr int lineSpacing = 4;

    QRect contentRect = cardRect.adjusted(hPadding, vPadding, -hPadding, -vPadding);

    QFont titleFont = option.font;
    titleFont.setWeight(QFont::Bold);
    QFontMetrics titleFm(titleFont);

    painter->setFont(titleFont);
    painter->setPen(titleColor);

    QRect titleRect = contentRect;
    titleRect.setHeight(titleFm.height());
    QString elidedTitle = titleFm.elidedText(title, Qt::ElideRight, titleRect.width());
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

    if (!text.isEmpty()) {
        QFont subtitleFont = option.font;
        int subtitleSize = qMax(subtitleFont.pointSize() - 1, 8);
        subtitleFont.setPointSize(subtitleSize);
        QFontMetrics subtitleFm(subtitleFont);

        painter->setFont(subtitleFont);
        painter->setPen(subtitleColor);

        QRect subtitleRect = contentRect;
        subtitleRect.setTop(titleRect.bottom() + lineSpacing);
        subtitleRect.setHeight(subtitleFm.height());
        QString elidedSubtitle = subtitleFm.elidedText(text, Qt::ElideRight, subtitleRect.width());
        painter->drawText(subtitleRect, Qt::AlignLeft | Qt::AlignVCenter, elidedSubtitle);
    }

    painter->restore();
}

QSize CardListDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index)

    constexpr int cardMargin = 3;
    constexpr int vPadding = 10;
    constexpr int lineSpacing = 4;

    QFont titleFont = option.font;
    titleFont.setWeight(QFont::Bold);
    QFontMetrics titleFm(titleFont);

    QFont subtitleFont = option.font;
    int subtitleSize = qMax(subtitleFont.pointSize() - 1, 8);
    subtitleFont.setPointSize(subtitleSize);
    QFontMetrics subtitleFm(subtitleFont);

    int totalHeight = cardMargin + vPadding + titleFm.height() + lineSpacing + subtitleFm.height() + vPadding + cardMargin;

    return QSize(190, totalHeight);
}
