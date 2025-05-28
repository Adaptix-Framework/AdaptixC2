#ifndef ADAPTIXCLIENT_CUSTOMELEMENTS_H
#define ADAPTIXCLIENT_CUSTOMELEMENTS_H

#include <main.h>

class PaddingDelegate : public QStyledItemDelegate {
public:
    explicit PaddingDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    // void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
    //      QStyleOptionViewItem opt = option;
    //      initStyleOption(&opt, index);
    //
    //      QVariant bgColor = index.data(Qt::BackgroundRole);
    //      if (bgColor.canConvert<QColor>())
    //          opt.backgroundBrush = QBrush(bgColor.value<QColor>());
    //
    //      QVariant fgColor = index.data(Qt::ForegroundRole);
    //      if (fgColor.canConvert<QColor>())
    //          opt.palette.setColor(QPalette::Text, fgColor.value<QColor>());
    //
    //     // opt.rect.adjust(0, 0, 0, 0);
    //
    //      QStyledItemDelegate::paint(painter, opt, index);
    //  }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem optFull = option;
        initStyleOption(&optFull, index);

        QVariant bgVar = index.data(Qt::BackgroundRole);
        bool hasBg = bgVar.isValid();
        QBrush bgBrush = hasBg ? bgVar.value<QBrush>() : QBrush();

        if (hasBg)
            painter->fillRect(optFull.rect, bgBrush);

        QStyledItemDelegate::paint(painter, optFull, index);
    }

     // QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
     //     QSize originalSize = QStyledItemDelegate::sizeHint(option, index);
     //     return QSize(originalSize.width() + 30, originalSize.height());
     // }
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



class FileSelector : public QWidget
{
Q_OBJECT

public:
    QVBoxLayout* layout = nullptr;
    QLineEdit*   input  = nullptr;
    QPushButton* button = nullptr;

    QString content;

    explicit FileSelector(QWidget* parent = nullptr);
    ~FileSelector() override = default;
};




class ClickableLabel : public QLabel {
    Q_OBJECT

public:
    explicit ClickableLabel(const QString &label, QWidget *parent = nullptr) : QLabel(label, parent) {}

    signals:
        void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            emit clicked();
        }
        QLabel::mousePressEvent(event);
    }
};

#endif //ADAPTIXCLIENT_CUSTOMELEMENTS_H
