#ifndef ADAPTIXCLIENT_CONNECTIONSTATUSWIDGET_H
#define ADAPTIXCLIENT_CONNECTIONSTATUSWIDGET_H

#include <QPushButton>

class ConnectionStatusWidget : public QPushButton
{
Q_OBJECT

public:
    enum State {
        Connected    = 0,
        Disconnected = 1,
        Reconnecting = 2
    };

    explicit ConnectionStatusWidget(QWidget* parent = nullptr);

    void setState(State state);
    State state() const { return m_state; }

    void setCompact(bool compact);
    bool isCompact() const { return m_compact; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    State   m_state = Connected;
    bool    m_compact = false;
    QString m_label;
    QColor  m_dotColor;
    QColor  m_fgColor;

    void refreshAppearance();
    void recolorFromTheme(bool fullUpdate = false);
};

#endif
