#include <Utils/CustomElements/ControlCard.h>
#include <Utils/CustomElements/ListFeed.h>
#include <Utils/FontManager.h>

#include <oclero/qlementine/style/Theme.hpp>
#include <oclero/qlementine/style/QlementineStyle.hpp>

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QEvent>
#include <QPainter>
#include <QStyle>
#include <QMetaType>
#include <QFontMetrics>
#include <QFrame>
#include <QApplication>
#include <QScrollBar>

QString ControlCardData::key() const
{
    if (id.typeId() == QMetaType::QString)
        return id.toString();
    if (id.typeId() == QMetaType::LongLong || id.typeId() == QMetaType::Int
        || id.typeId() == QMetaType::ULongLong || id.typeId() == QMetaType::UInt)
        return QString::number(id.toLongLong());
    return id.toString();
}

QString ControlCardList::keyOf(const QVariant& id)
{
    ControlCardData d;
    d.id = id;
    return d.key();
}

static QString cssColor(const QColor& c)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

struct CardTheme {
    bool dark = true;
    FeedColors fc;

    QColor bg() const { return fc.rowBg; }
    QColor bgElevated() const { return fc.rowAltBg; }
    QColor bgHover() const { return fc.rowHoverBg; }
    QColor bgDead() const { return fc.rowDeadBg; }
    QColor bgSelected() const { return fc.rowSelectedBg; }
    QColor bgNotch() const { return fc.groupBg; } // title/status differs from widget
    QColor text() const { return fc.textPrimary; }
    QColor textMuted() const { return fc.textSecondary; }
    QColor textDead() const { return fc.textDead; }
    QColor border() const { return fc.separatorLine; }
    QColor separator() const { return fc.separatorLine; }
    QColor primary() const { return fc.running; }
    QColor success() const { return fc.success; }
    QColor warning() const { return fc.hosted; }
    QColor error() const { return fc.error; }
    QColor info() const { return fc.canceled; } // statusColorInfo via Feed
    QColor tagBorder() const { return fc.tagBorder; }
    QColor tagText() const { return fc.tagText; }
};

static CardTheme resolveCardTheme(const QWidget* w)
{
    CardTheme t;
    t.fc = FeedColors::fromTheme();

    const QPalette pal = w ? w->palette() : qApp->palette();
    QColor baseText = pal.color(QPalette::Text);
    if (!baseText.isValid() || baseText.alpha() == 0)
        baseText = pal.color(QPalette::WindowText);
    if (baseText.isValid() && baseText.alpha() > 0) {
        t.fc.textPrimary = baseText;
        {
            float h, s, l, a;
            baseText.getHslF(&h, &s, &l, &a);
            const bool dark = t.fc.rowBg.lightnessF() < 0.5;
            if (dark)
                l = qBound(0.45f, l * 0.65f, 0.7f);
            else
                l = qBound(0.35f, l * 1.4f, 0.55f);
            s *= 0.35f;
            t.fc.textSecondary = QColor::fromHslF(h, s, l, a);
        }
        {
            float h, s, l, a;
            baseText.getHslF(&h, &s, &l, &a);
            const bool dark = t.fc.rowBg.lightnessF() < 0.5;
            if (dark)
                l = qBound(0.55f, l * 0.65f, 0.70f);
            else
                l = qBound(0.35f, l * 1.5f, 0.50f);
            s *= 0.15f;
            t.fc.textDead = QColor::fromHslF(h, s, l, a);
        }
        t.fc.tagText = baseText;
    }
    t.dark = t.fc.rowBg.lightnessF() < 0.5;
    return t;
}

static void styleOutlineButton(QPushButton* btn, const QColor& borderColor, const QColor& textColor, const QColor& hoverBorder, const QColor& pressBg)
{
    if (!btn)
        return;
    btn->setFlat(false);
    btn->setAutoDefault(false);
    btn->setDefault(false);
    btn->setAttribute(Qt::WA_StyledBackground, true);
    btn->setAutoFillBackground(false);

    QColor border = borderColor;
    if (!border.isValid())
        border = textColor;
    if (border.alpha() < 160)
        border.setAlpha(160);
    if (border.alpha() > 220)
        border.setAlpha(200);

    QColor text = textColor;
    if (!text.isValid())
        text = QColor(220, 220, 220);

    QColor hi = hoverBorder.isValid() ? hoverBorder : border;
    if (hi.alpha() < 180)
        hi.setAlpha(180);
    if (hi.alpha() > 230)
        hi.setAlpha(220);
    QColor press = pressBg;
    if (!press.isValid()) {
        press = border;
        press.setAlpha(40);
    } else {
        press.setAlpha(qMin(55, press.alpha() > 0 ? press.alpha() : 40));
    }

    QColor dis = text;
    dis.setAlpha(100);
    QColor disBorder = border;
    disBorder.setAlpha(90);

    const QString name = btn->objectName().isEmpty() ? QStringLiteral("ControlCardBtn") : btn->objectName();
    if (btn->objectName().isEmpty())
        btn->setObjectName(name);

    btn->setStyleSheet(QStringLiteral(
        "QPushButton#%5 {"
        "  background-color: transparent; color: %1;"
        "  border: 1px solid %2; border-radius: 4px;"
        "  padding: 2px 10px;"
        "  min-height: 0;"
        "}"
        "QPushButton#%5:hover { color: %1; border-color: %3; background-color: transparent; }"
        "QPushButton#%5:pressed { color: %1; border-color: %3; background-color: %4; }"
        "QPushButton#%5:disabled { color: %6; border-color: %7; }"
    ).arg(cssColor(text), cssColor(border), cssColor(hi), cssColor(press),
          name, cssColor(dis), cssColor(disBorder)));
}

ControlFieldTile::ControlFieldTile(QWidget* parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("ControlFieldTile"));
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    setFrameShape(QFrame::NoFrame);

    m_caption = new QLabel(this);
    m_caption->setObjectName(QStringLiteral("fieldCaption"));
    m_caption->setTextInteractionFlags(Qt::NoTextInteraction);
    m_caption->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    m_value = new QLabel(this);
    m_value->setObjectName(QStringLiteral("fieldValue"));
    m_value->setTextInteractionFlags(Qt::NoTextInteraction);
    m_value->setWordWrap(false);
    m_value->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_value->setMinimumWidth(0);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(7, 3, 8, 3);
    lay->setSpacing(6);
    lay->addWidget(m_caption, 0);
    lay->addWidget(m_value, 0);
}

void ControlFieldTile::setExpanding(bool expanding)
{
    setSizePolicy(expanding ? QSizePolicy::Expanding : QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_value->setSizePolicy(expanding ? QSizePolicy::Expanding : QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void ControlFieldTile::setCaption(const QString& caption)
{
    m_caption->setText(caption);
    m_caption->setVisible(!caption.isEmpty());
}

void ControlFieldTile::setValue(const QString& value)
{
    m_fullValue = value;
    m_value->setToolTip(value);
    reflow(m_value->width() > 8 ? m_value->width() : 120);
}

void ControlFieldTile::setValueFont(const QFont& font)
{
    m_value->setFont(font);
}

void ControlFieldTile::setCaptionFont(const QFont& font)
{
    m_caption->setFont(font);
}

void ControlFieldTile::setMonoValue(bool mono)
{
    m_mono = mono;
}

void ControlFieldTile::applyColors(const QColor& caption, const QColor& value, const QColor& panelBg, const QColor& panelBorder)
{
    auto setC = [](QLabel* lb, const QColor& c) {
        QPalette p = lb->palette();
        p.setColor(QPalette::WindowText, c);
        p.setColor(QPalette::Text, c);
        lb->setPalette(p);
        lb->setForegroundRole(QPalette::WindowText);
    };
    setC(m_caption, caption);
    setC(m_value, value);

    setStyleSheet(QStringLiteral(
        "QFrame#ControlFieldTile {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 4px;"
        "}"
    ).arg(cssColor(panelBg), cssColor(panelBorder)));
}

void ControlFieldTile::reflow(int /*maxValueWidth*/)
{
    if (m_fullValue.isEmpty()) {
        m_value->clear();
        return;
    }
    m_value->setText(m_fullValue);
    m_value->setToolTip(m_fullValue);
    adjustSize();
    updateGeometry();
}

ControlCard::ControlCard(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("ControlCard"));
    setCursor(Qt::ArrowCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(0);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    m_factsZone = new QWidget(this);
    m_factsZone->setObjectName(QStringLiteral("factsZone"));
    m_factsLayout = new QHBoxLayout(m_factsZone);
    m_factsLayout->setContentsMargins(0, 0, 0, 0);
    m_factsLayout->setSpacing(6);

    m_primaryTile = new ControlFieldTile(m_factsZone);
    m_detailTile  = new ControlFieldTile(m_factsZone);
    m_primaryTile->setExpanding(false);
    m_detailTile->setExpanding(false);

    m_tagChip = new QFrame(m_factsZone);
    m_tagChip->setObjectName(QStringLiteral("tagChip"));
    m_tagChip->setFrameShape(QFrame::NoFrame);
    m_tagChip->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_tagChip->setVisible(false);
    auto* tagLay = new QHBoxLayout(m_tagChip);
    tagLay->setContentsMargins(6, 2, 6, 2);
    tagLay->setSpacing(0);
    m_sideLabel = new QLabel(m_tagChip);
    m_sideLabel->setObjectName(QStringLiteral("factsSide"));
    m_sideLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_sideLabel->setWordWrap(false);
    m_sideLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    tagLay->addWidget(m_sideLabel);

    m_dateLabel = new QLabel(m_factsZone);
    m_dateLabel->setObjectName(QStringLiteral("factsDate"));
    m_dateLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_dateLabel->setWordWrap(false);
    m_dateLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_dateLabel->setVisible(false);

    m_factsSpacer = new QWidget(m_factsZone);
    m_factsSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_factsLayout->addWidget(m_primaryTile, 0);
    m_factsLayout->addWidget(m_detailTile, 0);
    m_factsLayout->addWidget(m_tagChip, 0);
    m_factsLayout->addStretch(1);
    m_factsLayout->addWidget(m_dateLabel, 0);
    m_factsLayout->addWidget(m_factsSpacer, 0);
    m_factsSpacer->setVisible(false);

    m_bottomZone = new QWidget(this);
    m_bottomZone->setObjectName(QStringLiteral("bottomZone"));
    m_bottomZone->setMinimumWidth(0);
    m_bottomZone->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_bottomLayout = new QHBoxLayout(m_bottomZone);
    m_bottomLayout->setContentsMargins(1, 0, 0, 0);
    m_bottomLayout->setSpacing(8);

    m_metaLead = new QLabel(m_bottomZone);
    m_metaLead->setObjectName(QStringLiteral("metaLead"));
    m_metaLead->setTextInteractionFlags(Qt::NoTextInteraction);
    m_metaLead->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_metaSep = new QLabel(QStringLiteral(" | "), m_bottomZone);
    m_metaSep->setObjectName(QStringLiteral("metaSep"));
    m_metaSep->setTextInteractionFlags(Qt::NoTextInteraction);
    m_metaSep->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_metaSep->setVisible(false);

    m_metaCaption = new QLabel(m_bottomZone);
    m_metaCaption->setObjectName(QStringLiteral("metaCaption"));
    m_metaCaption->setTextInteractionFlags(Qt::NoTextInteraction);
    m_metaCaption->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    m_metaValue = new QLabel(m_bottomZone);
    m_metaValue->setObjectName(QStringLiteral("metaValue"));
    m_metaValue->setTextInteractionFlags(Qt::NoTextInteraction);
    m_metaValue->setWordWrap(false);
    m_metaValue->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_metaValue->setMinimumWidth(0);
    m_metaValue->setMaximumWidth(QWIDGETSIZE_MAX);

    m_trafficChip = new QFrame(m_bottomZone);
    m_trafficChip->setObjectName(QStringLiteral("trafficChip"));
    m_trafficChip->setFrameShape(QFrame::NoFrame);
    m_trafficChip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* chipLay = new QHBoxLayout(m_trafficChip);
    chipLay->setContentsMargins(7, 2, 7, 2);
    chipLay->setSpacing(0);
    m_statsValue = new QLabel(m_trafficChip);
    m_statsValue->setObjectName(QStringLiteral("statsValue"));
    m_statsValue->setTextInteractionFlags(Qt::NoTextInteraction);
    m_statsValue->setAlignment(Qt::AlignCenter);
    m_statsValue->setWordWrap(false);
    chipLay->addWidget(m_statsValue);
    m_trafficChip->setVisible(false);

    m_generateBtn = new QPushButton(QStringLiteral("Agent"), m_bottomZone);
    m_generateBtn->setObjectName(QStringLiteral("ControlCardBtnAgent"));
    m_generateBtn->setCursor(Qt::PointingHandCursor);
    m_generateBtn->setFocusPolicy(Qt::NoFocus);
    m_generateBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_generateBtn->setToolTip(QStringLiteral("Generate Agent"));
    m_generateBtn->setVisible(false);
    connect(m_generateBtn, &QPushButton::clicked, this, [this]() {
        Q_EMIT generateClicked(m_id);
    });

    m_actionSep = new QFrame(m_bottomZone);
    m_actionSep->setObjectName(QStringLiteral("actionSep"));
    m_actionSep->setFrameShape(QFrame::NoFrame);
    m_actionSep->setVisible(false);
    m_actionSep->hide();

    m_primaryBtn = new QPushButton(m_bottomZone);
    m_primaryBtn->setObjectName(QStringLiteral("ControlCardBtnPrimary"));
    m_primaryBtn->setCursor(Qt::PointingHandCursor);
    m_primaryBtn->setFocusPolicy(Qt::NoFocus);
    m_primaryBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_primaryBtn, &QPushButton::clicked, this, [this]() {
        Q_EMIT primaryActionClicked(m_id);
    });

    m_deleteBtn = new QPushButton(QStringLiteral("Remove"), m_bottomZone);
    m_deleteBtn->setObjectName(QStringLiteral("ControlCardBtnRemove"));
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setFocusPolicy(Qt::NoFocus);
    m_deleteBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(m_deleteBtn, &QPushButton::clicked, this, [this]() {
        Q_EMIT deleteClicked(m_id);
    });

    m_bottomLayout->addWidget(m_metaLead, 0);
    m_bottomLayout->addWidget(m_metaSep, 0);
    m_bottomLayout->addWidget(m_metaCaption, 0);
    m_bottomLayout->addWidget(m_metaValue, 1);
    m_bottomLayout->addWidget(m_trafficChip, 0);
    m_bottomLayout->addWidget(m_generateBtn, 0);
    m_bottomLayout->addWidget(m_primaryBtn, 0);
    m_bottomLayout->addWidget(m_deleteBtn, 0);

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(12, 16, 10, 6);
    m_rootLayout->setSpacing(5);
    m_rootLayout->addWidget(m_factsZone, 0);
    m_rootLayout->addWidget(m_bottomZone, 0);

    setPrimaryAction(ActionStop);
    applyTypography();
    applyDimStyle();
}

void ControlCard::setBodyLayout(BodyLayout layout)
{
    m_bodyLayout = layout;
    applyBodyVisibility();
    applyTypography();
}

void ControlCard::setContentStyle(ContentStyle style)
{
    m_contentStyle = style;
    applyContentStyle();
    applyDimStyle();
    updateElidedTexts();
    update();
}

void ControlCard::applyBodyVisibility()
{
    const bool hasPrimary = !m_fullPrimary.isEmpty();
    const bool hasDetail  = !m_fullDetail.isEmpty();
    const bool hasSide    = !m_fullSide.isEmpty();
    const bool hasDate    = !m_fullDate.isEmpty();
    const bool hasLead    = !m_fullSecondaryLead.isEmpty();
    const bool hasMeta    = !m_fullSecondary.isEmpty() || hasLead;
    const bool hasTraffic = !m_fullTertiary.isEmpty();
    const bool hasBottom  = hasMeta || hasTraffic || (m_generateBtn && m_generateBtn->isVisible()) || (m_primaryBtn && m_primaryBtn->isVisible()) || (m_deleteBtn && m_deleteBtn->isVisible());

    m_primaryTile->setVisible(hasPrimary);
    m_detailTile->setVisible(hasDetail);
    if (m_tagChip)
        m_tagChip->setVisible(hasSide);
    if (m_sideLabel) {
        m_sideLabel->setText(m_fullSide);
        m_sideLabel->setToolTip(m_fullSide);
    }
    if (m_dateLabel) {
        m_dateLabel->setVisible(hasDate);
        m_dateLabel->setText(m_fullDate);
        m_dateLabel->setToolTip(m_fullDate);
    }
    m_factsZone->setVisible(hasPrimary || hasDetail || hasSide || hasDate);

    if (m_factsSpacer)
        m_factsSpacer->setVisible(false);

    if (m_metaLead) {
        m_metaLead->setVisible(hasLead);
        m_metaLead->setText(m_fullSecondaryLead);
        m_metaLead->setToolTip(m_fullSecondaryLead);
    }
    const bool showCap = !m_secondaryPrefix.isEmpty() && (!m_fullSecondary.isEmpty() || hasLead);
    if (m_metaSep)
        m_metaSep->setVisible(hasLead && !m_fullSecondary.isEmpty() && !showCap);
    m_metaCaption->setVisible(showCap);
    if (showCap)
        m_metaCaption->setText(m_secondaryPrefix);
    m_metaValue->setVisible(!m_fullSecondary.isEmpty());
    m_trafficChip->setVisible(hasTraffic);
    m_statsValue->setVisible(hasTraffic);
    m_bottomZone->setVisible(hasBottom);
}

int ControlCard::titleBandHeight() const
{
    const AppTypography& ty = FontManager::instance().typography();
    QFont titleF = ty.primary;
    titleF.setPointSizeF(titleF.pointSizeF() + 1.0);
    titleF.setWeight(QFont::Bold);
    return qMax(18, QFontMetrics(titleF).height() + 4);
}

QColor ControlCard::styleAccent() const
{
    const CardTheme th = resolveCardTheme(this);
    return th.primary().isValid() ? th.primary() : th.info();
}

QColor ControlCard::styleSurfaceTint(const QColor& bg, bool darkUi) const
{
    QColor accent = styleAccent();
    const qreal t = darkUi ? 0.045 : 0.03;
    return QColor::fromRgb(
        int(bg.red()   * (1.0 - t) + accent.red()   * t),
        int(bg.green() * (1.0 - t) + accent.green() * t),
        int(bg.blue()  * (1.0 - t) + accent.blue()  * t));
}

QColor ControlCard::widgetBackdrop() const
{
    for (QWidget* w = parentWidget(); w; w = w->parentWidget()) {
        if (qobject_cast<QScrollArea*>(w) || w->objectName() == QLatin1String("ControlCardHost")) {
            QColor c = w->palette().color(QPalette::Window);
            if (!c.isValid() || c.alpha() == 0)
                c = w->palette().color(QPalette::Base);
            if (c.isValid() && c.alpha() > 0)
                return c;
        }
        if (w == parentWidget()) {
            QColor c = w->palette().color(QPalette::Window);
            if (!c.isValid() || c.alpha() == 0)
                c = w->palette().color(QPalette::Base);
            if (c.isValid() && c.alpha() > 0)
                return c;
        }
    }
    return resolveCardTheme(this).bg();
}

void ControlCard::applyContentStyle()
{
    const AppTypography& ty = FontManager::instance().typography();

    QFont cap = ty.micro;
    cap.setWeight(QFont::DemiBold);
    cap.setLetterSpacing(QFont::PercentageSpacing, 104);
    m_metaCaption->setFont(cap);
    m_primaryTile->setCaptionFont(cap);
    m_detailTile->setCaptionFont(cap);

    QFont sideF = ty.caption;
    if (m_sideLabel)
        m_sideLabel->setFont(sideF);
    QFont dateF = ty.caption;
    m_dateLabel->setFont(dateF);

    QFont typeFont = ty.mono;
    typeFont.setWeight(QFont::Normal);
    QFont bindFont = ty.mono;
    bindFont.setWeight(QFont::DemiBold);

    switch (m_contentStyle) {
    case StyleListener: {
        m_primaryTile->setValueFont(typeFont);
        m_primaryTile->setMonoValue(true);
        m_detailTile->setValueFont(bindFont);
        m_detailTile->setMonoValue(true);
        m_metaCaption->setFont(cap);
        QFont lead = ty.body;
        lead.setWeight(QFont::DemiBold);
        if (m_metaLead)
            m_metaLead->setFont(lead);
        if (m_metaSep)
            m_metaSep->setFont(ty.caption);
        QFont meta = ty.body;
        meta.setPointSizeF(meta.pointSizeF() - 0.5);
        meta.setWeight(QFont::Medium);
        m_metaValue->setFont(meta);
        m_statsValue->setFont(ty.caption);
        break;
    }
    case StyleTunnel: {
        m_primaryTile->setValueFont(bindFont);
        m_primaryTile->setMonoValue(true);
        m_detailTile->setValueFont(bindFont);
        m_detailTile->setMonoValue(true);
        m_metaCaption->setFont(cap);
        QFont lead = ty.body;
        lead.setWeight(QFont::DemiBold);
        if (m_metaLead)
            m_metaLead->setFont(lead);
        if (m_metaSep)
            m_metaSep->setFont(ty.caption);
        QFont meta = ty.body;
        meta.setPointSizeF(meta.pointSizeF() - 0.5);
        meta.setWeight(QFont::Medium);
        m_metaValue->setFont(meta);
        QFont traffic = ty.mono;
        traffic.setWeight(QFont::DemiBold);
        m_statsValue->setFont(traffic);
        break;
    }
    default:
        m_primaryTile->setValueFont(ty.primary);
        m_detailTile->setValueFont(ty.mono);
        m_metaValue->setFont(ty.body);
        m_statsValue->setFont(ty.caption);
        break;
    }
}

void ControlCard::applyButtonStyles()
{
    const CardTheme th = resolveCardTheme(this);
    if (m_generateBtn)
        m_generateBtn->setPalette(palette());
    if (m_primaryBtn)
        m_primaryBtn->setPalette(palette());
    if (m_deleteBtn)
        m_deleteBtn->setPalette(palette());

    QColor accent = styleAccent();
    QColor text = m_dimmed ? th.textDead() : th.text();
    QColor outline = accent.isValid() ? accent : (th.primary().isValid() ? th.primary() : th.border());
    if (m_dimmed) {
        outline.setAlpha(120);
        text = th.textDead();
    } else {
        outline.setAlpha(th.dark ? 180 : 160);
    }

    QColor hover = th.primary().isValid() ? th.primary() : accent;
    QColor press = th.primary().isValid() ? th.primary() : accent;
    press.setAlpha(th.dark ? 45 : 35);

    styleOutlineButton(m_generateBtn, outline, text, hover, press);
    styleOutlineButton(m_primaryBtn, outline, text, hover, press);
    styleOutlineButton(m_deleteBtn, outline, text, hover, press);

    if (m_actionSep)
        m_actionSep->setVisible(false);
}

void ControlCard::applyZoneChrome()
{
    const CardTheme th = resolveCardTheme(this);
    const QColor accent = styleAccent();
    const QPalette pal = palette();

    QColor panelBg = th.bgElevated().isValid() ? th.bgElevated() : th.bg();
    if (m_dimmed) {
        panelBg = pal.color(QPalette::Disabled, QPalette::Base);
        if (!panelBg.isValid() || panelBg.alpha() == 0)
            panelBg = th.dark ? th.bg().darker(145) : th.bg().darker(120);
    } else if (th.dark) {
        panelBg = panelBg.lighter(108);
    } else {
        panelBg = panelBg.darker(102);
    }

    QColor panelBorder = accent.isValid() ? accent : th.border();
    if (m_dimmed) {
        panelBorder = pal.color(QPalette::Disabled, QPalette::Mid);
        if (!panelBorder.isValid() || panelBorder.alpha() == 0)
            panelBorder = th.border();
        panelBorder.setAlpha(th.dark ? 70 : 85);
    } else {
        panelBorder.setAlpha(th.dark ? 100 : 90);
    }

    QColor fieldCap = m_dimmed ? pal.color(QPalette::Disabled, QPalette::WindowText) : th.textMuted();
    if (!fieldCap.isValid() || fieldCap.alpha() == 0)
        fieldCap = th.textMuted();

    QColor valueCol = m_dimmed ? pal.color(QPalette::Disabled, QPalette::Text) : th.text();
    if (!valueCol.isValid() || valueCol.alpha() == 0)
        valueCol = m_dimmed ? th.textDead() : th.text();
    if (m_dimmed)
        valueCol.setAlpha(qMin(valueCol.alpha(), 110));

    QColor muted = m_dimmed ? pal.color(QPalette::Disabled, QPalette::WindowText) : th.textMuted();
    if (m_dimmed)
        muted.setAlpha(qMin(muted.alpha(), 95));

    QColor primaryVal = valueCol;
    QColor detailVal = valueCol;

    m_primaryTile->applyColors(fieldCap, primaryVal, panelBg, panelBorder);
    m_detailTile->applyColors(fieldCap, detailVal, panelBg, panelBorder);

    auto setC = [](QLabel* lb, const QColor& c) {
        if (!lb)
            return;
        QPalette p = lb->palette();
        p.setColor(QPalette::WindowText, c);
        p.setColor(QPalette::Text, c);
        lb->setPalette(p);
        lb->setForegroundRole(QPalette::WindowText);
    };
    setC(m_metaCaption, fieldCap);
    setC(m_metaLead, valueCol);
    setC(m_metaSep, muted);
    setC(m_metaValue, valueCol);
    setC(m_dateLabel, muted);

    QColor tagBorder = th.tagBorder().isValid() ? th.tagBorder() : th.border();
    QColor tagText = m_dimmed ? valueCol : (th.tagText().isValid() ? th.tagText() : th.text());
    QColor tagBg = m_dimmed ? panelBg : th.bgElevated();
    if (!m_dimmed) {
        if (th.dark)
            tagBg = tagBg.lighter(108);
        else
            tagBg = tagBg.darker(103);
    }
    tagBorder.setAlpha(th.dark ? (m_dimmed ? 55 : 100) : (m_dimmed ? 65 : 95));
    if (m_tagChip) {
        m_tagChip->setStyleSheet(QStringLiteral(
            "QFrame#tagChip {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 4px;"
            "}"
        ).arg(cssColor(tagBg), cssColor(tagBorder)));
    }
    setC(m_sideLabel, tagText);

    QColor chipBg = accent.isValid() ? accent : th.primary();
    chipBg.setAlpha(th.dark ? (m_dimmed ? 18 : 40) : (m_dimmed ? 16 : 32));
    QColor chipBorder = accent.isValid() ? accent : th.primary();
    chipBorder.setAlpha(th.dark ? (m_dimmed ? 50 : 100) : (m_dimmed ? 45 : 95));
    QColor chipText = m_dimmed ? valueCol : (accent.isValid() ? accent : valueCol);

    if (m_trafficChip) {
        m_trafficChip->setStyleSheet(QStringLiteral(
            "QFrame#trafficChip {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 4px;"
            "}"
        ).arg(cssColor(chipBg), cssColor(chipBorder)));
    }
    setC(m_statsValue, chipText);
}

void ControlCard::applyTypography()
{
    const AppTypography& ty = FontManager::instance().typography();
    applyContentStyle();

    const int btnH = qMax(20, ty.controlInnerH - 1);
    QFont btnFont = ty.regular;
    if (btnFont.pointSize() <= 0 && ty.chromeFontPx > 0)
        btnFont.setPixelSize(ty.chromeFontPx);
    QFontMetrics fm(btnFont);
    int btnW = qMax(56, ty.controlHeight + 12);
    for (QPushButton* b : { m_generateBtn, m_primaryBtn, m_deleteBtn }) {
        if (!b)
            continue;
        const QString label = b->text().isEmpty() ? QStringLiteral("Resume") : b->text();
        btnW = qMax(btnW, fm.horizontalAdvance(label) + 24);
    }
    for (QPushButton* b : { m_generateBtn, m_primaryBtn, m_deleteBtn }) {
        if (!b)
            continue;
        b->setFixedHeight(btnH);
        b->setFixedWidth(btnW);
    }
    if (m_actionSep)
        m_actionSep->setVisible(false);
    applyButtonStyles();

    if (m_trafficChip)
        m_trafficChip->setFixedHeight(btnH);
    if (m_tagChip)
        m_tagChip->setFixedHeight(btnH);

    const int band = titleBandHeight();
    int h = band + 8;
    h += qMax(ty.lineH1, 22) + 4;
    h += qMax(btnH, ty.lineH2) + 12;
    setMinimumHeight(h);
    setMaximumHeight(h + 14);

    if (m_rootLayout) {
        m_rootLayout->setContentsMargins(12, band + 4, 10, 6);
        m_rootLayout->setSpacing(5);
    }

    applyBodyVisibility();
    applyZoneChrome();
    updateElidedTexts();
    updateGeometry();
    update();
}

void ControlCard::setCardId(const QVariant& id)
{
    m_id = id;
    ControlCardData d;
    d.id = id;
    m_key = d.key();
}

void ControlCard::setTitle(const QString& title)
{
    m_fullTitle = title;
    QString tip = title;
    if (!m_fullTitleSuffix.isEmpty())
        tip += QStringLiteral(" [") + m_fullTitleSuffix + QLatin1Char(']');
    setToolTip(tip);
    updateElidedTexts();
    update();
}

void ControlCard::setTitleSuffix(const QString& suffix)
{
    m_fullTitleSuffix = suffix.trimmed();
    QString tip = m_fullTitle;
    if (!m_fullTitleSuffix.isEmpty())
        tip += QStringLiteral(" [") + m_fullTitleSuffix + QLatin1Char(']');
    setToolTip(tip);
    updateElidedTexts();
    update();
}

void ControlCard::setStatus(const QString& status, bool active)
{
    m_active = active;
    m_fullStatus = status.isEmpty() ? (active ? QStringLiteral("Active") : QStringLiteral("Stopped")) : status;
    updateElidedTexts();
    update();
}

void ControlCard::setPrimary(const QString& text)
{
    m_fullPrimary = text;
    m_primaryTile->setValue(text);
    applyBodyVisibility();
    updateElidedTexts();
}

void ControlCard::setDetail(const QString& text)
{
    m_fullDetail = text;
    m_detailTile->setValue(text);
    applyBodyVisibility();
    updateElidedTexts();
    applyTypography();
}

void ControlCard::setSecondary(const QString& text)
{
    m_fullSecondary = text;
    m_metaValue->setToolTip(text);
    applyBodyVisibility();
    updateElidedTexts();
}

void ControlCard::setSecondaryLead(const QString& text)
{
    m_fullSecondaryLead = text.trimmed();
    applyBodyVisibility();
    updateElidedTexts();
}

void ControlCard::setTertiary(const QString& text)
{
    m_fullTertiary = text;
    m_statsValue->setToolTip(text);
    applyBodyVisibility();
    updateElidedTexts();
    applyTypography();
}

void ControlCard::setSideText(const QString& text)
{
    m_fullSide = text.trimmed();
    applyBodyVisibility();
    updateElidedTexts();
}

void ControlCard::setDateText(const QString& text)
{
    m_fullDate = text.trimmed();
    applyBodyVisibility();
    updateElidedTexts();
}

void ControlCard::setPrimaryPrefix(const QString& prefix)
{
    m_primaryPrefix = prefix.trimmed().toUpper();
    m_primaryTile->setCaption(m_primaryPrefix);
}

void ControlCard::setDetailPrefix(const QString& prefix)
{
    m_detailPrefix = prefix.trimmed().toUpper();
    m_detailTile->setCaption(m_detailPrefix);
}

void ControlCard::setSecondaryPrefix(const QString& prefix)
{
    m_secondaryPrefix = prefix.trimmed().toUpper();
    m_metaCaption->setText(m_secondaryPrefix);
    applyBodyVisibility();
}

void ControlCard::setTertiaryPrefix(const QString& prefix)
{
    m_tertiaryPrefix = prefix.trimmed().toUpper();
    if (m_trafficChip && !m_tertiaryPrefix.isEmpty())
        m_trafficChip->setToolTip(m_tertiaryPrefix);
    applyBodyVisibility();
}

void ControlCard::updateElidedTexts()
{
    const AppTypography& ty = FontManager::instance().typography();
    QFont titleF = ty.primary;
    titleF.setPointSizeF(titleF.pointSizeF() + 1.0);
    titleF.setWeight(QFont::Bold);
    QFont statusF = ty.body;
    statusF.setPointSizeF(qMax(statusF.pointSizeF(), ty.caption.pointSizeF() + 1.0));
    statusF.setWeight(QFont::DemiBold);
    QFont suffixF = titleF;
    suffixF.setPointSizeF(qMax(8.0, titleF.pointSizeF() - 1.0));
    suffixF.setWeight(QFont::Medium);
    QFontMetrics titleFm(titleF);
    QFontMetrics statusFm(statusF);
    QFontMetrics suffixFm(suffixF);

    const int cardW = qMax(1, width());

    const QString statusFull = m_active ? (QStringLiteral("●  ") + m_fullStatus) : (QStringLiteral("○  ") + m_fullStatus);
    const int statusNatural = qMax(1, statusFm.horizontalAdvance(statusFull) + 2);
    const int statusCap = qBound(qMin(36, statusNatural), qMin(cardW / 3, statusNatural), statusNatural);
    m_statusDrawn = statusFm.elidedText(statusFull, Qt::ElideRight, statusCap);
    const int statusW = statusFm.horizontalAdvance(m_statusDrawn) + 16;

    int titleBudget = qMax(40, cardW - statusW - 36);
    m_titleSuffixDrawn.clear();
    if (!m_fullTitleSuffix.isEmpty()) {
        const QString sufFull = QStringLiteral(" [") + m_fullTitleSuffix + QLatin1Char(']');
        const int sufNatural = qMax(1, suffixFm.horizontalAdvance(sufFull));
        const int sufMax = qBound(qMin(20, sufNatural), qMin(titleBudget / 2, sufNatural), sufNatural);
        m_titleSuffixDrawn = suffixFm.elidedText(sufFull, Qt::ElideRight, sufMax);
        titleBudget -= suffixFm.horizontalAdvance(m_titleSuffixDrawn);
    }
    m_titleDrawn = titleFm.elidedText(m_fullTitle, Qt::ElideRight, qMax(24, titleBudget));

    if (m_primaryTile)
        m_primaryTile->reflow(0);
    if (m_detailTile)
        m_detailTile->reflow(0);

    if (m_metaValue) {
        int reserved = 0;
        auto addVis = [&](QWidget* w) {
            if (w && w->isVisible())
                reserved += w->sizeHint().width() + (m_bottomLayout ? m_bottomLayout->spacing() : 8);
        };
        addVis(m_metaLead);
        addVis(m_metaSep);
        addVis(m_metaCaption);
        addVis(m_trafficChip);
        addVis(m_generateBtn);
        addVis(m_primaryBtn);
        addVis(m_deleteBtn);
        int margins = 0;
        if (m_bottomLayout) {
            const auto m = m_bottomLayout->contentsMargins();
            margins = m.left() + m.right();
        }
        int zoneW = m_bottomZone ? m_bottomZone->width() : 0;
        if (zoneW < 8)
            zoneW = qMax(8, cardW - 22);
        const int avail = qMax(20, zoneW - reserved - margins);
        m_metaValue->setMaximumWidth(avail);
        if (m_fullSecondary.isEmpty()) {
            m_metaValue->clear();
            m_metaValue->setToolTip(QString());
        } else {
            QFontMetrics fm(m_metaValue->font());
            m_metaValue->setText(fm.elidedText(m_fullSecondary, Qt::ElideRight, avail));
            m_metaValue->setToolTip(m_fullSecondary);
        }
    }

    if (m_statsValue) {
        if (m_fullTertiary.isEmpty()) {
            m_statsValue->clear();
        } else {
            m_statsValue->setText(m_fullTertiary);
            m_statsValue->setToolTip(
                m_tertiaryPrefix.isEmpty()
                    ? m_fullTertiary
                    : (m_tertiaryPrefix + QStringLiteral(": ") + m_fullTertiary));
        }
    }
}

void ControlCard::reflowText()
{
    updateElidedTexts();
    update();
}

void ControlCard::setDimmed(bool dimmed)
{
    m_dimmed = dimmed;
    applyDimStyle();
}

void ControlCard::setPrimaryAction(PrimaryAction action, const QString& label)
{
    if (action == ActionNone) {
        m_primaryBtn->setVisible(false);
        applyTypography();
        return;
    }
    m_primaryBtn->setVisible(true);
    if (!label.isEmpty())
        m_primaryBtn->setText(label);
    else if (action == ActionStart)
        m_primaryBtn->setText(QStringLiteral("Start"));
    else
        m_primaryBtn->setText(QStringLiteral("Stop"));
    m_primaryBtn->setToolTip(m_primaryBtn->text());
    applyTypography();
}

void ControlCard::setDeleteVisible(bool visible)
{
    m_deleteBtn->setVisible(visible);
    applyTypography();
}

void ControlCard::setDeleteLabel(const QString& label)
{
    if (!label.isEmpty())
        m_deleteBtn->setText(label);
    m_deleteBtn->setToolTip(m_deleteBtn->text());
    applyTypography();
}

void ControlCard::setGenerateVisible(bool visible)
{
    if (m_generateBtn)
        m_generateBtn->setVisible(visible);
    if (m_actionSep)
        m_actionSep->setVisible(false);
    applyTypography();
    applyBodyVisibility();
}

void ControlCard::setGenerateLabel(const QString& label)
{
    if (!m_generateBtn)
        return;
    if (!label.isEmpty())
        m_generateBtn->setText(label);
    m_generateBtn->setToolTip(QStringLiteral("Generate Agent"));
}

void ControlCard::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void ControlCard::applyDimStyle()
{
    if (graphicsEffect())
        setGraphicsEffect(nullptr);
    applyZoneChrome();
    applyButtonStyles();
    setEnabled(true);
    update();
}

void ControlCard::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const CardTheme th = resolveCardTheme(this);
    const AppTypography& ty = FontManager::instance().typography();

    QFont titleFont = ty.primary;
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1.0);
    titleFont.setWeight(QFont::Bold);
    QFont statusFont = ty.body;
    statusFont.setPointSizeF(qMax(statusFont.pointSizeF(), ty.caption.pointSizeF() + 1.0));
    statusFont.setWeight(QFont::DemiBold);
    QFontMetrics titleFm(titleFont);
    QFontMetrics statusFm(statusFont);

    const qreal radius = (m_contentStyle == StyleTunnel) ? 7.0 : 9.0;
    const qreal penW = 1.0;
    const int band = titleBandHeight();
    const qreal topY = band * 0.5;

    QColor bg = th.bgElevated().isValid() ? th.bgElevated() : th.bg();
    if (!bg.isValid())
        bg = palette().color(QPalette::Base);
    const bool darkUi = th.dark;
    if (m_dimmed) {
        QColor dis = palette().color(QPalette::Disabled, QPalette::Base);
        if (dis.isValid() && dis.alpha() > 0)
            bg = dis;
        else
            bg = darkUi ? bg.darker(155) : bg.darker(125);
        float h, s, l, a;
        bg.getHslF(&h, &s, &l, &a);
        s *= 0.35f;
        if (darkUi)
            l = qMax(0.08f, l * 0.75f);
        else
            l = qMin(0.92f, l * 1.05f);
        bg = QColor::fromHslF(h, s, l, a);
    } else {
        if (darkUi)
            bg = bg.lighter(106);
        else
            bg = bg.darker(101);
        bg = styleSurfaceTint(bg, darkUi);
    }

    QColor border = th.border().isValid() ? th.border() : (darkUi ? bg.lighter(130) : bg.darker(125));
    if (m_dimmed) {
        border = palette().color(QPalette::Disabled, QPalette::Mid);
        if (!border.isValid() || border.alpha() == 0)
            border = th.border();
        border.setAlpha(darkUi ? 55 : 70);
    } else {
        QColor acc = styleAccent();
        const qreal t = darkUi ? 0.10 : 0.08;
        border = QColor::fromRgb(
            int(border.red()   * (1.0 - t) + acc.red()   * t),
            int(border.green() * (1.0 - t) + acc.green() * t),
            int(border.blue()  * (1.0 - t) + acc.blue()  * t),
            darkUi ? 95 : 110);
    }
    if (m_hover && !m_selected) {
        QColor acc = styleAccent();
        if (acc.isValid()) {
            border = acc;
            border.setAlpha(m_dimmed ? 80 : (darkUi ? 150 : 140));
        } else {
            border = darkUi ? border.lighter(118) : border.darker(112);
            border.setAlpha(qMin(180, border.alpha() + 40));
        }
    }

    QColor titleColor = m_dimmed ? palette().color(QPalette::Disabled, QPalette::WindowText) : th.text();
    if (!titleColor.isValid() || titleColor.alpha() == 0)
        titleColor = m_dimmed ? th.textDead() : th.text();
    if (m_dimmed)
        titleColor.setAlpha(qMin(titleColor.alpha(), 100));

    if (m_selected) {
        QColor hi = th.bgSelected().isValid() ? th.bgSelected() : th.primary();
        if (!hi.isValid())
            hi = styleAccent();
        const qreal t = darkUi ? 0.16 : 0.11;
        bg = QColor::fromRgb(
            int(bg.red()   * (1.0 - t) + hi.red()   * t),
            int(bg.green() * (1.0 - t) + hi.green() * t),
            int(bg.blue()  * (1.0 - t) + hi.blue()  * t));
        border = hi.isValid() ? hi : styleAccent();
        border.setAlpha(darkUi ? 170 : 160);
    }

    QColor statusColor = m_active ? (th.success().isValid() ? th.success() : th.primary()) : (th.warning().isValid() ? th.warning() : th.error());
    if (m_dimmed && !m_active) {
        statusColor = th.warning().isValid() ? th.warning() : th.error();
        statusColor.setAlpha(200);
    }

    const QString titleText = m_titleDrawn.isEmpty() ? m_fullTitle : m_titleDrawn;
    const QString titleSuffixText = m_titleSuffixDrawn;
    const QString statusText = m_statusDrawn.isEmpty()
        ? (m_active ? (QStringLiteral("●  ") + m_fullStatus)
                    : (QStringLiteral("○  ") + m_fullStatus))
        : m_statusDrawn;

    QFont suffixFont = titleFont;
    suffixFont.setPointSizeF(qMax(8.0, titleFont.pointSizeF() - 1.0));
    suffixFont.setWeight(QFont::Medium);
    QFontMetrics suffixFm(suffixFont);

    const int titlePad = 6;
    const int titleMainW = titleFm.horizontalAdvance(titleText);
    const int titleSufW = titleSuffixText.isEmpty() ? 0 : suffixFm.horizontalAdvance(titleSuffixText);
    const int titleW = titleMainW + titleSufW;
    const int statusW = statusFm.horizontalAdvance(statusText);

    const qreal inset = penW * 0.5 + 0.5;
    const qreal left = inset;
    const qreal right = width() - inset;
    const qreal bottom = height() - inset;
    const qreal titleLeft = 14.0;
    const qreal titleGapL = titleLeft - titlePad;
    const qreal titleGapR = titleLeft + titleW + titlePad;
    const qreal statusRight = right - 12.0;
    qreal statusGapL = statusRight - statusW - titlePad;
    if (statusGapL < titleGapR + 8.0)
        statusGapL = qMin(statusRight - statusW - titlePad, right - statusW - titlePad - 4.0);
    const qreal statusGapR = statusGapL + statusW + 2 * titlePad;

    QRectF fillR(left, topY, right - left, bottom - topY);

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(fillR, radius, radius);

    QPen pen(border, penW);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(fillR, radius, radius);

    {
        QColor strip = styleAccent();
        strip.setAlpha(m_dimmed ? 60 : (m_active ? 150 : 90));
        QRectF stripR(fillR.left() + 1.5, fillR.top() + radius * 0.55, 2.5, fillR.height() - radius * 1.1);
        p.setPen(Qt::NoPen);
        p.setBrush(strip);
        p.drawRoundedRect(stripR, 1.2, 1.2);
    }

    if (m_bottomZone && m_bottomZone->isVisible() && m_factsZone && m_factsZone->isVisible()) {
        const int y = m_bottomZone->geometry().top() - 2;
        if (y > int(topY) + 8) {
            QColor line = th.separator().isValid() ? th.separator() : th.border();
            line.setAlpha(darkUi ? (m_dimmed ? 14 : 28) : (m_dimmed ? 12 : 22));
            p.setPen(QPen(line, 1.0));
            p.drawLine(int(left) + 14, y, int(right) - 12, y);
        }
    }

    const QColor notchBg = widgetBackdrop().isValid() ? widgetBackdrop() : th.bg();
    p.setPen(Qt::NoPen);
    p.setBrush(notchBg);
    const qreal gapH = penW + 3.0;
    if (titleW > 0)
        p.drawRect(QRectF(titleGapL, topY - gapH * 0.5, titleGapR - titleGapL, gapH));
    if (statusW > 0)
        p.drawRect(QRectF(statusGapL, topY - gapH * 0.5, statusGapR - statusGapL, gapH));

    p.setBrush(notchBg);
    if (titleW > 0) {
        p.drawRoundedRect(QRectF(titleGapL - 1, 1, titleGapR - titleGapL + 2, band - 1), 3, 3);
    }
    if (statusW > 0) {
        p.drawRoundedRect(QRectF(statusGapL - 1, 1, statusGapR - statusGapL + 2, band - 1), 3, 3);
    }

    p.setFont(titleFont);
    p.setPen(titleColor);
    p.drawText(QRect(int(titleLeft), 0, titleMainW + 2, band), Qt::AlignLeft | Qt::AlignVCenter, titleText);
    if (!titleSuffixText.isEmpty()) {
        QColor sufCol = titleColor;
        sufCol.setAlpha(qMax(90, titleColor.alpha() - 40));
        p.setFont(suffixFont);
        p.setPen(sufCol);
        p.drawText(QRect(int(titleLeft) + titleMainW, 0, titleSufW + 2, band), Qt::AlignLeft | Qt::AlignVCenter, titleSuffixText);
    }

    p.setFont(statusFont);
    p.setPen(statusColor);
    p.drawText(QRect(int(statusGapL + titlePad), 0, statusW + 2, band), Qt::AlignLeft | Qt::AlignVCenter, statusText);
}

void ControlCard::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    updateElidedTexts();
}

void ControlCard::mouseDoubleClickEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
        Q_EMIT doubleClicked(m_id);
    QWidget::mouseDoubleClickEvent(e);
}

void ControlCard::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton)
        Q_EMIT selected(m_id, e->modifiers());
    QWidget::mousePressEvent(e);
}

void ControlCard::contextMenuEvent(QContextMenuEvent* e)
{
    Q_EMIT contextMenuRequested(m_id, e->globalPos());
}

bool ControlCard::event(QEvent* e)
{
    if (e->type() == QEvent::HoverEnter) {
        m_hover = true;
        update();
    } else if (e->type() == QEvent::HoverLeave) {
        m_hover = false;
        update();
    } else if (e->type() == QEvent::PaletteChange || e->type() == QEvent::StyleChange || e->type() == QEvent::ApplicationPaletteChange) {
        applyButtonStyles();
        applyDimStyle();
    }
    return QWidget::event(e);
}

ControlCardList::ControlCardList(QWidget* parent) : QScrollArea(parent)
{
    setObjectName(QStringLiteral("ControlCardList"));
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(nullptr); // list itself receives arrow keys

    m_host = new QWidget(this);
    m_host->setObjectName(QStringLiteral("ControlCardHost"));
    m_grid = new QGridLayout(m_host);
    m_grid->setContentsMargins(12, 12, 12, 12);
    m_grid->setHorizontalSpacing(12);
    m_grid->setVerticalSpacing(12);
    m_grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    setWidget(m_host);

    const CardTheme th = resolveCardTheme(this);
    QColor panel = th.bg();
    if (!panel.isValid())
        panel = palette().color(QPalette::Window);
    auto paintPanel = [panel](QWidget* w) {
        if (!w)
            return;
        w->setAutoFillBackground(true);
        QPalette p = w->palette();
        p.setColor(QPalette::Window, panel);
        p.setColor(QPalette::Base, panel);
        w->setPalette(p);
        w->setBackgroundRole(QPalette::Window);
    };
    paintPanel(this);
    paintPanel(viewport());
    paintPanel(m_host);

    connect(&FontManager::instance(), &FontManager::typographyChanged, this, &ControlCardList::applyTypography);
}

void ControlCardList::applyTypography()
{
    const CardTheme th = resolveCardTheme(this);
    QColor panel = th.bg();
    if (panel.isValid()) {
        auto paintPanel = [panel](QWidget* w) {
            if (!w)
                return;
            w->setAutoFillBackground(true);
            QPalette p = w->palette();
            p.setColor(QPalette::Window, panel);
            p.setColor(QPalette::Base, panel);
            w->setPalette(p);
            w->setBackgroundRole(QPalette::Window);
        };
        paintPanel(this);
        paintPanel(viewport());
        paintPanel(m_host);
    }

    for (auto* c : m_cards)
        c->applyTypography();
    const AppTypography& ty = FontManager::instance().typography();
    m_grid->setHorizontalSpacing(qMax(10, ty.blockGap));
    m_grid->setVerticalSpacing(qMax(10, ty.blockGap));
}

void ControlCardList::clear()
{
    for (auto* c : m_cards)
        delete c;
    m_cards.clear();
    m_data.clear();
    m_order.clear();
    m_selectedId = QVariant();
    m_selectedKeys.clear();
    m_anchorKey.clear();
    m_currentKey.clear();
}

void ControlCardList::setCards(const QVector<ControlCardData>& cards)
{
    const QSet<QString> prevSel = m_selectedKeys;
    const QString prevCurrent = m_currentKey;
    const QString prevAnchor = m_anchorKey;
    const QVariant prevPrimary = m_selectedId;

    clear();
    for (const auto& d : cards)
        upsertCard(d);
    rebuildGrid();

    m_selectedKeys.clear();
    for (const QString& k : prevSel) {
        if (m_cards.contains(k))
            m_selectedKeys.insert(k);
    }
    if (m_cards.contains(prevCurrent))
        m_currentKey = prevCurrent;
    if (m_cards.contains(prevAnchor))
        m_anchorKey = prevAnchor;
    if (prevPrimary.isValid() && m_cards.contains(keyOf(prevPrimary)))
        m_selectedId = prevPrimary;
    else if (!m_selectedKeys.isEmpty()) {
        m_selectedId = m_data.value(*m_selectedKeys.begin()).id;
    }
    applySelectionVisuals();
}

bool ControlCardList::contains(const QVariant& id) const
{
    return m_cards.contains(keyOf(id));
}

ControlCard* ControlCardList::makeCard(const ControlCardData& data)
{
    auto* card = new ControlCard(m_host);
    applyData(card, data);
    connect(card, &ControlCard::primaryActionClicked, this, &ControlCardList::primaryActionClicked);
    connect(card, &ControlCard::deleteClicked, this, &ControlCardList::deleteClicked);
    connect(card, &ControlCard::generateClicked, this, &ControlCardList::generateClicked);
    connect(card, &ControlCard::doubleClicked, this, &ControlCardList::doubleClicked);
    connect(card, &ControlCard::selected, this, [this](const QVariant& id, Qt::KeyboardModifiers mods) {
        setFocus(Qt::MouseFocusReason);
        selectWithModifiers(id, mods);
        Q_EMIT selectionChanged(id);
    });
    connect(card, &ControlCard::contextMenuRequested, this, &ControlCardList::contextMenuRequested);
    return card;
}

void ControlCardList::applyData(ControlCard* card, const ControlCardData& data)
{
    card->setCardId(data.id);
    card->setBodyLayout(data.bodyLayout);
    card->setContentStyle(data.contentStyle);
    card->setTitle(data.title);
    card->setTitleSuffix(data.titleSuffix);
    card->setStatus(data.status, data.active);
    card->setPrimaryPrefix(data.primaryPrefix);
    card->setDetailPrefix(data.detailPrefix);
    card->setSecondaryPrefix(data.secondaryPrefix);
    card->setTertiaryPrefix(data.tertiaryPrefix);
    card->setPrimary(data.primary);
    card->setDetail(data.detail);
    card->setSecondaryLead(data.secondaryLead);
    card->setSecondary(data.secondary);
    card->setTertiary(data.tertiary);
    card->setSideText(data.sideText);
    card->setDateText(data.dateText);
    card->setDimmed(!data.active);
    if (data.showPrimaryAction)
        card->setPrimaryAction(data.primaryAction, data.primaryActionLabel);
    else
        card->setPrimaryAction(ControlCard::ActionNone);
    card->setDeleteVisible(data.showDelete);
    if (!data.deleteActionLabel.isEmpty())
        card->setDeleteLabel(data.deleteActionLabel);
    card->setGenerateVisible(data.showGenerate);
    if (!data.generateActionLabel.isEmpty())
        card->setGenerateLabel(data.generateActionLabel);
    card->setSelected(m_selectedKeys.contains(data.key()));
    card->applyTypography();
}

void ControlCardList::upsertCard(const ControlCardData& data)
{
    const QString k = data.key();
    m_data[k] = data;
    if (auto* existing = m_cards.value(k, nullptr)) {
        applyData(existing, data);
        return;
    }
    m_order.append(k);
    m_cards[k] = makeCard(data);
    rebuildGrid();
}

void ControlCardList::removeCard(const QVariant& id)
{
    const QString k = keyOf(id);
    if (auto* c = m_cards.take(k))
        delete c;
    m_data.remove(k);
    m_order.removeAll(k);
    m_selectedKeys.remove(k);
    if (m_anchorKey == k)
        m_anchorKey.clear();
    if (m_currentKey == k)
        m_currentKey.clear();
    if (keyOf(m_selectedId) == k) {
        m_selectedId = QVariant();
        if (!m_selectedKeys.isEmpty()) {
            const QString first = *m_selectedKeys.constBegin();
            if (m_data.contains(first))
                m_selectedId = m_data.value(first).id;
        }
    }
    rebuildGrid();
}

void ControlCardList::setSelectedId(const QVariant& id)
{
    selectWithModifiers(id, Qt::NoModifier);
}

QList<QVariant> ControlCardList::selectedIds() const
{
    QList<QVariant> out;
    for (const QString& k : m_order) {
        if (m_selectedKeys.contains(k) && m_data.contains(k))
            out.append(m_data.value(k).id);
    }
    return out;
}

bool ControlCardList::isIdSelected(const QVariant& id) const
{
    return m_selectedKeys.contains(keyOf(id));
}

void ControlCardList::ensureSelected(const QVariant& id)
{
    const QString k = keyOf(id);
    if (!m_cards.contains(k))
        return;
    if (!m_selectedKeys.contains(k)) {
        selectWithModifiers(id, Qt::NoModifier);
    } else {
        m_selectedId = id;
        m_currentKey = k;
    }
}

void ControlCardList::selectWithModifiers(const QVariant& id, Qt::KeyboardModifiers mods)
{
    const QString k = keyOf(id);
    if (!m_cards.contains(k) && !m_data.contains(k))
        return;

    if (mods & Qt::ControlModifier) {
        if (m_selectedKeys.contains(k))
            m_selectedKeys.remove(k);
        else
            m_selectedKeys.insert(k);
        m_currentKey = k;
        m_anchorKey = k;
        m_selectedId = id;
    } else if (mods & Qt::ShiftModifier) {
        if (m_anchorKey.isEmpty() || !m_order.contains(m_anchorKey))
            m_anchorKey = m_currentKey.isEmpty() ? k : m_currentKey;
        int a = m_order.indexOf(m_anchorKey);
        int b = m_order.indexOf(k);
        if (a < 0) a = b;
        m_selectedKeys.clear();
        if (a >= 0 && b >= 0) {
            const int lo = qMin(a, b);
            const int hi = qMax(a, b);
            for (int i = lo; i <= hi; ++i)
                m_selectedKeys.insert(m_order.at(i));
        } else {
            m_selectedKeys.insert(k);
        }
        m_currentKey = k;
        m_selectedId = id;
    } else {
        m_selectedKeys.clear();
        m_selectedKeys.insert(k);
        m_anchorKey = k;
        m_currentKey = k;
        m_selectedId = id;
    }

    applySelectionVisuals();
    ensureCardVisible(k);
}

void ControlCardList::applySelectionVisuals()
{
    for (auto it = m_cards.begin(); it != m_cards.end(); ++it)
        it.value()->setSelected(m_selectedKeys.contains(it.key()));
}

void ControlCardList::ensureCardVisible(const QString& key)
{
    if (auto* c = m_cards.value(key, nullptr))
        ensureWidgetVisible(c, 0, 12);
}

void ControlCardList::moveCurrent(int delta, Qt::KeyboardModifiers mods)
{
    if (m_order.isEmpty())
        return;
    int idx = m_order.indexOf(m_currentKey);
    if (idx < 0)
        idx = (delta > 0) ? -1 : 0;
    int next = qBound(0, idx + delta, m_order.size() - 1);
    if (next < 0 || next >= m_order.size())
        return;
    const QString k = m_order.at(next);
    if (!m_data.contains(k))
        return;
    const QVariant id = m_data.value(k).id;

    if (mods & Qt::ControlModifier) {
        m_currentKey = k;
        if (m_selectedKeys.contains(k)) {
        } else {
            m_selectedKeys.insert(k);
        }
        m_selectedId = id;
        applySelectionVisuals();
        ensureCardVisible(k);
        Q_EMIT selectionChanged(id);
        return;
    }

    selectWithModifiers(id, mods);
    Q_EMIT selectionChanged(id);
}

void ControlCardList::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Up) {
        moveCurrent(-1, e->modifiers());
        e->accept();
        return;
    }
    if (e->key() == Qt::Key_Down) {
        moveCurrent(+1, e->modifiers());
        e->accept();
        return;
    }
    if (e->key() == Qt::Key_A && (e->modifiers() & Qt::ControlModifier)) {
        m_selectedKeys.clear();
        for (const QString& k : m_order)
            m_selectedKeys.insert(k);
        if (!m_order.isEmpty()) {
            m_currentKey = m_order.last();
            m_anchorKey = m_order.first();
            m_selectedId = m_data.value(m_currentKey).id;
        }
        applySelectionVisuals();
        Q_EMIT selectionChanged(m_selectedId);
        e->accept();
        return;
    }
    if (e->key() == Qt::Key_Space && (e->modifiers() & Qt::ControlModifier)) {
        if (!m_currentKey.isEmpty() && m_data.contains(m_currentKey)) {
            selectWithModifiers(m_data.value(m_currentKey).id, Qt::ControlModifier);
            Q_EMIT selectionChanged(m_selectedId);
        }
        e->accept();
        return;
    }
    QScrollArea::keyPressEvent(e);
}

void ControlCardList::focusInEvent(QFocusEvent* e)
{
    QScrollArea::focusInEvent(e);
    if (m_currentKey.isEmpty() && !m_order.isEmpty()) {
        m_currentKey = m_order.first();
        if (m_selectedKeys.isEmpty()) {
            selectWithModifiers(m_data.value(m_currentKey).id, Qt::NoModifier);
            Q_EMIT selectionChanged(m_selectedId);
        }
    }
}

int ControlCardList::columnCountForWidth(int /*w*/) const
{
    return 1;
}

void ControlCardList::rebuildGrid()
{
    while (m_grid->count() > 0)
        m_grid->takeAt(0);
    m_cols = 1;
    int i = 0;
    for (const QString& k : m_order) {
        ControlCard* card = m_cards.value(k);
        if (!card)
            continue;
        m_grid->addWidget(card, i, 0);
        ++i;
    }
    m_grid->setColumnStretch(0, 1);
}

void ControlCardList::resizeEvent(QResizeEvent* e)
{
    QScrollArea::resizeEvent(e);
    for (auto* c : m_cards)
        c->reflowText();
}
