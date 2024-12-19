#ifndef ADAPTIXCLIENT_CUSTOMELEMENTS_H
#define ADAPTIXCLIENT_CUSTOMELEMENTS_H

#include <QStyledItemDelegate>

class PaddingDelegate : public QStyledItemDelegate {
public:
    explicit PaddingDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        opt.rect.adjust(15, 0, -15, 0);

        QStyledItemDelegate::paint(painter, opt, index);
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize originalSize = QStyledItemDelegate::sizeHint(option, index);
        return QSize(originalSize.width() + 30, originalSize.height());
    }
};

#endif //ADAPTIXCLIENT_CUSTOMELEMENTS_H
