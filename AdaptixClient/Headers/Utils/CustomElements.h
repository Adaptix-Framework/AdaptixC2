#ifndef ADAPTIXCLIENT_CUSTOMELEMENTS_H
#define ADAPTIXCLIENT_CUSTOMELEMENTS_H

#include <main.h>

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
