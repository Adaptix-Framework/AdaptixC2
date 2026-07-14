#ifndef ADAPTIXCLIENT_CLICKABLELABEL_H
#define ADAPTIXCLIENT_CLICKABLELABEL_H

#include <main.h>

class ClickableLabel : public QLabel
{
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

#endif
