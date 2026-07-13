#include <Utils/CustomElements/BoldHeaderView.h>
#include <QPainter>

#include <oclero/qlementine/style/QlementineStyle.hpp>

BoldHeaderView::BoldHeaderView(Qt::Orientation orientation, QWidget *parent) : QHeaderView(orientation, parent)
{
    setDefaultAlignment(Qt::AlignCenter);
    setMinimumSectionSize(24);
    setDefaultSectionSize(100);
    setSectionsClickable(true);
    setSortIndicatorShown(true);
}

void BoldHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
    if (!rect.isValid())
        return;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    QColor bgColor, textColor;
    if (qs) {
        bgColor = qs->tableHeaderBgColor(oclero::qlementine::MouseState::Normal, oclero::qlementine::CheckState::NotChecked);
        textColor = qs->tableHeaderFgColor(oclero::qlementine::MouseState::Normal, oclero::qlementine::CheckState::NotChecked);
    } else {
        const QPalette& pal = palette();
        bgColor = pal.color(QPalette::Window).darker(115);
        textColor = pal.color(QPalette::ButtonText);
    }

    painter->fillRect(rect, bgColor);

    QString text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
    QFont boldFont = painter->font();
    boldFont.setBold(true);
    painter->setFont(boldFont);
    painter->setPen(textColor);

    bool hasSortIndicator = isSortIndicatorShown() && sortIndicatorSection() == logicalIndex;
    QRect textRect = hasSortIndicator ? rect.adjusted(4, 0, -16, 0) : rect;
    painter->drawText(textRect, Qt::AlignCenter, text);

    if (hasSortIndicator) {
        int arrowSize = 6;
        int centerY = rect.center().y();
        int arrowX = rect.right() - 10;

        painter->setPen(Qt::NoPen);
        painter->setBrush(textColor);  // Same color as text

        QPolygon triangle;
        if (sortIndicatorOrder() == Qt::AscendingOrder) {
            triangle << QPoint(arrowX, centerY + arrowSize/2)
                     << QPoint(arrowX + arrowSize, centerY + arrowSize/2)
                     << QPoint(arrowX + arrowSize/2, centerY - arrowSize/2);
        } else {
            triangle << QPoint(arrowX, centerY - arrowSize/2)
                     << QPoint(arrowX + arrowSize, centerY - arrowSize/2)
                     << QPoint(arrowX + arrowSize/2, centerY + arrowSize/2);
        }
        painter->drawPolygon(triangle);
    }

    painter->restore();
}
