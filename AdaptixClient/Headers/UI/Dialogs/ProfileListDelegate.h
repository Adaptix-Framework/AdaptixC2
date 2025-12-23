#ifndef ADAPTIXCLIENT_PROFILELISTDELEGATE_H
#define ADAPTIXCLIENT_PROFILELISTDELEGATE_H

#include <main.h>
#include <QApplication>
#include <QColor>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>

class QStyleOptionViewItem;
class QModelIndex;
class QWidget;

class ProfileListDelegate : public QStyledItemDelegate
{
public:
    explicit ProfileListDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    void setTitleColor(const QColor &color);
    void setSubtitleColor(const QColor &color);
    QColor titleColor() const;
    QColor subtitleColor() const;

private:
    QFont getFont(const QWidget *widget) const;
    QColor m_titleColor;
    QColor m_subtitleColor;
};

#endif

