#include <Utils/FontManager.h>
#include <QFontInfo>
#include <QFontMetrics>
#include <QtMath>

FontManager::FontManager() : QObject(nullptr) {}

FontManager& FontManager::instance()
{
    static FontManager instance;
    return instance;
}

void FontManager::initialize()
{
    if (m_initialized)
        return;

    rebuildTypography(QStringLiteral("JetBrains Mono"), 10);
}

void FontManager::loadApplicationFonts()
{
    struct FontResource {
        QString resourcePath;
        QString alias;
    };

    QList<FontResource> fonts = {
        {":/fonts/Hack", "Hack"},
        {":/fonts/Hack_B", "Hack"},
        {":/fonts/Hack_BI", "Hack"},
        {":/fonts/Hack_I", "Hack"},
        {":/fonts/JetBrainsMono", "JetBrains Mono"},
        {":/fonts/JetBrainsMono_B", "JetBrains Mono"},
        {":/fonts/JetBrainsMono_BI", "JetBrains Mono"},
        {":/fonts/JetBrainsMono_I", "JetBrains Mono"}
    };

    for (const auto& fontRes : fonts) {
        int fontId = QFontDatabase::addApplicationFont(fontRes.resourcePath);
        if (fontId != -1) {
            QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
            if (!fontFamilies.isEmpty()) {
                QString actualFamilyName = fontFamilies.first();
                m_loadedFonts[fontRes.alias] = actualFamilyName;
            }
        }
    }

    for (auto it = m_loadedFonts.begin(); it != m_loadedFonts.end(); ++it) {
        QFont testFont(it.value());
        QFontInfo fontInfo(testFont);
        if (fontInfo.family() != it.value() && !fontInfo.family().startsWith(it.value())) {
            QStringList allFamilies = QFontDatabase::families();
            for (const QString& family : allFamilies) {
                if (family.contains(it.key(), Qt::CaseInsensitive) || family.contains(it.value(), Qt::CaseInsensitive)) {
                    it.value() = family;
                    break;
                }
            }
        }
    }
}

QFont FontManager::makeFont(const QString& family, int pointSize, QFont::Weight weight)
{
    QFont f(family);
    f.setPointSize(qMax(6, pointSize));
    f.setWeight(weight);
    f.setStyleHint(QFont::Monospace);
    f.setFixedPitch(true);
    return f;
}

void FontManager::rebuildTypography(const QString& family, int pointSize)
{
    if (!m_initialized) {
        loadApplicationFonts();
        m_initialized = true;
    }

    const int N = qBound(7, pointSize, 30);
    QString fam = family.isEmpty() ? QStringLiteral("JetBrains Mono") : family;
    if (m_loadedFonts.contains(fam))
        fam = m_loadedFonts[fam];
    else if (family.isEmpty() || family == QLatin1String("JetBrains Mono"))
        fam = resolveFamily(QStringLiteral("JetBrains Mono"));

    AppTypography t;
    t.baseSize = N;
    t.family   = fam;

    t.regular = makeFont(fam, N, QFont::Normal);
    t.bold    = makeFont(fam, N, QFont::Bold);
    t.mono    = makeFont(fam, N, QFont::Normal);
    t.primary = makeFont(fam, N + 1, QFont::Bold);
    t.body    = makeFont(fam, N, QFont::Normal);
    t.caption = makeFont(fam, qMax(7, N - 1), QFont::Normal);
    t.micro   = makeFont(fam, qMax(6, N - 2), QFont::Normal);
    t.tag     = makeFont(fam, N, QFont::Normal);

    const qreal s = qMax(0.7, N / 10.0);

    const int bodyH    = QFontMetrics(t.body).height();
    const int primaryH = QFontMetrics(t.primary).height();

    t.lineH1        = qMax(primaryH + 4, qRound(18 * s));
    t.lineH2        = qMax(bodyH + 4, qRound(16 * s));
    t.lineH1Compact = qMax(bodyH + 4, qRound(15 * s));
    t.lineGap       = qMax(2, qRound(3 * s));

    const int vPadNormal  = qMax(8, qRound(10 * s));
    const int vPadCompact = qMax(6, qRound(8 * s));
    t.rowHeightNormal  = t.lineH1 + t.lineGap + t.lineH2 + vPadNormal;
    t.rowHeightCompact = t.lineH1Compact + vPadCompact;
    if (N == 10) {
        t.rowHeightNormal  = qMax(t.rowHeightNormal, 54);
        t.rowHeightCompact = qMax(t.rowHeightCompact, 30);
    }

    t.iconNormal     = qBound(14, qRound(22 * s), 36);
    t.iconCompact    = qBound(12, qRound(18 * s), 30);
    t.tagFontSize    = N;
    t.tagBadgeHeight = qMax(QFontMetrics(t.tag).height() + 4, qRound(20 * s));
    t.blockGap       = qMax(6, qRound(12 * s));
    t.toolbarHeight  = qMax(28, qRound(36 * s));
    t.controlHeight  = qMax(22, qRound(28 * s));
    t.controlInnerH  = qMax(18, t.controlHeight - 6);
    t.historyBarHeight = t.controlHeight;

    t.chromeFontPx  = qMax(9, N + 1);   // 11 @ N=10
    t.captionFontPx = qMax(8, N);       // 10 @ N=10
    t.titleFontPx   = qMax(11, N + 3);  // 13 @ N=10
    t.chipFontPx    = qMax(11, N + 3);  // 13 @ N=10

    t.tabBarHeight  = qMax(20, qRound(24 * s));
    t.mainToolbarH  = qMax(32, qRound(40 * s));
    t.sideToolbarW  = qMax(56, qRound(72 * s));
    t.segmentHeight = qMax(22, qRound(26 * s));

    t.chatAvatarSize = qMax(24, qRound(32 * s));
    t.chatReplyH     = qMax(24, qRound(32 * s));
    t.chatNameH      = qMax(12, QFontMetrics(t.caption).height() + 2);
    t.chatInputH     = qMax(48, qRound(68 * s));
    t.chatReactionH  = qMax(18, qRound(22 * s));
    t.chatTimeH      = qMax(12, QFontMetrics(t.micro).height() + 2);

    t.graphNodeSize  = 100.0 * s;
    t.graphNoteH     = qMax(36.0, 50.0 * s);
    t.graphBadgeW    = qMax(32.0, 42.0 * s);
    t.graphBadgeH    = qMax(18.0, 24.0 * s);
    t.graphBadgeOffX = 38.0 * s;
    t.graphBadgeOffY = -6.0 * s;

    m_typography = t;
    Q_EMIT typographyChanged();
}

QFont FontManager::getFont(const QString& fontName, int pointSize)
{
    if (!m_initialized)
        initialize();

    QFont font;

    if (m_loadedFonts.contains(fontName)) {
        font = QFont(m_loadedFonts[fontName]);
    } else {
        font = getDefaultMonospaceFont();
    }

    if (pointSize > 0)
        font.setPointSize(pointSize);
    else
        font.setPointSize(m_typography.baseSize > 0 ? m_typography.baseSize : 10);

    return font;
}

bool FontManager::isFontAvailable(const QString& fontName)
{
    if (!m_initialized)
        initialize();

    return m_loadedFonts.contains(fontName);
}

QString FontManager::resolveFamily(const QString& fontName)
{
    if (!m_initialized)
        initialize();

    if (m_loadedFonts.contains(fontName))
        return m_loadedFonts[fontName];

    return fontName;
}

QFont FontManager::getDefaultMonospaceFont(int pointSize)
{
    if (!m_initialized)
        initialize();

    const int sz = pointSize > 0 ? pointSize : (m_typography.baseSize > 0 ? m_typography.baseSize : 10);

    if (m_loadedFonts.contains("JetBrains Mono")) {
        QFont font(m_loadedFonts["JetBrains Mono"]);
        font.setPointSize(sz);
        return font;
    }

    if (m_loadedFonts.contains("Hack")) {
        QFont font(m_loadedFonts["Hack"]);
        font.setPointSize(sz);
        return font;
    }

    QFont font;
    font.setFamily("monospace");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(sz);
    return font;
}

QString FontManager::findBestMonospaceFont()
{
    if (!m_initialized)
        initialize();

    if (m_loadedFonts.contains("JetBrains Mono"))
        return "JetBrains Mono";

    if (m_loadedFonts.contains("Hack"))
        return "Hack";

    return "monospace";
}
