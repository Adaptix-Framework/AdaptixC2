#include <UI/Dialogs/ProfileListDelegate.h>

ProfileListDelegate::ProfileListDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

QFont ProfileListDelegate::getFont(const QWidget *widget) const
{
    return widget ? widget->font() : QApplication::font();
}

void ProfileListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    
    QString profileName = index.data(Qt::DisplayRole).toString();
    QString subtitle = index.data(Qt::UserRole).toString();
    
    opt.text.clear();
    
    QVariant bgColorVariant = index.data(Qt::BackgroundRole);
    QBrush customBgBrush;
    bool hasCustomBg = false;
    if (bgColorVariant.isValid() && bgColorVariant.canConvert<QBrush>()) {
        customBgBrush = bgColorVariant.value<QBrush>();
        if (customBgBrush.style() != Qt::NoBrush && !(option.state & QStyle::State_Selected)) {
            hasCustomBg = true;
            opt.backgroundBrush = customBgBrush;
            painter->fillRect(opt.rect, customBgBrush);
        }
    }
    
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);
    
    if (hasCustomBg) {
        painter->fillRect(opt.rect, customBgBrush);
    }

    painter->save();

    QRect rect = option.rect.adjusted(12, 10, -12, -10);
    int padding = 2;

    QFont titleFont = getFont(opt.widget);
    
    QVariant fontVariant = index.data(Qt::FontRole);
    if (fontVariant.isValid() && fontVariant.canConvert<QFont>()) {
        QFont itemFont = fontVariant.value<QFont>();
        if (itemFont.bold()) {
            titleFont.setBold(true);
        }
    }
    
    if (option.state & QStyle::State_Selected) {
        painter->setPen(option.palette.highlightedText().color());
    } else {
        painter->setPen(option.palette.text().color());
    }
    
    QFontMetrics titleFm(titleFont);
    painter->setFont(titleFont);

    QRect titleRect = rect;
    titleRect.setHeight(titleFm.height());
    QString elidedTitle = titleFm.elidedText(profileName, Qt::ElideRight, titleRect.width());
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignTop, elidedTitle);

    if (!subtitle.isEmpty()) {
        QColor subtitleColor;
        if (option.state & QStyle::State_Selected) {
            subtitleColor = option.palette.highlightedText().color();
            subtitleColor.setAlpha(180);
        } else {
            subtitleColor = option.palette.color(QPalette::Disabled, QPalette::Text);
        }
        
        QFont subtitleFont = titleFont;
        subtitleFont.setPointSize(subtitleFont.pointSize() - 3);
        painter->setFont(subtitleFont);
        painter->setPen(subtitleColor);

        QFontMetrics subtitleFm(subtitleFont);
        QRect subtitleRect = rect;
        subtitleRect.setTop(titleRect.bottom() + padding);
        subtitleRect.setHeight(subtitleFm.height());
        QString elidedSubtitle = subtitleFm.elidedText(subtitle, Qt::ElideRight, subtitleRect.width());
        painter->drawText(subtitleRect, Qt::AlignLeft | Qt::AlignTop, elidedSubtitle);
    }

    painter->restore();
}

QSize ProfileListDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QFont titleFont = getFont(option.widget);
    
    QFontMetrics fm(titleFont);
    int baseHeight = fm.height() + 20;
    
    QString subtitle = index.data(Qt::UserRole).toString();

    if (!subtitle.isEmpty()) {
        QFont subtitleFont = titleFont;
        subtitleFont.setPointSize(subtitleFont.pointSize() - 3);
        QFontMetrics subtitleFm(subtitleFont);
        baseHeight += subtitleFm.height() + 2;
    }

    return QSize(100, baseHeight);
}

