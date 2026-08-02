#ifndef ADAPTIXCLIENT_CARDLISTWIDGET_H
#define ADAPTIXCLIENT_CARDLISTWIDGET_H

#include <main.h>

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

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

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

#endif
