#include <Utils/CustomElements/ListFeed.h>
#include <Utils/FontManager.h>
#include <Client/Settings.h>
#include <MainAdaptix.h>

#include <oclero/qlementine/style/Theme.hpp>
#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/utils/ImageUtils.hpp>
#include <oclero/qlementine/widgets/Switch.hpp>

#include <QPainter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <functional>

static QColor ensureContrast(const QColor& c, bool dark, qreal minLightness = 0.35, qreal maxLightness = 0.75) {
    float h, s, l, a;
    c.getHslF(&h, &s, &l, &a);
    if (dark && l < minLightness)
        l = static_cast<float>(minLightness);
    if (!dark && l > maxLightness)
        l = static_cast<float>(maxLightness);
    return QColor::fromHslF(h, s, l, a);
}

static QColor mutedFromBase(const QColor& baseText, bool dark) {
    float h, s, l, a;
    baseText.getHslF(&h, &s, &l, &a);
    if (dark)
        l = qBound(0.45f, l * 0.65f, 0.7f);
    else
        l = qBound(0.35f, l * 1.4f, 0.55f);
    s *= 0.35f;
    return QColor::fromHslF(h, s, l, a);
}

static qreal relativeLuminance(const QColor& c)
{
    auto linearize = [](qreal v) -> qreal {
        return (v <= 0.04045) ? v / 12.92 : qPow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linearize(c.redF())
         + 0.7152 * linearize(c.greenF())
         + 0.0722 * linearize(c.blueF());
}

static qreal contrastRatio(const QColor& c1, const QColor& c2)
{
    qreal l1 = relativeLuminance(c1);
    qreal l2 = relativeLuminance(c2);
    qreal lighter = qMax(l1, l2);
    qreal darker  = qMin(l1, l2);
    return (lighter + 0.05) / (darker + 0.05);
}

static QColor ensureDeadBgContrast(const QColor& bg, bool dark)
{
    float h, s, l, a;
    bg.getHslF(&h, &s, &l, &a);
    if (h < 0.0f) h = 0.0f;
    l = dark ? qMin(l + static_cast<float>(GlobalClient->settings->data.DeadLightnessShift), 1.0f) : qMax(l - static_cast<float>(GlobalClient->settings->data.DeadLightnessShift), 0.0f);
    s = qBound(0.0f, s, 1.0f);
    l = qBound(0.0f, l, 1.0f);
    return QColor::fromHslF(h, s, l, a);
}

FeedColors FeedColors::fromTheme()
{
    FeedColors fc;
    auto* style = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style());
    QPalette pal;
    bool dark = false;
    QColor baseText;

    if (style) {
        const auto& theme = style->theme();
        dark = theme.backgroundColorMain1.lightnessF() < 0.5;
        baseText = pal.color(QPalette::Text);

        fc.success  = ensureContrast(theme.statusColorSuccess, dark);
        fc.error    = ensureContrast(theme.statusColorError, dark);
        fc.canceled = ensureContrast(theme.statusColorInfo, dark);
        fc.running  = ensureContrast(theme.primaryColor, dark);
        fc.hosted   = ensureContrast(theme.statusColorWarning, dark);

        fc.rowBg       = theme.backgroundColorMain1;
        fc.rowAltBg    = theme.backgroundColorMain2;
        fc.rowHoverBg  = theme.neutralColorHovered;
        fc.rowDeadBg   = ensureDeadBgContrast(theme.backgroundColorMain1, dark);
        fc.groupBg     = dark ? theme.backgroundColorMain1.lighter(122)
                              : theme.backgroundColorMain1.darker(112);
        fc.separatorLine = theme.borderColor;
        fc.tagBorder   = theme.borderColor;
    } else {
        dark = pal.color(QPalette::Base).lightnessF() < 0.5;
        baseText = pal.color(QPalette::Text);

        fc.success  = dark ? QColor("#3fb950") : QColor("#1a7f37");
        fc.error    = dark ? QColor("#f85149") : QColor("#cf222e");
        fc.canceled = dark ? QColor("#79c0ff") : QColor("#0550ae");
        fc.running  = dark ? QColor("#58a6ff") : QColor("#0969da");
        fc.hosted   = dark ? QColor("#d29922") : QColor("#9a6700");

        fc.rowBg       = pal.color(QPalette::Base);
        fc.rowAltBg    = dark ? fc.rowBg.lighter(105) : fc.rowBg.darker(103);
        fc.rowHoverBg  = dark ? fc.rowBg.lighter(115) : fc.rowBg.darker(106);
        fc.rowDeadBg   = ensureDeadBgContrast(fc.rowBg, dark);
        fc.groupBg     = dark ? QColor("#1c2330") : QColor("#e8ecf0");
        fc.separatorLine = dark ? QColor("#30363d") : QColor("#d0d7de");
        fc.tagBorder   = dark ? QColor("#3fb950") : QColor("#1a7f37");
    }

    fc.textPrimary   = baseText;
    fc.textSecondary = mutedFromBase(baseText, dark);
    {
        float h, s, l, a;
        baseText.getHslF(&h, &s, &l, &a);
        if (dark) l = qBound(0.55f, l * 0.65f, 0.70f);
        else      l = qBound(0.35f, l * 1.5f, 0.50f);
        s *= 0.15f;
        fc.textDead = QColor::fromHslF(h, s, l, a);
    }
    fc.tagText       = baseText;
    fc.dark          = dark;

    QColor selBase = style ? style->theme().primaryColor : pal.color(QPalette::Highlight);
    fc.accent = selBase.isValid() ? selBase : (dark ? QColor("#3fb950") : QColor("#1a7f37"));
    fc.rowSelectedBg = fc.accent;

    fc.selectedText = fc.textPrimary;
    fc.selectedMuted = fc.textSecondary;

    return fc;
}

static QColor feedLerp(const QColor& a, const QColor& b, qreal t)
{
    if (!a.isValid())
        return b;
    if (!b.isValid())
        return a;
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgb(
        int(a.red()   * (1.0 - t) + b.red()   * t),
        int(a.green() * (1.0 - t) + b.green() * t),
        int(a.blue()  * (1.0 - t) + b.blue()  * t),
        255);
}

QColor FeedColors::selectedWash(const QColor& base, qreal t) const
{
    QColor hi = accent.isValid() ? accent : rowSelectedBg;
    if (!hi.isValid())
        return base;
    if (!base.isValid())
        return hi;

    QColor elevated = rowAltBg.isValid() ? rowAltBg : base;
    if (dark) {
        elevated = elevated.lighter(112);
        QColor fromBase = base.lighter(120);
        if (fromBase.lightness() > elevated.lightness())
            elevated = fromBase;
    } else {
        elevated = elevated.darker(103);
    }
    elevated = feedLerp(elevated, hi, dark ? 0.06 : 0.04);

    if (t < 0.0)
        t = dark ? 0.28 : 0.18;
    return feedLerp(elevated, hi, t);
}

void paintFeedTableCellBackground(QPainter* p, const QRect& rect, bool selected, bool hovered, bool oddRow, bool firstColumn, const QColor& customBg)
{
    if (!p || !rect.isValid())
        return;

    const FeedColors fc = FeedColors::fromTheme();
    QColor surface;
    if (customBg.isValid())
        surface = customBg;
    else
        surface = oddRow ? (fc.rowAltBg.isValid() ? fc.rowAltBg : fc.rowBg) : fc.rowBg;
    if (!surface.isValid())
        surface = QColor(40, 40, 40);

    if (selected) {
        surface = fc.selectedWash(surface);
    } else if (hovered) {
        QColor hi = fc.accent.isValid() ? fc.accent : fc.rowSelectedBg;
        QColor hov = fc.dark ? surface.lighter(110) : surface.darker(102);
        surface = feedLerp(hov, hi, fc.dark ? 0.08 : 0.05);
    }

    p->fillRect(rect, surface);

    if (!selected || !firstColumn)
        return;

    QColor acc = fc.accent.isValid() ? fc.accent : fc.rowSelectedBg;
    if (!acc.isValid())
        return;
    acc.setAlpha(fc.dark ? 175 : 160);
    const int insetY = qMax(3, rect.height() / 7);
    const QRectF stripR(rect.left() + 2.0, rect.top() + insetY, 2.5, qMax(4.0, qreal(rect.height() - 2 * insetY)));
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);
    p->setBrush(acc);
    p->drawRoundedRect(stripR, 1.2, 1.2);
    p->restore();
}




int FeedListModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return m_rows.size();
}

QVariant FeedListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return {};
    if (index.row() < 0 || index.row() >= m_rows.size()) return {};
    const FeedRow& row = m_rows.at(index.row());
    if (role == Qt::UserRole)
        return row.entityId;
    if (role == Qt::DisplayRole)
        return row.size() > 0 ? row.blockData.first() : QVariant();
    if (role == GroupKeyRole && m_groupKeyBlock >= 0 && m_groupKeyBlock < row.size()) {
        return row.blockData[m_groupKeyBlock].toMap()[m_groupKeyField].toString();
    }
    return {};
}

void FeedListModel::setGroupKeySource(int blockIndex, const QString& fieldKey)
{
    m_groupKeyBlock = blockIndex;
    m_groupKeyField = fieldKey;
}

void FeedListModel::addRow(const FeedRow& row) {
    int pos = m_rows.size();
    beginInsertRows({}, pos, pos);
    m_rows.append(row);
    endInsertRows();
    Q_EMIT rowsContentChanged();
}

void FeedListModel::insertRow(int pos, const FeedRow& row) {
    if (pos < 0)
        pos = 0;
    if (pos > m_rows.size())
        pos = m_rows.size();
    beginInsertRows({}, pos, pos);
    m_rows.insert(pos, row);
    endInsertRows();
    Q_EMIT rowsContentChanged();
}

void FeedListModel::addRows(const QVector<FeedRow>& rows) {
    if (rows.isEmpty())
        return;
    int start = m_rows.size();
    int end = start + rows.size() - 1;
    beginInsertRows({}, start, end);
    m_rows.append(rows);
    endInsertRows();
    Q_EMIT rowsContentChanged();
}

void FeedListModel::updateRow(int row, const FeedRow& data) {
    if (row < 0 || row >= m_rows.size())
        return;
    m_rows[row] = data;
    Q_EMIT dataChanged(index(row, 0), index(row, 0), { Qt::DisplayRole, Qt::UserRole, Qt::DecorationRole, Qt::BackgroundRole, Qt::ForegroundRole });
    Q_EMIT rowsContentChanged();
}

void FeedListModel::removeRow(int row) {
    if (row < 0 || row >= m_rows.size())
        return;
    beginRemoveRows({}, row, row);
    m_rows.removeAt(row);
    endRemoveRows();
    Q_EMIT rowsContentChanged();
}

void FeedListModel::clear() {
    beginResetModel();
    m_rows.clear();
    m_idToRow.clear();
    endResetModel();
    Q_EMIT rowsContentChanged();
}

const FeedRow& FeedListModel::rowAt(int i) const {
    return m_rows.at(i);
}

int FeedListModel::size() const {
    return m_rows.size();
}

void FeedListModel::sortByBlock(int blockIndex, Qt::SortOrder order) {
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    std::sort(m_rows.begin(), m_rows.end(), [blockIndex, order](const FeedRow& a, const FeedRow& b) {
        if (blockIndex >= a.size() || blockIndex >= b.size())
            return false;
        bool less = a.blockData[blockIndex].toString() < b.blockData[blockIndex].toString();
        return order == Qt::AscendingOrder ? less : !less;
    });
    endResetModel();
    Q_EMIT rowsContentChanged();
}

void FeedListModel::sortByField(int blockIndex, const QString& key, Qt::SortOrder order) {
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    std::sort(m_rows.begin(), m_rows.end(), [blockIndex, key, order](const FeedRow& a, const FeedRow& b) {
        if (blockIndex >= a.size() || blockIndex >= b.size())
            return false;
        QString va = a.blockData[blockIndex].toMap()[key].toString();
        QString vb = b.blockData[blockIndex].toMap()[key].toString();
        bool less = va < vb;
        return order == Qt::AscendingOrder ? less : !less;
    });
    endResetModel();
    Q_EMIT rowsContentChanged();
}

void FeedListModel::sortByFieldNumeric(int blockIndex, const QString& key, Qt::SortOrder order) {
    if (m_rows.isEmpty())
        return;
    beginResetModel();
    std::sort(m_rows.begin(), m_rows.end(), [blockIndex, key, order](const FeedRow& a, const FeedRow& b) {
        if (blockIndex >= a.size() || blockIndex >= b.size())
            return false;
        qint64 va = a.blockData[blockIndex].toMap()[key].toLongLong();
        qint64 vb = b.blockData[blockIndex].toMap()[key].toLongLong();
        bool less = va < vb;
        return order == Qt::AscendingOrder ? less : !less;
    });
    endResetModel();
    Q_EMIT rowsContentChanged();
}



int IconBlock::measureWidth(const QVariant& data, const QFont&, const QFont&, const QFont&) const {
    QIcon icon = data.value<QIcon>();
    return icon.isNull() ? 0 : 26;
}

void IconBlock::paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont&, const QFont&, const QColor&, const QColor&, bool selected, bool, const FeedPaintContext& ctx) const {
    QIcon icon = data.value<QIcon>();
    if (icon.isNull()) return;

    int availableH = rect.height();
    int availableW = rect.width();

    int target = qMin(availableH - 4, ctx.iconSize);
    target = qMin(target, availableW - 4);

    int top = rect.top() + (availableH - target) / 2;

    int leftPad = 2;
    if (leftPad + target > availableW) {
        leftPad = 1;
    }
    int left = rect.left() + leftPad;

    QRect iconRect(left, top, target, target);
    Q_UNUSED(selected);
    icon.paint(p, iconRect, Qt::AlignCenter, QIcon::Normal);
}

int IdBadgeBlock::measureWidth(const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont) const {
    auto map = data.toMap();
    QFont idF = monoFont;
    QFontMetrics fmId(idF);
    int idW = fmId.horizontalAdvance(map["id"].toString()) + 4;
    QString badgeStr = map["badge"].toString();
    int line1W = idW;
    if (!badgeStr.isEmpty()) {
        QFontMetrics fmTiny(tinyFont);
        int badgeW = fmTiny.horizontalAdvance(badgeStr) + 10;
        line1W = idW + 6 + badgeW;
    }
    QString dateStr = map["date"].toString();
    int createdW = dateStr.isEmpty() ? 0 : QFontMetrics(smallFont).horizontalAdvance(dateStr) + 4;
    return qMax(line1W, createdW) + 8;
}

void IdBadgeBlock::paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont, const QColor& colText, const QColor& colMuted, bool, bool, const FeedPaintContext& ctx) const {
    auto map = data.toMap();
    bool compact = ctx.compact;
    int lh1 = compact ? ctx.lineH1Compact : ctx.lineH1;
    int lh2 = compact ? 0 : ctx.lineH2;
    int gap = compact ? 0 : ctx.lineGap;
    int y1 = rect.top();
    int y2 = y1 + lh1 + gap;

    QString badgeStr = map["badge"].toString();
    int badgeW = 0;
    int badgeX = rect.right() - 4;

    QFont badgeF = tinyFont;

    if (!badgeStr.isEmpty()) {
        QFontMetrics fmB(badgeF);
        badgeW = fmB.horizontalAdvance(badgeStr) + 10;
        badgeX = rect.right() - badgeW - 4;
    }

    QFont idF = monoFont;
    QFontMetrics fmId(idF);
    int maxTextW = badgeX - rect.left() - 6;
    if (maxTextW < 20) maxTextW = 20;
    QString elidedId = fmId.elidedText(map["id"].toString(), Qt::ElideRight, maxTextW);

    p->setFont(idF);
    p->setPen(colText);
    p->drawText(rect.left(), y1, maxTextW, lh1, Qt::AlignLeft | Qt::AlignVCenter, elidedId);

    if (!badgeStr.isEmpty()) {
        p->setFont(badgeF);
        QRect badgeRect(badgeX, y1 + 2, badgeW, lh1 - 4);
        p->setPen(colMuted);
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(badgeRect, 3, 3);
        p->drawText(badgeRect, Qt::AlignCenter, badgeStr);
    }

    QString dateStr = map["date"].toString();
    if (!dateStr.isEmpty()) {
        p->setFont(smallFont);
        p->setPen(colMuted);
        p->drawText(rect.left(), y2, rect.width(), lh2, Qt::AlignLeft | Qt::AlignVCenter, dateStr);
    }
}

int IdBadgeBlock::measureCompactDivision(int division, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont) const {
    auto map = data.toMap();
    if (division == 0) {
        QFont idF = monoFont;
        QFontMetrics fmId(idF);
        int idW = fmId.horizontalAdvance(map["id"].toString()) + 6;
        QString badgeStr = map["badge"].toString();
        if (!badgeStr.isEmpty()) {
            QFontMetrics fmB(tinyFont);
            idW += 6 + fmB.horizontalAdvance(badgeStr) + 10;
        }
        return idW;
    } else if (division == 1) {
        QString dateStr = map["date"].toString();
        if (dateStr.isEmpty()) return 0;
        return QFontMetrics(smallFont).horizontalAdvance(dateStr) + 6;
    }
    return 0;
}

void IdBadgeBlock::paintCompactDivision(QPainter* p, const QRect& subRect, int division, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx) const {
    auto map = data.toMap();
    if (division == 0) {
        QString badgeStr = map["badge"].toString();
        int badgeW = 0;
        int badgeX = subRect.right() - 2;
        QFont badgeF = tinyFont;

        if (!badgeStr.isEmpty()) {
            QFontMetrics fmB(badgeF);
            badgeW = fmB.horizontalAdvance(badgeStr) + 10;
            badgeX = subRect.right() - badgeW - 2;
        }

        QFont idF = monoFont;
        QFontMetrics fmId(idF);
        int maxTextW = badgeX - subRect.left() - 6;
        if (maxTextW < 20) maxTextW = 20;
        if (badgeW == 0) {
            maxTextW = subRect.width();
        }
        QString elidedId = fmId.elidedText(map["id"].toString(), Qt::ElideRight, maxTextW);

        p->setFont(idF);
        p->setPen(colText);
        p->drawText(subRect.left(), subRect.top(), maxTextW, subRect.height(), Qt::AlignLeft | Qt::AlignVCenter, elidedId);

        if (!badgeStr.isEmpty() && badgeW > 0) {
            QRect badgeRect(badgeX, subRect.top() + 1, badgeW, subRect.height() - 2);
            p->setFont(badgeF);
            p->setPen(colMuted);
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(badgeRect, 2, 2);
            p->drawText(badgeRect, Qt::AlignCenter, badgeStr);
        }
    } else if (division == 1) {
        QString dateStr = map["date"].toString();
        if (!dateStr.isEmpty()) {
            p->setFont(smallFont);
            p->setPen(colMuted);
            p->drawText(subRect.left(), subRect.top(), subRect.width(), subRect.height(), Qt::AlignLeft | Qt::AlignVCenter, dateStr);
        }
    }
}

QFont MainBlock::mainTextFont(const QFont& smallFont, const QFont& /*primaryFont*/) const
{
    QFont f = smallFont;
    const qreal base = (smallFont.pointSizeF() > 0.0 ? smallFont.pointSizeF() : qreal(smallFont.pointSize())) + 1.0;
    f.setPointSizeF(qMax(6.0, base + m_mainTextPointOffset));
    f.setBold(true);
    return f;
}

int MainBlock::measureWidth(const QVariant& data, const QFont&, const QFont& smallFont, const QFont&) const {
    auto map = data.toMap();
    QFont bold = mainTextFont(smallFont);
    QFontMetrics fmBold(bold);
    QFontMetrics fmSmall(smallFont);

    QString mainText = map["main"].toString();
    QString subMain = map["submain"].toString();
    QString second = map["second"].toString();

    int line1W = 0;
    if (!mainText.isEmpty()) line1W += fmBold.horizontalAdvance(mainText) + 4;
    if (!subMain.isEmpty()) line1W += 24 + fmSmall.horizontalAdvance(subMain) + 4;

    int line2W = second.isEmpty() ? 0 : fmSmall.horizontalAdvance(second) + 4;

    return qMax(line1W, line2W);
}

void MainBlock::paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool, bool, const FeedPaintContext& ctx) const {
    auto map = data.toMap();
    QString mainText = map["main"].toString();
    QString subMain = map["submain"].toString();
    QString second = map["second"].toString();

    bool compact = ctx.compact;
    int lh1 = compact ? ctx.lineH1Compact : ctx.lineH1;
    int lh2 = compact ? 0 : ctx.lineH2;
    int gap = compact ? 0 : ctx.lineGap;
    int y1 = rect.top();
    int y2 = y1 + lh1 + gap;
    int maxW = rect.width() - 4;

    QFont bold;
    if (!compact && ctx.primaryFont.pointSizeF() > 0.0) {
        bold = ctx.primaryFont;
        if (!qFuzzyIsNull(m_mainTextPointOffset)) {
            const qreal ps = bold.pointSizeF() > 0.0 ? bold.pointSizeF() : qreal(bold.pointSize());
            bold.setPointSizeF(qMax(6.0, ps + m_mainTextPointOffset));
        }
    } else {
        bold = mainTextFont(smallFont);
    }
    QFontMetrics fmBold(bold);
    QFontMetrics fmSmall(smallFont);
    int x = rect.left();

    if (!mainText.isEmpty()) {
        p->setFont(bold);
        p->setPen(colText);
        int w = fmBold.horizontalAdvance(mainText) + 4;
        int drawW = qMin(w, maxW);
        p->drawText(x, y1, drawW, lh1, Qt::AlignLeft | Qt::AlignVCenter, fmBold.elidedText(mainText, Qt::ElideRight, drawW));
        x += w;
    }

    if (!subMain.isEmpty()) {
        if (x + 24 <= rect.right()) {
            p->setPen(colMuted);
            p->drawText(x, y1, 24, lh1, Qt::AlignCenter, "|");
        }
        x += 24;

        p->setFont(smallFont);
        p->setPen(colMuted);
        int remainW = rect.right() - x - 4;
        if (remainW > 0)
            p->drawText(x, y1, remainW, lh1, Qt::AlignLeft | Qt::AlignVCenter, fmSmall.elidedText(subMain, Qt::ElideRight, remainW));
    }

    if (!second.isEmpty() && !compact) {
        p->setFont(smallFont);
        p->setPen(colMuted);
        p->drawText(rect.left(), y2, maxW, lh2, Qt::AlignLeft | Qt::AlignVCenter, fmSmall.elidedText(second, Qt::ElideRight, maxW));
    }
}

int MainBlock::measureCompactDivision(int division, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&) const {
    auto map = data.toMap();
    QFont bold = mainTextFont(smallFont);
    QFontMetrics fmBold(bold);
    QFontMetrics fmSmall(smallFont);

    if (division == 0) {
        QString mainText = map["main"].toString();
        return mainText.isEmpty() ? 0 : fmBold.horizontalAdvance(mainText) + 6;
    } else if (division == 1) {
        QString subMain = map["submain"].toString();
        int w = 0;
        if (!subMain.isEmpty())
            w += 20 + fmSmall.horizontalAdvance(subMain) + 6;  // space for | + sub
        return w;
    } else {
        QString second = map["second"].toString();
        return second.isEmpty() ? 0 : fmSmall.horizontalAdvance(second) + 6;
    }
}

void MainBlock::paintCompactDivision(QPainter* p, const QRect& subRect, int division, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool, bool, const FeedPaintContext& ctx) const {
    Q_UNUSED(ctx);
    auto map = data.toMap();
    QFont bold = mainTextFont(smallFont);
    QFontMetrics fmBold(bold);
    QFontMetrics fmSmall(smallFont);

    if (division == 0) {
        QString mainText = map["main"].toString();
        if (!mainText.isEmpty()) {
            p->setFont(bold);
            p->setPen(colText);
            int w = fmBold.horizontalAdvance(mainText) + 4;
            int mw = subRect.width() - 4;
            if (mw < 4) mw = 4;
            p->drawText(subRect.left(), subRect.top(), qMin(w, mw), subRect.height(), Qt::AlignLeft | Qt::AlignVCenter, fmBold.elidedText(mainText, Qt::ElideRight, mw));
        }
    } else if (division == 1) {
        QString subMain = map["submain"].toString();
        int x = subRect.left();
        if (!subMain.isEmpty() && x + 16 < subRect.right()) {
            p->setPen(colMuted);
            p->drawText(x, subRect.top(), 16, subRect.height(), Qt::AlignCenter, "|");
            x += 16;
            p->setFont(smallFont);
            int remain = subRect.right() - x - 2;
            if (remain < 4) remain = 4;
            p->drawText(x, subRect.top(), remain, subRect.height(), Qt::AlignLeft | Qt::AlignVCenter, fmSmall.elidedText(subMain, Qt::ElideRight, remain));
        }
    } else {
        QString second = map["second"].toString();
        if (!second.isEmpty()) {
            p->setFont(smallFont);
            p->setPen(colMuted);
            int mw = subRect.width() - 4;
            if (mw < 4) mw = 4;
            p->drawText(subRect.left(), subRect.top(), mw, subRect.height(), Qt::AlignLeft | Qt::AlignVCenter, fmSmall.elidedText(second, Qt::ElideRight, mw));
        }
    }
}

int TextBlock::measureWidth(const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont&) const {
    auto map = data.toMap();
    QFont idF = monoFont;
    QFontMetrics fmId(idF);
    QFontMetrics fmSmall(smallFont);
    QString mainText = map["main"].toString();
    QString second = map["second"].toString();
    int w1 = mainText.isEmpty() ? 0 : fmId.horizontalAdvance(mainText) + 6;
    int w2 = second.isEmpty() ? 0 : fmSmall.horizontalAdvance(second) + 6;
    return qMax(w1, w2);
}

void TextBlock::paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool, bool, const FeedPaintContext& ctx) const {
    auto map = data.toMap();
    QString mainText = map["main"].toString();
    QString second = map["second"].toString();

    bool compact = ctx.compact;
    QFont idF = monoFont;
    QFontMetrics fmId(idF);
    QFontMetrics fmSmall(smallFont);
    int lh1 = compact ? ctx.lineH1Compact : ctx.lineH1;
    int lh2 = compact ? 0 : ctx.lineH2;
    int gap = compact ? 0 : qMax(2, ctx.lineGap - 1);
    int y1 = rect.top();
    int y2 = y1 + lh1 + gap;
    int maxW = rect.width() - 2;

    if (!mainText.isEmpty()) {
        p->setFont(idF);
        p->setPen(colText);
        p->drawText(rect.left(), y1, maxW, lh1, Qt::AlignLeft | Qt::AlignVCenter, fmId.elidedText(mainText, Qt::ElideRight, maxW));
    }

    if (!second.isEmpty() && !compact) {
        p->setFont(smallFont);
        p->setPen(colMuted);
        p->drawText(rect.left(), y2, maxW, lh2, Qt::AlignLeft | Qt::AlignVCenter, fmSmall.elidedText(second, Qt::ElideRight, maxW));
    }
}

int TextBlock::measureCompactDivision(int division, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont&) const {
    auto map = data.toMap();
    if (division == 0) {
        QFont idF = monoFont;
        QFontMetrics fmId(idF);
        return map["main"].toString().isEmpty() ? 0 : fmId.horizontalAdvance(map["main"].toString()) + 6;
    } else {
        QFontMetrics fmSmall(smallFont);
        return map["second"].toString().isEmpty() ? 0 : fmSmall.horizontalAdvance(map["second"].toString()) + 6;
    }
}

void TextBlock::paintCompactDivision(QPainter* p, const QRect& subRect, int division, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool, bool, const FeedPaintContext& ctx) const {
    auto map = data.toMap();
    if (division == 0) {
        QString mainText = map["main"].toString();
        if (!mainText.isEmpty()) {
            QFont idF = monoFont;
            QFontMetrics fmId(idF);
            p->setFont(idF);
            p->setPen(colText);
            int mw = subRect.width() - 2;
            if (mw < 8) mw = 8;
            p->drawText(subRect.left(), subRect.top(), mw, subRect.height(), Qt::AlignLeft | Qt::AlignVCenter, fmId.elidedText(mainText, Qt::ElideRight, mw));
        }
    } else {
        QString second = map["second"].toString();
        if (!second.isEmpty()) {
            QFontMetrics fmSmall(smallFont);
            p->setFont(smallFont);
            p->setPen(colMuted);
            int mw = subRect.width() - 2;
            if (mw < 8) mw = 8;
            p->drawText(subRect.left(), subRect.top(), mw, subRect.height(), Qt::AlignLeft | Qt::AlignVCenter, fmSmall.elidedText(second, Qt::ElideRight, mw));
        }
    }
}


int ProgressBlock::measureWidth(const QVariant& data, const QFont&, const QFont& smallFont, const QFont&) const {
    auto map = data.toMap();
    QFontMetrics fmSmall(smallFont);
    QString mainText = map["main"].toString();
    int w = fmSmall.horizontalAdvance(mainText) + 4;
    return qMax(w, 150);
}

void ProgressBlock::paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&, const QColor&, const QColor& colMuted, bool selected, bool, const FeedPaintContext& ctx) const {
    auto map = data.toMap();
    QString progressText = map["main"].toString();
    QString infoText = map["second"].toString();
    double pct = map["percent"].toDouble();

    bool compact = ctx.compact;
    int lh1 = compact ? ctx.lineH1Compact : ctx.lineH1;
    int lh2 = compact ? 0 : ctx.lineH2;
    int gap = compact ? 0 : ctx.lineGap;
    int y1 = rect.top();
    int y2 = y1 + lh1 + gap;
    int maxW = rect.width() - 4;

    int barH = qMax(10, qMin(lh1 - 2, qRound(lh1 * 0.85)));
    int barY = y1 + (lh1 - barH) / 2;
    QRect barRect(rect.left(), barY, maxW, barH);

    QColor trackColor(255, 255, 255, 12);
    p->fillRect(barRect, trackColor);

    QColor fillColor = FeedColors::fromTheme().accent;
    if (!fillColor.isValid())
        fillColor = FeedColors::fromTheme().rowSelectedBg;
    fillColor.setAlpha(selected ? 160 : 200);
    int fillW = (int)(maxW * qBound(0.0, pct / 100.0, 1.0));
    if (fillW > 0) {
        QRect fillRect(barRect.left(), barRect.top(), fillW, barH);
        p->fillRect(fillRect, fillColor);
    }

    if (!progressText.isEmpty()) {
        QFont pf = ctx.microFont.pointSize() > 0 ? ctx.microFont : smallFont;
        if (ctx.microFont.pointSize() <= 0)
            pf.setPointSize(qMax(6, smallFont.pointSize() - 2));
        QFontMetrics fmP(pf);
        p->setFont(pf);
        p->setPen(QColor(255, 255, 255, 220));
        p->drawText(barRect, Qt::AlignCenter, progressText);
    }

    if (!infoText.isEmpty() && !compact) {
        p->setFont(smallFont);
        p->setPen(colMuted);
        QFontMetrics fmSmall(smallFont);
        p->drawText(rect.left(), y2, maxW, lh2, Qt::AlignLeft | Qt::AlignVCenter, fmSmall.elidedText(infoText, Qt::ElideRight, maxW));
    }
}


int TagsBlock::measureWidth(const QVariant& data, const QFont&, const QFont& smallFont, const QFont&) const {
    QStringList tags = data.toStringList();
    if (tags.isEmpty())
        return 0;
    QFontMetrics fm(smallFont);
    int w = 0;
    for (const QString& t : tags) {
        QString trimmed = t.trimmed();
        if (!trimmed.isEmpty()) w += fm.horizontalAdvance(trimmed) + 12 + 6;
    }
    return w > 0 ? w + 4 : 0;
}

void TagsBlock::paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont& smallFont, const QFont& tinyFont, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx) const {
    QStringList tags = data.toStringList();
    if (tags.isEmpty())
        return;
    QFont tagFont = ctx.tagFont.pointSize() > 0 ? ctx.tagFont : smallFont;
    if (ctx.tagFont.pointSize() <= 0 && ctx.tagFontSize > 0)
        tagFont.setPointSize(ctx.tagFontSize);
    Q_UNUSED(tinyFont);
    p->setFont(tagFont);
    QFontMetrics fm(tagFont);
    int x = rect.left() + 4;
    int badgeH = ctx.tagBadgeHeight;
    int y;
    if (ctx.compact) {
        badgeH = qMin(badgeH, rect.height() - 4);
        y = rect.top() + 1;
    } else {
        y = rect.top() + (rect.height() - badgeH) / 2;
    }
    for (const QString& tag : tags) {
        QString trimmed = tag.trimmed();
        if (trimmed.isEmpty())
            continue;
        int tw = fm.horizontalAdvance(trimmed) + 10;
        QRect tagRect(x, y, tw, badgeH);
        const FeedColors fc = FeedColors::fromTheme();
        QColor tagCol = fc.accent.isValid() ? fc.accent : fc.rowSelectedBg;
        if (selected)
            tagCol = colText;
        p->setPen(tagCol);
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(tagRect, 3, 3);
        p->setPen(selected ? colText : colMuted);
        p->drawText(tagRect, Qt::AlignCenter, trimmed);
        x += tw + 5;
    }
}

int StatusBlock::measureWidth(const QVariant& data, const QFont&, const QFont& smallFont, const QFont&) const {
    auto map = data.toMap();
    QFont bold = smallFont; bold.setBold(true);
    QFont mainFont = smallFont;
    mainFont.setPointSize(smallFont.pointSize() + 1);
    mainFont.setBold(true);
    QFontMetrics fmMain(mainFont);
    QFontMetrics fmBold(bold);
    QFontMetrics fmSmall(smallFont);

    QString mainText = map["main"].toString();
    QString status = map["status"].toString();
    QString progress = map["progress"].toString();
    QString second = map["second"].toString();

    int line1W = 0;
    if (!mainText.isEmpty())
        line1W += fmMain.horizontalAdvance(mainText) + 4;
    if (!progress.isEmpty())
        line1W += 100;
    if (!status.isEmpty())
        line1W += fmBold.horizontalAdvance(status) + 8;
    int line2W = second.isEmpty() ? 0 : fmSmall.horizontalAdvance(second) + 4;
    return qMax(line1W, line2W) + 8;
}

void StatusBlock::paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx) const {
    auto map = data.toMap();
    QString mainText = map["main"].toString();
    QString status = map["status"].toString();
    QString statusType = map["statusType"].toString();
    QString progress = map["progress"].toString();
    QString second = map["second"].toString();

    bool compact = ctx.compact;
    int lh1 = compact ? ctx.lineH1Compact : ctx.lineH1;
    int lh2 = compact ? 0 : ctx.lineH2;
    int gap = compact ? 0 : ctx.lineGap;
    int y1 = rect.top();
    int y2 = y1 + lh1 + gap;
    int maxW = rect.width() - 4;

    QFont bold = smallFont; bold.setBold(true);
    QFont mainFont = ctx.primaryFont.pointSize() > 0 ? ctx.primaryFont : smallFont;
    if (ctx.primaryFont.pointSize() <= 0) {
        mainFont.setPointSize(smallFont.pointSize() + 1);
        mainFont.setBold(true);
    }
    QFontMetrics fmMain(mainFont);
    QFontMetrics fmBold(bold);
    QFontMetrics fmSmall(smallFont);

    QColor statusCol = colText;
    {
        const auto fc = FeedColors::fromTheme();
        if (statusType == "success") statusCol = fc.success;
        else if (statusType == "error") statusCol = fc.error;
        else if (statusType == "running") statusCol = fc.running;
        else if (statusType == "hosted") statusCol = fc.hosted;
        else if (statusType == "canceled") statusCol = fc.canceled;
    }
    Q_UNUSED(selected);

    if (!mainText.isEmpty()) {
        p->setFont(mainFont);
        p->setPen(colText);
        p->drawText(rect.left(), y1, maxW, lh1, Qt::AlignLeft | Qt::AlignVCenter, fmMain.elidedText(mainText, Qt::ElideRight, maxW));
    }
    if (!status.isEmpty() && progress.isEmpty()) {
        p->setFont(bold);
        p->setPen(statusCol);
        int w = fmBold.horizontalAdvance(status) + 4;
        if (mainText.isEmpty())
            p->drawText(rect.left(), y1, w, lh1, Qt::AlignLeft | Qt::AlignVCenter, status);
        else
            p->drawText(rect.right() - w, y1, w, lh1, Qt::AlignRight | Qt::AlignVCenter, status);
    }
    if (!progress.isEmpty()) {
        int barH = qMax(10, qMin(lh1 - 2, qRound(lh1 * 0.85)));
        int barY = y1 + (lh1 - barH) / 2;
        QColor fillColor = FeedColors::fromTheme().accent;
        if (!fillColor.isValid())
            fillColor = FeedColors::fromTheme().rowSelectedBg;
        fillColor.setAlpha(200);
        double pctVal = progress.left(progress.length() - 1).toDouble();
        QFont pf = ctx.microFont.pointSize() > 0 ? ctx.microFont : smallFont;
        if (ctx.microFont.pointSize() <= 0)
            pf.setPointSize(qMax(6, smallFont.pointSize() - 2));

        if (mainText.isEmpty()) {
            QRect barRect(rect.left(), barY, maxW, barH);
            p->fillRect(barRect, QColor(255, 255, 255, 12));
            int fillW = (int)(maxW * qBound(0.0, pctVal / 100.0, 1.0));
            if (fillW > 0) p->fillRect(QRect(rect.left(), barY, fillW, barH), fillColor);
            p->setFont(pf);
            p->setPen(QColor(255, 255, 255, 220));
            p->drawText(barRect, Qt::AlignCenter, progress);
        } else {
            int barW = qMax(60, maxW);
            QRect barRect(rect.right() - barW, barY, barW, barH);
            p->fillRect(barRect, QColor(255, 255, 255, 12));
            int fillW = (int)(barW * qBound(0.0, pctVal / 100.0, 1.0));
            if (fillW > 0) p->fillRect(QRect(rect.right() - barW, barY, fillW, barH), fillColor);
            p->setFont(pf);
            p->setPen(QColor(255, 255, 255, 220));
            p->drawText(barRect, Qt::AlignCenter, progress);
        }
    }

    if (!second.isEmpty() && !compact) {
        p->setFont(smallFont);
        p->setPen(colMuted);
        p->drawText(rect.left(), y2, maxW, lh2, Qt::AlignLeft | Qt::AlignVCenter, fmSmall.elidedText(second, Qt::ElideRight, maxW));
    }
}

int StatusBlock::measureCompactDivision(int division, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&) const {
    auto map = data.toMap();
    QFont bold = smallFont; bold.setBold(true);
    QFont mainFont = smallFont; mainFont.setPointSize(smallFont.pointSize() + 1); mainFont.setBold(true);
    QFontMetrics fmMain(mainFont);
    QFontMetrics fmBold(bold);
    QFontMetrics fmSmall(smallFont);

    if (division == 0) {
        QString mainText = map["main"].toString();
        QString status = map["status"].toString();
        QString progress = map["progress"].toString();
        int w = 0;
        if (!mainText.isEmpty())
            w += fmMain.horizontalAdvance(mainText) + 4;
        if (!progress.isEmpty())
            w += 60;
        if (!status.isEmpty())
            w += fmBold.horizontalAdvance(status) + 8;
        return w;
    } else {
        QString second = map["second"].toString();
        return second.isEmpty() ? 0 : fmSmall.horizontalAdvance(second) + 4;
    }
}

void StatusBlock::paintCompactDivision(QPainter* p, const QRect& subRect, int division, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx) const {
    auto map = data.toMap();
    bool compact = ctx.compact;

    QFont bold = smallFont; bold.setBold(true);
    QFont mainFont = smallFont; mainFont.setPointSize(smallFont.pointSize() + 1); mainFont.setBold(true);
    QFontMetrics fmMain(mainFont);
    QFontMetrics fmBold(bold);
    QFontMetrics fmSmall(smallFont);

    QColor statusCol = colText;
    {
        const auto fc = FeedColors::fromTheme();
        QString statusType = map["statusType"].toString();
        if (statusType == "success") statusCol = fc.success;
        else if (statusType == "error") statusCol = fc.error;
        else if (statusType == "running") statusCol = fc.running;
        else if (statusType == "hosted") statusCol = fc.hosted;
        else if (statusType == "canceled") statusCol = fc.canceled;
    }
    Q_UNUSED(selected);

    if (division == 0) {
        QString mainText = map["main"].toString();
        QString status = map["status"].toString();
        QString progress = map["progress"].toString();

        if (!mainText.isEmpty()) {
            p->setFont(mainFont);
            p->setPen(colText);
            int mw = subRect.width() - 4;
            if (mw < 8) mw = 8;
            p->drawText(subRect.left(), subRect.top(), mw, subRect.height(), Qt::AlignLeft | Qt::AlignVCenter, fmMain.elidedText(mainText, Qt::ElideRight, mw));
        }

        if (!status.isEmpty() && progress.isEmpty()) {
            p->setFont(bold);
            p->setPen(statusCol);
            int sw = subRect.width() - 4;
            if (sw < 8) sw = 8;
            if (mainText.isEmpty()) {
                p->drawText(subRect.left(), subRect.top(), sw, subRect.height(), Qt::AlignLeft | Qt::AlignVCenter, status);
            } else {
                p->drawText(subRect, Qt::AlignRight | Qt::AlignVCenter, status);
            }
        }

        if (!progress.isEmpty()) {
            int barH = 10;
            int barY = subRect.top() + (subRect.height() - barH) / 2;
            double pct = map["percent"].toDouble();
            if (pct <= 0.0 && !progress.isEmpty()) {
                pct = progress.left(progress.length() - 1).toDouble();
            }
            QColor track(255, 255, 255, 20);
            if (mainText.isEmpty()) {
                int bw = qMax(60, subRect.width() - 4);
                QRect bar(subRect.left(), barY, bw, barH);
                p->fillRect(bar, track);
                int fill = int(bw * qBound(0.0, pct / 100.0, 1.0));
                if (fill > 0) p->fillRect(QRect(bar.left(), barY, fill, barH), statusCol);
            } else {
                QRect bar(subRect.right() - 50, barY, 48, barH);
                p->fillRect(bar, track);
                int fill = int(48 * qBound(0.0, pct / 100.0, 1.0));
                if (fill > 0) p->fillRect(QRect(bar.left(), barY, fill, barH), statusCol);
            }
        }
    } else {
        QString second = map["second"].toString();
        if (!second.isEmpty()) {
            p->setFont(smallFont);
            p->setPen(colMuted);
            p->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter, fmSmall.elidedText(second, Qt::ElideRight, subRect.width()));
        }
    }
}


int AttachmentBlock::measureWidth(const QVariant& data, const QFont&, const QFont& smallFont, const QFont& tinyFont) const {
    auto map = data.toMap();
    QFontMetrics fmMain(smallFont);
    QFontMetrics fmSub(tinyFont);
    QFontMetrics fmBtn(tinyFont);
    int w1 = fmMain.horizontalAdvance(map["main"].toString()) + 8;
    int w2 = fmSub.horizontalAdvance(map["submain"].toString()) + 8;
    int textW = qMax(w1, w2);

    auto actions = map["actions"].toList();
    int btnsW = 0;
    for (int i = 0; i < actions.size(); ++i) {
        QString label = actions[i].toMap()["label"].toString();
        if (i > 0)
            btnsW += BTN_GAP;
        btnsW += fmBtn.horizontalAdvance(label) + BTN_PAD * 2 + 14;
    }
    return qMax(textW, btnsW) + 8;
}

int AttachmentBlock::measureCompactDivision(int division, const QVariant& data, const QFont&, const QFont& smallFont, const QFont& tinyFont) const {
    auto map = data.toMap();
    if (division == 0) {
        QFontMetrics fm(smallFont);
        return map["main"].toString().isEmpty() ? 0 : fm.horizontalAdvance(map["main"].toString()) + 8;
    } else {
        auto actions = map["actions"].toList();
        QFontMetrics fmBtn(tinyFont);
        int w = 0;
        for (int i = 0; i < actions.size(); ++i) {
            if (i > 0) w += BTN_GAP;
            QString label = actions[i].toMap()["label"].toString();
            w += fmBtn.horizontalAdvance(label) + BTN_PAD * 2 + 10;
        }
        return w;
    }
}

void AttachmentBlock::paintCompactDivision(QPainter* p, const QRect& subRect, int division, const QVariant& data, const QFont&, const QFont& smallFont, const QFont& tinyFont, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx) const {
    auto map = data.toMap();
    if (division == 0) {
        QString mainText = map["main"].toString();
        if (!mainText.isEmpty()) {
            QFontMetrics fm(smallFont);
            p->setFont(smallFont);
            p->setPen(colMuted);
            int mw = subRect.width() - 4;
            if (mw < 8) mw = 8;
            p->drawText(subRect.left(), subRect.top(), mw, subRect.height(), Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(mainText, Qt::ElideRight, mw));
        }
    } else {
        auto actions = map["actions"].toList();
        QFontMetrics fmBtn(tinyFont);
        int x = subRect.left();
        int btnH = qMin(14, subRect.height() - 2);
        int y = subRect.top() + (subRect.height() - btnH) / 2;
        for (int i = 0; i < actions.size(); ++i) {
            auto aMap = actions[i].toMap();
            QString icon = aMap["icon"].toString();
            QString label = aMap["label"].toString();
            int btnW = fmBtn.horizontalAdvance(icon + " " + label) + BTN_PAD * 2 + 8;
            QRect btnRect(x, y, btnW, btnH);
            p->setPen(QPen(colMuted, 0.5));
            p->setBrush(QColor(255, 255, 255, 8));
            p->drawRoundedRect(btnRect, 2, 2);
            p->setFont(tinyFont);
            p->setPen(colMuted);
            p->drawText(btnRect.adjusted(BTN_PAD, 0, -BTN_PAD, 0), Qt::AlignCenter, icon + " " + label);
            x += btnW + BTN_GAP;
        }
    }
}

void AttachmentBlock::paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont& smallFont, const QFont& tinyFont, const QColor& colText, const QColor& colMuted, bool, bool, const FeedPaintContext& ctx) const {
    auto map = data.toMap();
    bool compact = ctx.compact;
    int lh1 = compact ? ctx.lineH1Compact : qMax(14, ctx.lineH1 - 2);
    int lh2 = compact ? 0 : qMax(12, ctx.lineH2 - 2);
    int gap = compact ? 0 : qMax(2, ctx.lineGap - 1);
    int y1 = rect.top();
    int y2 = y1 + lh1 + gap;
    int y3 = y2 + lh2 + (compact ? 2 : 4);
    int maxW = rect.width() - 4;

    QString mainText = map["main"].toString();
    if (!mainText.isEmpty()) {
        QFontMetrics fm(smallFont);
        p->setFont(smallFont);
        p->setPen(colMuted);
        p->drawText(rect.left(), y1, maxW, lh1, Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(mainText, Qt::ElideRight, maxW));
    }

    QString subText = map["submain"].toString();
    if (!subText.isEmpty() && !compact) {
        QFontMetrics fm(tinyFont);
        p->setFont(tinyFont);
        p->setPen(colMuted);
        p->drawText(rect.left(), y2, maxW, lh2, Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(subText, Qt::ElideRight, maxW));
    }

    auto actions = map["actions"].toList();
    QFontMetrics fmBtn(tinyFont);
    int x = rect.left();
    for (int i = 0; i < actions.size(); ++i) {
        auto aMap = actions[i].toMap();
        QString icon = aMap["icon"].toString();
        QString label = aMap["label"].toString();
        int btnW = fmBtn.horizontalAdvance(icon + " " + label) + BTN_PAD * 2 + 14;
        QRect btnRect(x, y3, btnW, BTN_H);

        p->setPen(QPen(colMuted, 0.5));
        p->setBrush(QColor(255, 255, 255, 8));
        p->drawRoundedRect(btnRect, 3, 3);

        p->setFont(tinyFont);
        p->setPen(colMuted);
        p->drawText(btnRect.adjusted(BTN_PAD, 0, -BTN_PAD, 0), Qt::AlignCenter, icon + " " + label);

        x += btnW + BTN_GAP;
    }
}

int AttachmentBlock::hitTest(const QPoint& localPos, const QRect& blockRect, const QVariant& data) const {
    auto map = data.toMap();
    const auto& ty = FontManager::instance().typography();
    int lh1 = ty.lineH1Compact;
    int y3 = blockRect.top() + lh1 + 2;
    if (localPos.y() < y3 || localPos.y() > y3 + BTN_H)
        return -1;

    QFontMetrics fmBtn(ty.caption);
    auto actions = map["actions"].toList();
    int x = blockRect.left();
    for (int i = 0; i < actions.size(); ++i) {
        auto aMap = actions[i].toMap();
        QString icon = aMap["icon"].toString();
        QString label = aMap["label"].toString();
        int btnW = fmBtn.horizontalAdvance(icon + " " + label) + BTN_PAD * 2 + 14;
        if (localPos.x() >= x && localPos.x() <= x + btnW)
            return i;
        x += btnW + BTN_GAP;
    }
    return -1;
}


void GroupHeaderBlock::paint(QPainter* p, const QRect& rect, const QVariant& data, const QFont&, const QFont& smallFont, const QFont&, const QColor& colText, const QColor&, bool, bool, const FeedPaintContext& ctx) const {
    auto map = data.toMap();
    bool expanded = map["expanded"].toBool();
    QString arrow = expanded ? QStringLiteral("\u25BC") : QStringLiteral("\u25B6");
    QFont gf = ctx.primaryFont.pointSize() > 0 ? ctx.primaryFont : smallFont;
    if (ctx.primaryFont.pointSize() <= 0) {
        gf.setPointSize(qMax(smallFont.pointSize() + 2, 11));
    } else {
        gf.setPointSize(gf.pointSize() + 1);
    }
    gf.setBold(true);
    gf.setStyleHint(QFont::SansSerif);

    const int padL = 10;
    const int padR = 12;
    const QString label = QStringLiteral("%1  %2").arg(arrow, map["label"].toString());
    QFontMetrics fm(gf);
    const QString elided = fm.elidedText(label, Qt::ElideRight, qMax(0, rect.width() - padL - padR));

    p->setFont(gf);
    p->setPen(colText);
    p->drawText(rect.adjusted(padL, 0, -padR, 0), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, elided);
}



ListFeedDelegate::ListFeedDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

ListFeedDelegate::~ListFeedDelegate() {
    qDeleteAll(m_blocks);
    delete m_fmMono;
    delete m_fmSmall;
    delete m_fmTiny;
}

void ListFeedDelegate::addBlock(FeedBlock* block)
{
    m_blocks.append(block);
    m_widthsDirty = true;
}

const FeedColors& ListFeedDelegate::feedColors() const {
    auto* style = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style());
    if (style) {
        const auto& theme = style->theme();
        if (theme.primaryColor != m_cachedPrimaryColor) {
            m_cachedPrimaryColor = theme.primaryColor;
            m_cachedColors = FeedColors::fromTheme();
        }
    } else {
        QStyle* currentStyle = qApp->style();
        if (currentStyle != m_cachedStyle) {
            m_cachedStyle = currentStyle;
            m_cachedColors = FeedColors::fromTheme();
        }
    }
    return m_cachedColors;
}

QFont ListFeedDelegate::monoFont() const { if (!m_fontsInited) initFonts(); return m_monoFont; }
QFont ListFeedDelegate::smallFont() const { if (!m_fontsInited) initFonts(); return m_smallFont; }
QFont ListFeedDelegate::tinyFont() const { if (!m_fontsInited) initFonts(); return m_tinyFont; }
QFont ListFeedDelegate::primaryFont() const { if (!m_fontsInited) initFonts(); return m_primaryFont; }
QFont ListFeedDelegate::microFont() const { if (!m_fontsInited) initFonts(); return m_microFont; }
QFont ListFeedDelegate::tagFont() const { if (!m_fontsInited) initFonts(); return m_tagFont; }
QFontMetrics ListFeedDelegate::fmMono() const { if (!m_fontsInited) initFonts(); return *m_fmMono; }
QFontMetrics ListFeedDelegate::fmSmall() const { if (!m_fontsInited) initFonts(); return *m_fmSmall; }
QFontMetrics ListFeedDelegate::fmTiny() const { if (!m_fontsInited) initFonts(); return *m_fmTiny; }

void ListFeedDelegate::initFonts() const {
    const AppTypography& ty = FontManager::instance().typography();
    m_monoFont    = ty.mono;
    m_smallFont   = ty.body;
    m_tinyFont    = ty.caption;
    m_primaryFont = ty.primary;
    m_microFont   = ty.micro;
    m_tagFont     = ty.tag;
    m_lineH1        = ty.lineH1;
    m_lineH2        = ty.lineH2;
    m_lineH1Compact = ty.lineH1Compact;
    m_lineGap       = ty.lineGap;

    delete m_fmMono;
    delete m_fmSmall;
    delete m_fmTiny;
    m_fmMono  = new QFontMetrics(m_monoFont);
    m_fmSmall = new QFontMetrics(m_smallFont);
    m_fmTiny  = new QFontMetrics(m_tinyFont);
    m_fontsInited = true;
}

void ListFeedDelegate::rebuildFonts() {
    m_fontsInited = false;
    initFonts();
    m_widthsDirty = true;
}

int ListFeedDelegate::paintIdBadge(QPainter* p, int x, int y, int lh, const QString& idStr, const QString& badgeStr, const QColor& colText, const QColor& colMuted) const {
    QFont idF = monoFont();
    p->setFont(idF);
    p->setPen(colText);
    QFontMetrics fmId(idF);
    int idW = fmId.horizontalAdvance(idStr) + 4;
    p->drawText(x, y, idW, lh, Qt::AlignLeft | Qt::AlignVCenter, idStr);
    QFont badgeF = tinyFont();
    p->setFont(badgeF);
    QFontMetrics fmB(badgeF);
    int badgeW = fmB.horizontalAdvance(badgeStr) + 10;
    QRect badgeRect(x + idW + 6, y + 2, badgeW, lh - 4);
    p->setPen(colMuted);
    p->setBrush(Qt::NoBrush);
    p->drawRoundedRect(badgeRect, 3, 3);
    p->drawText(badgeRect, Qt::AlignCenter, badgeStr);
    return idW + 6 + badgeW;
}

int ListFeedDelegate::paintTagBadges(QPainter* p, int x, int y, int lh, const QStringList& tags, const QColor& borderColor, const QColor& selectedColor, bool selected) const {
    if (tags.isEmpty()) return 0;
    QFont tf = tagFont();
    if (m_tagFontSize > 0)
        tf.setPointSize(m_tagFontSize);
    p->setFont(tf);
    QFontMetrics fm(tf);
    int startX = x;
    for (const QString& t : tags) {
        QString trimmed = t.trimmed();
        if (trimmed.isEmpty()) continue;
        int tw = fm.horizontalAdvance(trimmed) + 12;
        QRect tagRect(x, y + 1, tw, lh - 2);
        p->setPen(selected ? selectedColor : borderColor);
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(tagRect, 3, 3);
        p->drawText(tagRect, Qt::AlignCenter, trimmed);
        x += tw + 6;
    }
    return x - startX;
}

int ListFeedDelegate::paintRightAligned(QPainter* p, int rightX, int y, int lh, const QString& text, const QFont& font, const QColor& color) const {
    p->setFont(font);
    p->setPen(color);
    QFontMetrics fm(font);
    int w = fm.horizontalAdvance(text) + 4;
    p->drawText(rightX - w, y, w, lh, Qt::AlignLeft | Qt::AlignVCenter, text);
    return w;
}

int ListFeedDelegate::paintSeparator(QPainter* p, int x, int y, int lh) const {
    p->drawText(x, y, 6, lh, Qt::AlignCenter, "|");
    return 6;
}

QColor ListFeedDelegate::statusColor(const QString& status, const QColor& fallback) const {
    const FeedColors& fc = feedColors();
    if (status == "Success") return fc.success;
    if (status == "Error") return fc.error;
    if (status == "Canceled") return fc.canceled;
    if (status == "Running") return fc.running;
    if (status == "Hosted") return fc.hosted;
    if (status == "Disconnected") return fc.error;
    if (status == "No response") return fc.hosted;
    return fallback;
}

void ListFeedDelegate::updateMaxWidths(const FeedListModel* model) const {
    if (!model)
        return;

    m_cachedBlockW.resize(m_blocks.size());
    std::fill(m_cachedBlockW.begin(), m_cachedBlockW.end(), 0);

    m_cachedSubBlockW.resize(m_blocks.size());
    for (auto& v : m_cachedSubBlockW) v.clear();

    QFont idFont = monoFont();
    QFontMetrics fmId(idFont);

    int maxIdTextW = 0;
    int idBlockIdx = -1;

    int rows = model->size();
    for (int i = 0; i < m_blocks.size(); ++i) {
        FeedBlock::Policy pol = m_blocks[i]->policy();
        bool needCache = (pol == FeedBlock::Fixed || pol == FeedBlock::RightAlign);
        if (pol == FeedBlock::LeftFill && !m_blocks[i]->stretch())
            needCache = true;
        if (needCache) {
            int maxW = 0;
            for (int r = 0; r < rows; ++r) {
                const FeedRow& row = model->rowAt(r);
                if (row.isGroup || i >= row.size())
                    continue;
                int w = m_blocks[i]->measureWidth(row.blockData[i], monoFont(), smallFont(), tinyFont());
                if (w > maxW) maxW = w;
            }
            m_cachedBlockW[i] = maxW;
        }

        if (m_compact) {
            int divs = m_blocks[i]->compactDivisions();
            if (divs > 1) {
                m_cachedSubBlockW[i].resize(divs);
                std::fill(m_cachedSubBlockW[i].begin(), m_cachedSubBlockW[i].end(), 0);
                for (int d = 0; d < divs; ++d) {
                    int maxSub = 0;
                    for (int r = 0; r < rows; ++r) {
                        const FeedRow& row = model->rowAt(r);
                        if (row.isGroup || i >= row.size()) continue;
                        int sw = m_blocks[i]->measureCompactDivision(d, row.blockData[i], monoFont(), smallFont(), tinyFont());
                        if (sw > maxSub) maxSub = sw;
                    }
                    m_cachedSubBlockW[i][d] = maxSub;
                }
            } else if (needCache) {
                m_cachedSubBlockW[i] = { m_cachedBlockW[i] };
            }
        }

        if (m_blocks[i]->name() == "id") {
            idBlockIdx = i;
            for (int r = 0; r < rows; ++r) {
                const FeedRow& row = model->rowAt(r);
                if (row.isGroup || i >= row.size()) continue;
                auto map = row.blockData[i].toMap();
                int idTextW = fmId.horizontalAdvance(map["id"].toString()) + 4;
                if (idTextW > maxIdTextW) maxIdTextW = idTextW;
            }
        }
    }

    m_cachedMaxIdW = (idBlockIdx >= 0) ? m_cachedBlockW[idBlockIdx] : 0;
    m_cachedMaxIdTextW = maxIdTextW;

    for (int i = 0; i < m_blocks.size(); ++i) {
        if (m_blocks[i]->name() == "tags")  {
            m_cachedMaxTagsW  = m_cachedBlockW[i];
            break;
        }
    }
    for (int i = 0; i < m_blocks.size(); ++i) {
        if (m_blocks[i]->name() == "right") {
            m_cachedMaxRightW = m_cachedBlockW[i];
            break;
        }
    }
    for (int i = 0; i < m_blocks.size(); ++i) {
        if (m_blocks[i]->name() == "text")  {
            m_cachedMaxTextW = m_cachedBlockW[i];
            break;
        }
    }

    int maxStatusW = 0;
    int statusBlockIdx = -1;
    for (int i = 0; i < m_blocks.size(); ++i) {
        if (m_blocks[i]->name() == "right") {
            statusBlockIdx = i;
            break;
        }
    }
    if (statusBlockIdx >= 0 && model) {
        QFont bold = smallFont(); bold.setBold(true);
        QFontMetrics fmBold(bold);
        QFontMetrics fmSm(smallFont());
        QFont mainFont = smallFont();
        mainFont.setPointSize(smallFont().pointSize() + 1);
        mainFont.setBold(true);
        QFontMetrics fmMain(mainFont);
        for (int r = 0; r < model->rowCount(); ++r) {
            if (statusBlockIdx < model->rowAt(r).blockData.size()) {
                QVariantMap d = model->rowAt(r).blockData[statusBlockIdx].toMap();
                QString mainT = d["main"].toString();
                QString status = d["status"].toString();
                QString progress = d["progress"].toString();
                QString second = d["second"].toString();
                int w1 = 0;
                if (!mainT.isEmpty())
                    w1 += fmMain.horizontalAdvance(mainT) + 4;
                if (!progress.isEmpty())
                    w1 += 100;
                if (!status.isEmpty())
                    w1 += fmBold.horizontalAdvance(status) + 8;
                int w2 = second.isEmpty() ? 0 : fmSm.horizontalAdvance(second) + 4;
                maxStatusW = qMax(maxStatusW, qMax(w1, w2));
            }
        }
    }
    m_cachedMaxStatusW = maxStatusW > 0 ? maxStatusW + 8 : 0;
    m_widthsDirty = false;
}

void ListFeedDelegate::setCompactMode(bool compact) {
    if (m_compact != compact) {
        m_compact = compact;
        m_widthsDirty = true;
    }
}

void ListFeedDelegate::setRowHeights(int normal, int compact) {
    m_normalRowHeight = qMax(20, normal);
    m_compactRowHeight = qMax(20, compact);
}

void ListFeedDelegate::setIconSizes(int normal, int compact) {
    m_normalIconSize = qMax(8, normal);
    m_compactIconSize = qMax(8, compact);
}

void ListFeedDelegate::setBlockGaps(int normal, int compact) {
    m_normalBlockGap = qMax(0, normal);
    m_compactBlockGap = qMax(0, compact);
}

void ListFeedDelegate::setBlockGap(int gap) {
    m_blockGap = qMax(0, gap);
    m_normalBlockGap = m_blockGap;
    m_compactBlockGap = m_blockGap;
}

void ListFeedDelegate::setTagSize(int fontPointSize, int badgeHeight) {
    m_tagFontSize = qMax(6, fontPointSize);
    m_tagBadgeHeight = qMax(8, badgeHeight);
}

void ListFeedDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    QRect r = option.rect;
    QPalette pal = option.palette;
    bool dark = pal.color(QPalette::Base).lightnessF() < 0.5;
    bool selected = option.state & QStyle::State_Selected;

    const FeedListModel* fmodel = m_feedModel;
    if (!fmodel) {
        painter->restore();
        return;
    }

    auto* gproxy = qobject_cast<const GroupingProxyModel*>(index.model());
    if (gproxy && gproxy->isGroupIndex(index)) {
        int groupIdx = -1;
        for (int i = 0; i < m_blocks.size(); ++i)
            if (m_blocks[i]->name() == "group") {
                groupIdx = i;
                break;
            }
        if (groupIdx >= 0) {
            const FeedColors& fc = feedColors();
            QRect fullRow = r; fullRow.setLeft(0);
            QColor groupBg = fc.groupBg;
            if (selected)
                groupBg = fc.selectedWash(groupBg);
            painter->fillRect(fullRow, groupBg);
            if (selected) {
                QColor acc = fc.accent.isValid() ? fc.accent : fc.rowSelectedBg;
                QColor edge = acc;
                edge.setAlpha(fc.dark ? 150 : 140);
                painter->setPen(QPen(edge, 1.0));
                painter->setBrush(Qt::NoBrush);
                painter->drawRoundedRect(QRectF(fullRow).adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
                QColor strip = acc;
                strip.setAlpha(fc.dark ? 175 : 160);
                const int insetY = qMax(3, fullRow.height() / 7);
                painter->setPen(Qt::NoPen);
                painter->setBrush(strip);
                painter->drawRoundedRect( QRectF(fullRow.left() + 2.0, fullRow.top() + insetY, 2.5, qMax(4.0, qreal(fullRow.height() - 2 * insetY))), 1.2, 1.2);
            } else {
                QColor sep = fc.separatorLine;
                sep.setAlpha(fc.dark ? 60 : 70);
                painter->setPen(QPen(sep, 0.5));
                painter->drawLine(0, r.bottom(), r.right(), r.bottom());
            }

            bool expanded = option.state & QStyle::State_Open;
            QVariant groupData = index.data(Qt::DisplayRole);
            QVariantMap map;
            map["label"] = groupData.toString();
            map["expanded"] = expanded;

            FeedPaintContext ctx;
            ctx.primaryFont = primaryFont();
            ctx.compact = true; // group headers are always single-line
            const QColor colText = fc.textPrimary;
            const QColor colMuted = fc.textSecondary;
            m_blocks[groupIdx]->paint(painter, r, map, monoFont(), smallFont(), tinyFont(), colText, colMuted, selected, false, ctx);
        }
        painter->restore();
        return;
    }

    int sourceRow = index.row();
    QModelIndex idx = index;
    while (idx.model() && idx.model() != fmodel) {
        auto* sortProxy = qobject_cast<const QSortFilterProxyModel*>(idx.model());
        if (sortProxy) {
            idx = sortProxy->mapToSource(idx);
            continue;
        }
        auto* groupProxy = qobject_cast<const GroupingProxyModel*>(idx.model());
        if (groupProxy) {
            idx = groupProxy->mapToSource(idx);
            continue;
        }
        break;
    }
    if (idx.isValid())
        sourceRow = idx.row();
    if (sourceRow < 0 || sourceRow >= fmodel->rowCount()) {
        painter->restore();
        return;
    }

    const FeedRow& row = fmodel->rowAt(sourceRow);

    const FeedColors& fc = feedColors();
    bool dead = row.isDead;
    bool oddRow = sourceRow % 2 == 1;
    bool hovered = (option.state & QStyle::State_MouseOver) && !selected;
    QColor baseBg = fc.rowBg;
    QColor altBg = fc.rowAltBg;

    QRect fullRow = r; fullRow.setLeft(0);

    QColor surface;
    if (dead) {
        if (row.backgroundColor.isValid()) {
            float h, s, l, a;
            row.backgroundColor.getHslF(&h, &s, &l, &a);
            if (h < 0.0f)
                h = 0.0f;
            l = dark ? qMin(l + static_cast<float>(GlobalClient->settings->data.DeadLightnessShift), 1.0f) : qMax(l - static_cast<float>(GlobalClient->settings->data.DeadLightnessShift), 0.0f);
            surface = QColor::fromHslF(h, qBound(0.0f, s, 1.0f), qBound(0.0f, l, 1.0f), a);
        } else {
            surface = fc.rowDeadBg;
        }
    } else if (row.backgroundColor.isValid()) {
        surface = row.backgroundColor;
    } else {
        surface = oddRow ? altBg : baseBg;
    }

    if (selected)
        surface = fc.selectedWash(surface);
    else if (hovered) {
        QColor hi = fc.accent.isValid() ? fc.accent : fc.rowSelectedBg;
        QColor hov = dark ? surface.lighter(110) : surface.darker(102);
        surface = feedLerp(hov, hi, dark ? 0.08 : 0.05);
    }

    painter->fillRect(fullRow, surface);

    if (selected) {
        QColor acc = fc.accent.isValid() ? fc.accent : fc.rowSelectedBg;

        {
            QColor edge = acc;
            edge.setAlpha(dark ? 150 : 140);
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(QPen(edge, 1.0));
            painter->setBrush(Qt::NoBrush);
            const QRectF edgeR = QRectF(fullRow).adjusted(0.5, 0.5, -0.5, -0.5);
            painter->drawRoundedRect(edgeR, 4.0, 4.0);
        }

        {
            QColor strip = acc;
            strip.setAlpha(dark ? 175 : 160);
            const int insetY = qMax(3, fullRow.height() / 7);
            const QRectF stripR(fullRow.left() + 2.0, fullRow.top() + insetY, 2.5, qMax(4.0, qreal(fullRow.height() - 2 * insetY)));
            painter->setPen(Qt::NoPen);
            painter->setBrush(strip);
            painter->drawRoundedRect(stripR, 1.2, 1.2);
        }
    }

    if (!selected) {
        QColor sep = fc.separatorLine;
        sep.setAlpha(dark ? 45 : 55);
        painter->setPen(QPen(sep, 0.25));
        painter->drawLine(0, r.bottom(), r.right(), r.bottom());
    }

    QColor colText, colMuted;
    if (dead) {
        colText = dark ? fc.textDead.lighter(120) : fc.textDead.darker(120);
        colMuted = fc.textDead;
    } else {
        colText = fc.textPrimary;
        colMuted = fc.textSecondary;
    }

    if (m_widthsDirty)
        updateMaxWidths(fmodel);

    const bool compact = m_compact;
    int ml = r.left() + (compact ? 6 : 10);
    int mr = compact ? 6 : 12;
    if (!m_fontsInited)
        initFonts();
    int lh1 = compact ? m_lineH1Compact : m_lineH1;
    int lh2 = compact ? 0 : m_lineH2;
    int vgap = compact ? 0 : m_lineGap;
    int contentHeight = lh1 + lh2 + vgap;
    int topPad = compact ? 4 : 5;
    int y1 = r.top() + qMax(topPad, (r.height() - contentHeight) / 2);
    if (y1 + contentHeight > r.bottom())
        y1 = r.top() + topPad;
    int blockGap = m_blockGap;

    int iconSlotW = 0;
    int iconThisW = 0;
    int iconBlockIdx = -1;
    for (int i = 0; i < m_blocks.size(); ++i) {
        if (m_blocks[i]->name() == "icon") {
            iconBlockIdx = i;
            if (m_cachedBlockW.size() > i)
                iconSlotW = m_cachedBlockW[i];
            if (i < row.size()) {
                iconThisW = m_blocks[i]->measureWidth(row.blockData[i], monoFont(), smallFont(), tinyFont());
            }
            break;
        }
    }

    int rightEdge = r.right() - mr;
    struct RB { int idx; int w; int x; };
    QVector<RB> rightBlocks;
    for (int i = m_blocks.size() - 1; i >= 0; --i) {
        if (m_blocks[i]->policy() != FeedBlock::RightAlign)
            continue;
        int bw = 0;
        if (compact && m_cachedSubBlockW.size() > i && !m_cachedSubBlockW[i].isEmpty()) {
            for (int sw : m_cachedSubBlockW[i])
                bw += sw;
            int divs = m_cachedSubBlockW[i].size();
            if (divs > 1)
                bw += (divs - 1) * blockGap;
        } else {
            bw = m_cachedBlockW.size() > i ? m_cachedBlockW[i] : 0;
        }
        if (bw <= 0) continue;
        rightEdge -= bw;
        rightBlocks.append({i, bw, rightEdge});
        rightEdge -= blockGap;
    }
    int leftMaxRightEdge = rightEdge;

    QVector<QRect> blockRects(m_blocks.size());
    QVector<int> blockWidths(m_blocks.size(), 0);

    QVector<int> seqBlocks;
    for (int i = 0; i < m_blocks.size(); ++i) {
        if (m_blocks[i]->name() == "icon" || m_blocks[i]->name() == "group")
            continue;
        if (m_blocks[i]->policy() == FeedBlock::RightAlign)
            continue;
        seqBlocks.append(i);
    }

    int nSeq = 0;
    int sumFixedSeq = 0;
    int nStretch = 0;
    for (int i : seqBlocks) {
        bool isStr = (m_blocks[i]->policy() == FeedBlock::LeftFill && m_blocks[i]->stretch());
        int w = 0;
        if (compact && m_cachedSubBlockW.size() > i && !m_cachedSubBlockW[i].isEmpty()) {
            for (int sw : m_cachedSubBlockW[i])
                w += sw;
            int divs = m_cachedSubBlockW[i].size();
            if (divs > 1)
                w += (divs - 1) * blockGap;
        } else {
            w = (!isStr && m_cachedBlockW.size() > i) ? m_cachedBlockW[i] : 0;
        }
        if (isStr || w > 0) {
            nSeq++;
            if (isStr)
                nStretch++;
            else
                sumFixedSeq += w;
        }
    }

    int gapsInSeq = qMax(0, nSeq - 1);
    int sequenceStartX = ml + iconSlotW + blockGap;
    int seqAvailable = qMax(0, leftMaxRightEdge - sequenceStartX);
    int gapsSpace = gapsInSeq * blockGap;
    int spaceForStretches = qMax(0, seqAvailable - gapsSpace - sumFixedSeq);
    int stretchW = (nStretch > 0) ? (spaceForStretches / nStretch) : 0;
    int stretchRemainder = (nStretch > 0) ? (spaceForStretches - stretchW * nStretch) : 0;

    int x = ml;
    for (int i = 0; i < m_blocks.size(); ++i) {
        if (m_blocks[i]->name() == "icon") {
            blockRects[i] = QRect(x, y1, iconSlotW, contentHeight);
            blockWidths[i] = iconSlotW;
            x += iconSlotW + blockGap;
            break;
        }
    }
    bool isFirstSeq = true;
    int stretchesSeen = 0;
    for (int i : seqBlocks) {
        FeedBlock::Policy pol = m_blocks[i]->policy();
        bool isStr = (pol == FeedBlock::LeftFill && m_blocks[i]->stretch());
        int bw = 0;
        if (pol == FeedBlock::Fixed) {
            if (compact && m_cachedSubBlockW.size() > i && !m_cachedSubBlockW[i].isEmpty()) {
                for (int sw : m_cachedSubBlockW[i])
                    bw += sw;
                int divs = m_cachedSubBlockW[i].size();
                if (divs > 1)
                    bw += (divs - 1) * blockGap;
            } else {
                bw = (m_cachedBlockW.size() > i) ? m_cachedBlockW[i] : 0;
            }
        } else if (pol == FeedBlock::LeftFill) {
            if (isStr) {
                bw = stretchW;
            } else if (compact && m_cachedSubBlockW.size() > i && !m_cachedSubBlockW[i].isEmpty()) {
                for (int sw : m_cachedSubBlockW[i]) bw += sw;
                int divs = m_cachedSubBlockW[i].size();
                if (divs > 1) bw += (divs - 1) * blockGap;
            } else {
                bw = (m_cachedBlockW.size() > i) ? m_cachedBlockW[i] : 0;
            }
        } else {
            continue;
        }
        if (bw <= 0)
            continue;

        if (!isFirstSeq)
            x += blockGap;
        if (isStr) {
            stretchesSeen++;
            if (stretchesSeen == nStretch && stretchRemainder > 0) {
                bw += stretchRemainder;
            }
        }

        blockRects[i] = QRect(x, y1, bw, contentHeight);
        blockWidths[i] = bw;
        x += bw;
        isFirstSeq = false;
    }
    for (const auto& rb : rightBlocks) {
        blockRects[rb.idx] = QRect(rb.x, y1, rb.w, contentHeight);
        blockWidths[rb.idx] = rb.w;
    }

    if (leftMaxRightEdge > sequenceStartX) {
        for (int j = seqBlocks.size() - 1; j >= 0; --j) {
            int idx = seqBlocks[j];
            if (blockWidths[idx] > 0) {
                bool isStr = (m_blocks[idx]->policy() == FeedBlock::LeftFill && m_blocks[idx]->stretch());
                if (isStr) {
                    int curRight = blockRects[idx].x() + blockWidths[idx];
                    if (leftMaxRightEdge > curRight) {
                        int add = leftMaxRightEdge - curRight;
                        blockWidths[idx] += add;
                        blockRects[idx].setWidth(blockWidths[idx]);
                    }
                }
                break;
            }
        }
    }

    FeedPaintContext ctx;
    ctx.maxIdTextWidth = maxIdTextWidth();
    ctx.compact = compact;
    ctx.iconSize = compact ? m_compactIconSize : m_normalIconSize;
    ctx.tagFontSize = m_tagFontSize;
    ctx.tagBadgeHeight = m_tagBadgeHeight;
    ctx.lineH1 = m_lineH1;
    ctx.lineH2 = m_lineH2;
    ctx.lineH1Compact = m_lineH1Compact;
    ctx.lineGap = m_lineGap;
    ctx.primaryFont = primaryFont();
    ctx.microFont = microFont();
    ctx.tagFont = tagFont();

    for (int i = 0; i < m_blocks.size(); ++i) {
        if (i >= row.size())
            break;
        if (blockWidths[i] <= 0)
            continue;

        int divisions = compact ? m_blocks[i]->compactDivisions() : 1;

        painter->save();
        painter->setClipRect(blockRects[i]);

        if (divisions <= 1) {
            m_blocks[i]->paint(painter, blockRects[i], row.blockData[i], monoFont(), smallFont(), tinyFont(), colText, colMuted, selected, dead, ctx);
        } else {
            const int baseLeft = blockRects[i].left();
            const int avail = blockRects[i].width();
            const int lastDiv = divisions - 1;
            const bool isMain = (m_blocks[i]->name() == QLatin1String("main"));

            QVector<int> subWs(divisions, 0);
            for (int d = 0; d < divisions; ++d) {
                if (m_cachedSubBlockW.size() > i && !m_cachedSubBlockW[i].isEmpty() && d < m_cachedSubBlockW[i].size())
                    subWs[d] = m_cachedSubBlockW[i][d];
                else if (subWs[d] == 0)
                    subWs[d] = m_blocks[i]->measureCompactDivision(d, row.blockData[i], monoFont(), smallFont(), tinyFont());
            }

            int sumLead = 0;
            int nLead = 0;
            for (int d = 0; d < lastDiv; ++d) {
                if (subWs[d] > 0) {
                    sumLead += subWs[d];
                    ++nLead;
                }
            }
            const int leadGaps = nLead * blockGap;
            if (sumLead + leadGaps > avail && sumLead > 0) {
                const double scale = double(qMax(0, avail - leadGaps)) / double(sumLead);
                for (int d = 0; d < lastDiv; ++d)
                    subWs[d] = qRound(subWs[d] * scale);
            }

            int colPos = baseLeft;
            for (int d = 0; d < divisions; ++d) {
                const int remaining = baseLeft + avail - colPos;
                if (remaining <= 0)
                    break;

                const int colW = (d < subWs.size() ? subWs[d] : 0);

                if (d < lastDiv) {
                    if (colW <= 0)
                        continue;

                    int thisW = colW;
                    if (isMain) {
                        thisW = m_blocks[i]->measureCompactDivision(d, row.blockData[i], monoFont(), smallFont(), tinyFont());
                        if (thisW > colW)
                            thisW = colW;
                    }
                    thisW = qMin(thisW, remaining);
                    if (thisW < 0) thisW = 0;

                    QRect subRect(colPos, blockRects[i].top(), thisW, blockRects[i].height());
                    m_blocks[i]->paintCompactDivision(painter, subRect, d, row.blockData[i], monoFont(), smallFont(), tinyFont(), colText, colMuted, selected, dead, ctx);

                    const int adv = qMin(colW, remaining);
                    colPos += adv + blockGap;
                } else {
                    const int thisW = remaining;
                    QRect subRect(colPos, blockRects[i].top(), thisW, blockRects[i].height());
                    m_blocks[i]->paintCompactDivision(painter, subRect, d, row.blockData[i], monoFont(), smallFont(), tinyFont(), colText, colMuted, selected, dead, ctx);
                    colPos += thisW;
                }
            }
        }

        painter->restore();
    }

    painter->restore();
}

QSize ListFeedDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    Q_UNUSED(option);
    auto* gproxy = qobject_cast<const GroupingProxyModel*>(index.model());
    if (gproxy && gproxy->isGroupIndex(index)) {
        if (!m_fontsInited)
            initFonts();
        QFont gf = m_primaryFont;
        gf.setPointSize(gf.pointSize() > 0 ? gf.pointSize() + 1 : m_smallFont.pointSize() + 2);
        gf.setBold(true);
        const int textH = QFontMetrics(gf).height();
        const int h = qMax(28, textH + 12);
        return QSize(200, h);
    }
    return QSize(200, m_compact ? m_compactRowHeight : m_normalRowHeight);
}

bool ListFeedDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() != QEvent::MouseButtonRelease)
        return false;

    auto* me = static_cast<QMouseEvent*>(event);
    if (me->button() != Qt::LeftButton)
        return false;

    FeedListModel* fmodel = nullptr;
    if (m_feedModel)
        fmodel = m_feedModel;
    if (!fmodel)
        return false;

    int srcRow = index.row();
    if (srcRow < 0 || srcRow >= fmodel->size())
        return false;
    const FeedRow& row = fmodel->rowAt(srcRow);

    int ml = option.rect.left() + (m_compact ? 6 : 10);
    int blockGap = m_blockGap;
    int iconSlotW = 0;
    int iconThisW = 0;
    for (int i = 0; i < m_blocks.size(); ++i) {
        if (m_blocks[i]->name() == "icon") {
            if (m_cachedBlockW.size() > i) iconSlotW = m_cachedBlockW[i];
            if (i < row.size()) {
                if (!m_fontsInited)
                    initFonts();
                iconThisW = m_blocks[i]->measureWidth(row.blockData[i], m_monoFont, m_smallFont, m_tinyFont);
            }
            break;
        }
    }

    int blockX = ml + iconSlotW + blockGap;
    QPoint localPos = me->pos();
    for (int i = 0; i < m_blocks.size() && i < row.size(); ++i) {
        const auto& name = m_blocks[i]->name();
        int blockW = 0;
        if (name == "id" || name == "main" || name == "text" || name == "tags" || name == "right" || name == "attachment") {
            if (!m_fontsInited) initFonts();
            blockW = m_blocks[i]->measureWidth(row.blockData[i], m_monoFont, m_smallFont, m_tinyFont);
            if (blockW <= 0)
                continue;

            QRect blockRect(blockX, option.rect.top(), blockW, option.rect.height());
            int btnIdx = m_blocks[i]->hitTest(localPos, blockRect, row.blockData[i]);
            if (btnIdx >= 0) {
                Q_EMIT buttonClicked(i, btnIdx, index);
                return true;
            }
            blockX += blockW + blockGap;
        }
    }

    return false;
}



ListFeedWidget::ListFeedWidget(QWidget* parent) : QWidget(parent)
{
    m_treeView = new QTreeView(this);
    m_treeView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeView->setAlternatingRowColors(false);
    m_treeView->setUniformRowHeights(true);
    m_treeView->setHeaderHidden(true);
    m_treeView->setRootIsDecorated(false);
    m_treeView->setExpandsOnDoubleClick(true);
    m_treeView->setIndentation(16);
    m_treeView->setContentsMargins(0, 0, 0, 0);
    m_treeView->viewport()->setAutoFillBackground(false);
    m_treeView->setProperty("autoIconColor", QVariant::fromValue(oclero::qlementine::AutoIconColor::None));
    m_treeView->setAutoFillBackground(false);

    m_toolbarWidget = new QWidget(this);
    m_toolbarWidget->setFixedHeight(FontManager::instance().typography().toolbarHeight);
    m_toolbarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_toolbarLayout = new QHBoxLayout(m_toolbarWidget);
    m_toolbarLayout->setContentsMargins(0, 0, 0, 0);
    m_toolbarLayout->setSpacing(0);

    m_mainLayout = new QGridLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setVerticalSpacing(0);
    m_mainLayout->addWidget(m_toolbarWidget, 0, 0, 1, 1);
    m_mainLayout->addWidget(m_treeView, 1, 0, 1, 1);

    setLayout(m_mainLayout);

    applyTypography();
    connect(&FontManager::instance(), &FontManager::typographyChanged, this, &ListFeedWidget::applyTypography);
}

ListFeedWidget::~ListFeedWidget() = default;

QModelIndex ListFeedWidget::prepareContextMenuSelection(QAbstractItemView* view, const QPoint& viewportPos)
{
    if (!view)
        return {};
    QModelIndex index = view->indexAt(viewportPos);
    if (!index.isValid())
        return {};

    auto* sm = view->selectionModel();
    if (!sm)
        return index;

    bool alreadySelected = sm->isSelected(index);
    if (!alreadySelected) {
        const QModelIndexList selected = sm->selectedIndexes();
        for (const QModelIndex& s : selected) {
            if (s.row() == index.row() && s.parent() == index.parent()) {
                alreadySelected = true;
                break;
            }
        }
    }

    if (!alreadySelected) {
        sm->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        view->setCurrentIndex(index);
    }
    return index;
}

QModelIndex ListFeedWidget::prepareContextMenuSelection(const QPoint& viewportPos) const
{
    return prepareContextMenuSelection(m_treeView, viewportPos);
}

void ListFeedWidget::setDelegate(ListFeedDelegate* delegate)
{
    m_treeView->setItemDelegate(delegate);
    if (delegate) {
        delegate->setCompactMode(m_compactMode);
        delegate->rebuildFonts();
        delegate->setRowHeights(m_storedNormalRowH, m_storedCompactRowH);
        delegate->setIconSizes(m_storedNormalIcon, m_storedCompactIcon);
        delegate->setBlockGaps(m_storedNormalGap, m_storedCompactGap);
        delegate->setBlockGap(m_storedNormalGap);
        delegate->setTagSize(m_storedTagFont, m_storedTagBadgeH);
    }
}

void ListFeedWidget::applyTypography()
{
    const AppTypography& ty = FontManager::instance().typography();

    if (!m_manualRowHeights) {
        m_storedNormalRowH = ty.rowHeightNormal;
        m_storedCompactRowH = ty.rowHeightCompact;
    }
    if (!m_manualIconSizes) {
        m_storedNormalIcon = ty.iconNormal;
        m_storedCompactIcon = ty.iconCompact;
    }
    if (!m_manualTagSize) {
        m_storedTagFont = ty.tagFontSize;
        m_storedTagBadgeH = ty.tagBadgeHeight;
    }
    if (!m_manualBlockGaps) {
        m_storedNormalGap = ty.blockGap;
        m_storedCompactGap = ty.blockGap;
    }

    if (m_toolbarWidget)
        m_toolbarWidget->setFixedHeight(ty.toolbarHeight);

    const int ctrlH = ty.controlHeight;
    if (m_searchInput)
        m_searchInput->setFixedHeight(ctrlH);
    if (m_groupCombo)
        m_groupCombo->setFixedHeight(ctrlH);
    if (m_filterCombo)
        m_filterCombo->setFixedHeight(ctrlH);
    if (m_sortingCombo)
        m_sortingCombo->setFixedHeight(ctrlH);

    if (auto* del = qobject_cast<ListFeedDelegate*>(m_treeView->itemDelegate())) {
        del->rebuildFonts();
        del->setRowHeights(m_storedNormalRowH, m_storedCompactRowH);
        del->setIconSizes(m_storedNormalIcon, m_storedCompactIcon);
        del->setBlockGaps(m_storedNormalGap, m_storedCompactGap);
        del->setBlockGap(m_storedNormalGap);
        del->setTagSize(m_storedTagFont, m_storedTagBadgeH);
    }

    if (m_treeView) {
        m_treeView->doItemsLayout();
        m_treeView->viewport()->update();
    }
}

void ListFeedWidget::setModel(ListFeedModel* model)
{
    m_feedModel = model;
}

void ListFeedWidget::setFilterModel(QSortFilterProxyModel* filter)
{
    m_filterModel = filter;
}

void ListFeedWidget::enableGrouping(bool enable, AdaptixWidget* aw)
{
    m_groupingEnabled = enable;
    m_groupingAdaptixWidget = aw;
    m_treeView->setRootIsDecorated(enable);
    m_treeView->setExpandsOnDoubleClick(enable);
    m_treeView->setUniformRowHeights(!enable);
    rebuildModelChain();
}

void ListFeedWidget::setGroupingField(int field)
{
    m_groupingField = field;
    if (m_groupingEnabled)
        rebuildModelChain();
}

void ListFeedWidget::setGroupingScope(const QString& scope)
{
    if (scope.isEmpty() || scope == m_groupingScope)
        return;
    m_groupingScope = scope;
    if (m_proxyModel)
        rebuildModelChain();
}

void ListFeedWidget::rebuildModelChain()
{
    if (!m_feedModel)
        return;

    QAbstractItemModel* inputModel = m_filterModel ? static_cast<QAbstractItemModel*>(m_filterModel) : static_cast<QAbstractItemModel*>(m_feedModel);

    if (m_filterModel && m_filterModel->sourceModel() != m_feedModel)
        m_filterModel->setSourceModel(m_feedModel);

    QVector<GroupNode> savedGroups;
    ViewMode savedMode = VM_Flat;
    AutoGroupField savedField = AG_None;
    bool hadProxy = false;
    if (auto* old = groupingProxy()) {
        hadProxy = true;
        savedGroups = old->allCustomGroups();
        savedMode = old->viewMode();
        savedField = old->autoGroupField();
        old->deleteLater();
    }

    auto* groupingModel = new GroupingProxyModel(m_groupingAdaptixWidget, m_groupingScope, this);
    groupingModel->setSourceModel(inputModel);

    for (const GroupNode& g : savedGroups)
        groupingModel->addCustomGroup(g.groupId, g.parentGroupId, g.name, g.memberIds);

    if (hadProxy) {
        groupingModel->setViewMode(savedMode);
        if (savedMode == VM_AutoGroup)
            groupingModel->setAutoGroupField(savedField);
    } else if (m_groupingEnabled) {
        groupingModel->setViewMode(VM_AutoGroup);
        groupingModel->setAutoGroupField(static_cast<AutoGroupField>(m_groupingField));
    } else {
        groupingModel->setViewMode(VM_Flat);
    }

    m_proxyModel = groupingModel;
    m_treeView->setModel(groupingModel);
    const bool treeMode = groupingModel->viewMode() != VM_Flat;
    m_treeView->setUniformRowHeights(!treeMode);
    m_treeView->setRootIsDecorated(treeMode);
    m_treeView->setExpandsOnDoubleClick(treeMode);
    if (treeMode)
        m_treeView->expandAll();
}


PaginationBar::PaginationBar(QWidget* parent) : QWidget(parent)
{
    prevBtn = new QPushButton(this);
    prevBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowLeft));
    prevBtn->setToolTip("Previous page");
    prevBtn->setFixedSize(28, 28);
    prevBtn->setEnabled(false);

    infoLabel = new QLabel("0/0", this);
    infoLabel->setMinimumWidth(60);
    infoLabel->setAlignment(Qt::AlignCenter);

    nextBtn = new QPushButton(this);
    nextBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    nextBtn->setToolTip("Next page");
    nextBtn->setFixedSize(28, 28);
    nextBtn->setEnabled(false);

    pageSizeLabel = new QLabel("count", this);

    pageSizeSpin = new QSpinBox(this);
    pageSizeSpin->setRange(1, 1000);
    pageSizeSpin->setValue(100);
    pageSizeSpin->setSingleStep(10);
    pageSizeSpin->setFixedWidth(70);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);
    layout->addWidget(prevBtn);
    layout->addWidget(infoLabel);
    layout->addWidget(nextBtn);
    layout->addWidget(pageSizeLabel);
    layout->addWidget(pageSizeSpin);

    connect(prevBtn, &QPushButton::clicked, this, &PaginationBar::prevClicked);
    connect(nextBtn, &QPushButton::clicked, this, &PaginationBar::nextClicked);
    connect(pageSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PaginationBar::pageSizeChanged);
}

PaginationBar::~PaginationBar() = default;

void PaginationBar::setInfo(int from, int to, int total) { infoLabel->setText(QString("%1-%2 / %3").arg(from).arg(to).arg(total)); }
void PaginationBar::setPrevEnabled(bool enabled) { prevBtn->setEnabled(enabled); }
void PaginationBar::setNextEnabled(bool enabled) { nextBtn->setEnabled(enabled); }
void PaginationBar::setLoading(bool loading) { prevBtn->setEnabled(!loading); nextBtn->setEnabled(!loading); pageSizeSpin->setEnabled(!loading); }
int  PaginationBar::pageSize() const { return pageSizeSpin->value(); }

void ListFeedWidget::setupConnections()
{
}

void ListFeedWidget::enableSearch(bool enable)
{
    if (!m_searchWidget && enable) {
        m_searchWidget = new QWidget(m_toolbarWidget);
        auto* layout = new QHBoxLayout(m_searchWidget);
        layout->setContentsMargins(0, 3, 0, 3);
        layout->setSpacing(4);

        m_searchInput = new oclero::qlementine::LineEdit(m_searchWidget);
        m_searchInput->setIcon(QIcon(":/icons/search"));
        m_searchInput->setPlaceholderText("filter: (adm | user) & cmd");
        m_searchInput->setClearButtonEnabled(true);
        m_searchInput->setMinimumWidth(180);
        m_searchInput->setMaximumWidth(320);
        m_searchInput->setFixedHeight(FontManager::instance().typography().controlHeight);

        m_autoAction = new QAction(m_searchInput);
        m_autoAction->setCheckable(true);
        m_autoAction->setChecked(true);
        m_autoAction->setToolTip("Auto-apply filter on each keystroke");
        m_autoAction->setIcon(QIcon(":/icons/check"));
        m_searchInput->addAction(m_autoAction, QLineEdit::TrailingPosition);
        connect(m_autoAction, &QAction::toggled, this, [this](bool checked) {
            m_autoAction->setIcon(QIcon(checked ? ":/icons/check" : ":/icons/close"));
        });

        layout->addWidget(m_searchInput);

        connect(m_searchInput, &QLineEdit::textChanged, this, &ListFeedWidget::onFilterChanged);
        connect(m_searchInput, &QLineEdit::returnPressed, this, &ListFeedWidget::onFilterChanged);

        m_toolbarLayout->addWidget(m_searchWidget);
    }
    if (m_searchWidget)
        m_searchWidget->setVisible(enable);
}

void ListFeedWidget::finalizeSearchWidget()
{
    if (!m_searchWidget)
        return;

    auto* layout = qobject_cast<QHBoxLayout*>(m_searchWidget->layout());
    if (layout)
        layout->addStretch();
}

void ListFeedWidget::enableGroupCombo(bool enable)
{
    if (!m_groupCombo && enable) {
        m_groupCombo = new QComboBox(m_toolbarWidget);
        m_groupCombo->addItem("No grouping");
        m_groupCombo->addItem("By Domain");
        m_groupCombo->addItem("By Computer");
        m_groupCombo->addItem("By User");
        m_groupCombo->addItem("By Listener");
        m_groupCombo->addItem("By OS");
        m_groupCombo->addItem("By Type");
        m_groupCombo->setContentsMargins(4, 0, 4, 0);
        m_groupCombo->setMinimumWidth(120);
        m_groupCombo->setMaximumWidth(180);
        m_groupCombo->setFixedHeight(FontManager::instance().typography().controlHeight);

        connect(m_groupCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ListFeedWidget::onGroupModeChanged);

        m_toolbarLayout->addStretch();
        m_toolbarLayout->addWidget(m_groupCombo);
    }
    if (m_groupCombo)
        m_groupCombo->setVisible(enable);
}

void ListFeedWidget::enableActiveFilter(bool enable, const QString& label)
{
    if (!m_activeFilter && enable) {
        if (!m_searchWidget)
            enableSearch(true);
        auto* layout = qobject_cast<QHBoxLayout*>(m_searchWidget->layout());
        if (!layout)
            return;

        m_activeFilter = new QCheckBox(label.isEmpty() ? QStringLiteral("active only") : label, m_searchWidget);
        m_activeFilter->setChecked(false);
        layout->addWidget(m_activeFilter);

        connect(m_activeFilter, &QCheckBox::checkStateChanged, this, &ListFeedWidget::onFilterChanged);
    } else if (m_activeFilter && enable && !label.isEmpty()) {
        m_activeFilter->setText(label);
    }
    if (m_activeFilter)
        m_activeFilter->setVisible(enable);
}

void ListFeedWidget::enablePagination(bool enable)
{
    if (!m_paginationBar && enable) {
        if (!m_groupCombo)
            m_toolbarLayout->addStretch();
        m_paginationBar = new PaginationBar(m_toolbarWidget);
        m_toolbarLayout->addWidget(m_paginationBar);
    }
    if (m_paginationBar)
        m_paginationBar->setVisible(enable);
}

void ListFeedWidget::enableAutoCheck(bool enable)
{
    Q_UNUSED(enable);
}

void ListFeedWidget::enableFilterCombo(bool enable, const QString& placeholder)
{
    if (!m_filterCombo && enable) {
        if (!m_searchWidget) enableSearch(true);
        auto* layout = qobject_cast<QHBoxLayout*>(m_searchWidget->layout());
        if (!layout)
            return;

        m_filterCombo = new QComboBox(m_searchWidget);
        m_filterCombo->addItem(placeholder);
        m_filterCombo->setMinimumWidth(160);
        m_filterCombo->setMaximumWidth(180);
        m_filterCombo->setFixedHeight(FontManager::instance().typography().controlHeight);
        layout->addWidget(m_filterCombo);

        connect(m_filterCombo, &QComboBox::currentTextChanged, this, &ListFeedWidget::onFilterChanged);
    }
    if (m_filterCombo)
        m_filterCombo->setVisible(enable);
}

void ListFeedWidget::addToolbarWidgetBefore(QWidget* widget)
{
    if (!widget || !m_toolbarLayout)
        return;

    auto* spacer1 = new QWidget(m_toolbarWidget);
    spacer1->setFixedWidth(8);
    auto* sep = new QWidget(m_toolbarWidget);
    sep->setFixedWidth(1);
    sep->setFixedHeight(20);
    sep->setStyleSheet("background: rgba(255,255,255,0.08);");
    auto* spacer2 = new QWidget(m_toolbarWidget);
    spacer2->setFixedWidth(8);

    m_toolbarLayout->insertWidget(0, spacer2);
    m_toolbarLayout->insertWidget(0, sep);
    m_toolbarLayout->insertWidget(0, spacer1);
    m_toolbarLayout->insertWidget(0, widget);
}

void ListFeedWidget::addToolbarWidgetAfter(QWidget* widget)
{
    if (!widget || !m_toolbarLayout)
        return;

    auto* spacer1 = new QWidget(m_toolbarWidget);
    spacer1->setFixedWidth(8);

    auto* sep = new QWidget(m_toolbarWidget);
    sep->setFixedWidth(1);
    sep->setFixedHeight(20);
    sep->setStyleSheet("background: rgba(255,255,255,0.08);");

    auto* spacer2 = new QWidget(m_toolbarWidget);
    spacer2->setFixedWidth(8);

    m_toolbarLayout->addWidget(spacer1);
    m_toolbarLayout->addWidget(sep);
    m_toolbarLayout->addWidget(spacer2);
    m_toolbarLayout->addWidget(widget);
}

void ListFeedWidget::enableSortingCombo(bool enable, const QStringList& items)
{
    if (!m_sortingCombo && enable) {
        if (!m_searchWidget) enableSearch(true);
        auto* layout = qobject_cast<QHBoxLayout*>(m_searchWidget->layout());
        if (!layout)
            return;

        m_sortingCombo = new QComboBox(m_searchWidget);
        m_sortingCombo->setEditable(true);
        m_sortingCombo->lineEdit()->setReadOnly(true);
        m_sortingCombo->lineEdit()->setFocusPolicy(Qt::NoFocus);
        m_sortingCombo->blockSignals(true);
        if (items.isEmpty()) {
            m_sortingCombo->addItem("Newest first");
            m_sortingCombo->addItem("Oldest first");
            m_sortingCombo->addItem("Name A-Z");
            m_sortingCombo->addItem("Name Z-A");
        } else {
            m_sortingCombo->addItems(items);
        }
        m_sortingCombo->setMinimumWidth(160);
        m_sortingCombo->setMaximumWidth(180);
        m_sortingCombo->setFixedHeight(FontManager::instance().typography().controlHeight);
        m_sortingCombo->blockSignals(false);

        m_sortOrderAction = new QAction(m_sortingCombo);
        m_sortOrderAction->setCheckable(true);
        m_sortOrderAction->setChecked(true);
        m_sortOrderAction->setToolTip("Ascending");
        m_sortOrderAction->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
        m_sortingCombo->lineEdit()->addAction(m_sortOrderAction, QLineEdit::TrailingPosition);
        connect(m_sortOrderAction, &QAction::toggled, this, [this](bool checked) {
            m_sortAscending = checked;
            m_sortOrderAction->setIcon(style()->standardIcon(checked ? QStyle::SP_ArrowUp : QStyle::SP_ArrowDown));
            m_sortOrderAction->setToolTip(checked ? "Ascending" : "Descending");
            if (m_sortingCombo)
                onSortingChanged(m_sortingCombo->currentIndex());
        });

        layout->addWidget(m_sortingCombo);

        connect(m_sortingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ListFeedWidget::onSortingChanged);
    }
    if (m_sortingCombo)
        m_sortingCombo->setVisible(enable);
}

void ListFeedWidget::onFilterChanged(){}

void ListFeedWidget::onSortingChanged(int index)
{
    Q_UNUSED(index);
}

void ListFeedWidget::onGroupModeChanged(int index)
{
    if (!groupingProxy()) return;
    if (index == 0) {
        groupingProxy()->setViewMode(VM_Flat);
        m_treeView->setRootIsDecorated(false);
        m_treeView->setUniformRowHeights(true);
    } else {
        AutoGroupField field = AG_None;
        switch (index) {
            case 1: field = AG_ByDomain; break;
            case 2: field = AG_ByComputer; break;
            case 3: field = AG_ByUser; break;
            case 4: field = AG_ByListener; break;
            case 5: field = AG_ByOs; break;
            case 6: field = AG_ByAgentType; break;
        }
        groupingProxy()->setViewMode(VM_AutoGroup);
        groupingProxy()->setAutoGroupField(field);
        m_treeView->setRootIsDecorated(true);
        m_treeView->setExpandsOnDoubleClick(true);
        m_treeView->setUniformRowHeights(false);
    }
    m_treeView->expandAll();
}

void ListFeedWidget::enableCompactSwitch(bool enable)
{
    if (enable && !m_compactSwitch) {
        m_compactSwitch = new oclero::qlementine::Switch(this);
        m_compactSwitch->setFixedSize(36, 18);
        m_compactSwitch->setToolTip("Compact mode (single-line rows)");
        m_compactSwitch->setChecked(m_compactMode);

        connect(m_compactSwitch, &oclero::qlementine::Switch::toggled, this, [this](bool checked) {
            setCompactMode(checked);
        });

        if (m_searchWidget && m_searchInput) {
            if (auto* sLayout = qobject_cast<QHBoxLayout*>(m_searchWidget->layout())) {
                int idx = sLayout->indexOf(m_searchInput);
                if (idx >= 0) {
                    sLayout->insertWidget(idx + 1, m_compactSwitch);
                } else {
                    sLayout->addWidget(m_compactSwitch);
                }
            }
        } else {
            addToolbarWidgetAfter(m_compactSwitch);
        }

        if (auto* del = qobject_cast<ListFeedDelegate*>(m_treeView->itemDelegate())) {
            del->setIconSizes(m_storedNormalIcon, m_storedCompactIcon);
        }
    } else if (!enable && m_compactSwitch) {
        m_compactSwitch->setVisible(false);
        m_compactSwitch->deleteLater();
        m_compactSwitch = nullptr;
    }
}

int FeedBlock::measureCompactDivision(int division, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont) const {
    Q_UNUSED(division);
    return measureWidth(data, monoFont, smallFont, tinyFont);
}

void FeedBlock::paintCompactDivision(QPainter* p, const QRect& subRect, int division, const QVariant& data, const QFont& monoFont, const QFont& smallFont, const QFont& tinyFont, const QColor& colText, const QColor& colMuted, bool selected, bool dead, const FeedPaintContext& ctx) const {
    Q_UNUSED(division);
    paint(p, subRect, data, monoFont, smallFont, tinyFont, colText, colMuted, selected, dead, ctx);
}

void ListFeedWidget::setCompactMode(bool compact)
{
    m_compactMode = compact;
    if (auto* del = qobject_cast<ListFeedDelegate*>(m_treeView->itemDelegate())) {
        del->setCompactMode(compact);
        del->setIconSizes(m_storedNormalIcon, m_storedCompactIcon);
    }
    if (m_compactSwitch) {
        QSignalBlocker blocker(m_compactSwitch);
        m_compactSwitch->setChecked(compact);
    }
    m_treeView->doItemsLayout();
    m_treeView->viewport()->update();
}

bool ListFeedWidget::isCompactMode() const
{
    if (auto* del = qobject_cast<ListFeedDelegate*>(m_treeView->itemDelegate())) {
        return del->isCompactMode();
    }
    return m_compactMode;
}

void ListFeedWidget::setRowHeights(int normal, int compact)
{
    m_manualRowHeights = true;
    m_storedNormalRowH = qMax(20, normal);
    m_storedCompactRowH = qMax(20, compact);
    if (auto* del = qobject_cast<ListFeedDelegate*>(m_treeView->itemDelegate())) {
        del->setRowHeights(m_storedNormalRowH, m_storedCompactRowH);
    }
    m_treeView->doItemsLayout();
    m_treeView->viewport()->update();
}

void ListFeedWidget::setIconSizes(int normal, int compact)
{
    m_manualIconSizes = true;
    m_storedNormalIcon = qMax(8, normal);
    m_storedCompactIcon = qMax(8, compact);
    if (auto* del = qobject_cast<ListFeedDelegate*>(m_treeView->itemDelegate())) {
        del->setIconSizes(m_storedNormalIcon, m_storedCompactIcon);
    }
}

void ListFeedWidget::setBlockGaps(int normal, int compact)
{
    m_manualBlockGaps = true;
    m_storedNormalGap = qMax(0, normal);
    m_storedCompactGap = qMax(0, compact);
    if (auto* del = qobject_cast<ListFeedDelegate*>(m_treeView->itemDelegate())) {
        del->setBlockGaps(m_storedNormalGap, m_storedCompactGap);
        if (m_storedNormalGap == m_storedCompactGap) {
            del->setBlockGap(m_storedNormalGap);
        }
    }
}

void ListFeedWidget::setBlockGap(int gap)
{
    m_manualBlockGaps = true;
    m_storedNormalGap = qMax(0, gap);
    m_storedCompactGap = qMax(0, gap);
    if (auto* del = qobject_cast<ListFeedDelegate*>(m_treeView->itemDelegate())) {
        del->setBlockGap(gap);
    }
}

void ListFeedWidget::setTagSize(int fontPointSize, int badgeHeight)
{
    m_manualTagSize = true;
    m_storedTagFont = qMax(6, fontPointSize);
    m_storedTagBadgeH = qMax(8, badgeHeight);
    if (auto* del = qobject_cast<ListFeedDelegate*>(m_treeView->itemDelegate())) {
        del->setTagSize(m_storedTagFont, m_storedTagBadgeH);
    }
}
