#ifndef ADAPTIXCLIENT_SEGMENTCONTROL_H
#define ADAPTIXCLIENT_SEGMENTCONTROL_H

#include <main.h>

#include <QFrame>
#include <QPushButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QVector>

class SegmentControl : public QFrame
{
Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)

    QHBoxLayout*          m_layout = nullptr;
    QButtonGroup*         m_group  = nullptr;
    QVector<QPushButton*> m_buttons;
    int                   m_currentIndex = -1;
    int                   m_minButtonWidth = 88;
    bool                  m_applyingTheme = false;
    QMetaObject::Connection m_themeConn;

    QPushButton* makeButton(const QString& text);
    void reindexButtons();
    void updateMetrics();
    void connectThemeSignals();

protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

public:
    explicit SegmentControl(QWidget* parent = nullptr);
    ~SegmentControl() override = default;

    int  addItem(const QString& text);
    void addItems(const QStringList& texts);
    void removeItem(int index);
    void clear();

    void    setItemText(int index, const QString& text);
    QString itemText(int index) const;
    QString getItemText(int index) const { return itemText(index); }
    void    setItemToolTip(int index, const QString& tip);

    int itemCount() const { return m_buttons.size(); }
    int count() const { return itemCount(); }

    int  currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int index);
    QString currentText() const;

    void setMinimumButtonWidth(int width);
    int  minimumButtonWidth() const { return m_minButtonWidth; }

    void applyTheme();

Q_SIGNALS:
    void currentIndexChanged();
};

#endif
