#ifndef ADAPTIXCLIENT_BOLDHEADERVIEW_H
#define ADAPTIXCLIENT_BOLDHEADERVIEW_H

#include <main.h>
#include <QHeaderView>

class BoldHeaderView : public QHeaderView
{
Q_OBJECT
public:
    explicit BoldHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
};

#endif
