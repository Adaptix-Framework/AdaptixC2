#include <Utils/CustomElements/ConnectionStatusWidget.h>
#include <Utils/FontManager.h>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>

#include <QApplication>
#include <QEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QFontMetrics>

static QColor withAlpha(const QColor& c, int alpha)
{
    QColor r = c;
    r.setAlpha(alpha);
    return r;
}

ConnectionStatusWidget::ConnectionStatusWidget(QWidget* parent) : QPushButton(parent)
{
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_StyledBackground, false);
    setFlat(true);
    connect(&FontManager::instance(), &FontManager::typographyChanged, this, [this]() {
        refreshAppearance();
    });
    refreshAppearance();
}

void ConnectionStatusWidget::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    refreshAppearance();
}

void ConnectionStatusWidget::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    refreshAppearance();
}

void ConnectionStatusWidget::changeEvent(QEvent* event)
{
    QPushButton::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange)
        recolorFromTheme();
}

void ConnectionStatusWidget::refreshAppearance()
{
    recolorFromTheme(/*regenerate_label_and_size=*/true);
}

void ConnectionStatusWidget::recolorFromTheme(bool fullUpdate)
{
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();

    QColor fg;
    QColor dot;
    QString label;

    switch (m_state) {
    case Connected:
        fg    = t.statusColorSuccess;
        dot   = t.statusColorSuccess;
        label = m_compact ? QString() : QStringLiteral("Connected");
        setToolTip(QStringLiteral("Connected to C2 — click to reconnect"));
        break;
    case Disconnected:
        fg    = t.statusColorError;
        dot   = t.statusColorError;
        label = m_compact ? QString() : QStringLiteral("Disconnected");
        setToolTip(QStringLiteral("Disconnected from C2 — click to reconnect"));
        break;
    case Reconnecting:
        fg    = t.statusColorWarning;
        dot   = t.statusColorWarning;
        label = m_compact ? QStringLiteral("…") : QStringLiteral("Reconnecting…");
        setToolTip(QStringLiteral("Reconnecting…"));
        break;
    }

    m_fgColor  = fg;
    m_dotColor = dot;


    if (fullUpdate) {
        m_label = label;
        const int h = FontManager::instance().typography().controlHeight;
        if (m_compact) {
            setFixedHeight(h);
            setFixedWidth(h);
        } else {
            setFixedHeight(h);
            QFont f = FontManager::instance().appRegularFont();
            f.setWeight(QFont::DemiBold);
            setFont(f);
            QFontMetrics fm(f);
            int textW = label.isEmpty() ? 0 : fm.horizontalAdvance(label);
            int dotW  = 8 + 7; // dotDiam + gap
            int pad   = 11 + 7; // left pad + text offset
            setMinimumWidth(pad + dotW + textW + 16);
            setMaximumWidth(QWIDGETSIZE_MAX);
        }
    }
    update();
}

void ConnectionStatusWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = r.height() / 2.0;

    const bool hovered  = underMouse();
    const bool pressed  = isDown();
    int fillAlpha = qRound(255 * 0.13);
    if (hovered)
        fillAlpha = qRound(255 * 0.20);
    if (pressed)
        fillAlpha = qRound(255 * 0.28);

    p.setPen(QPen(withAlpha(m_fgColor, qRound(255 * 0.40)), 1));
    p.setBrush(withAlpha(m_fgColor, fillAlpha));
    p.drawRoundedRect(r, radius, radius);

    const int dotDiam = m_compact ? 10 : 8;
    const qreal dotCx = m_compact ? r.center().x() : r.left() + 11 + dotDiam / 2.0;
    const qreal dotCy = r.center().y();

    if (m_state == Connected) {
        QColor halo = m_dotColor;
        halo.setAlpha(70);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(halo, 1.4));
        p.drawEllipse(QPointF(dotCx, dotCy), dotDiam / 2.0 + 2.0, dotDiam / 2.0 + 2.0);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(m_dotColor);
    p.drawEllipse(QPointF(dotCx, dotCy), dotDiam / 2.0, dotDiam / 2.0);

    if (!m_compact && !m_label.isEmpty()) {
        QFont f = font();
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(m_fgColor);

        const int textX = qRound(dotCx + dotDiam / 2.0 + 7);
        const QRect textRect(textX, qRound(r.top()), qRound(r.width()) - (textX - qRound(r.left())), qRound(r.height()));
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, m_label);
    }
}
