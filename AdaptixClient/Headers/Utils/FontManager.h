#ifndef ADAPTIXCLIENT_FONTMANAGER_H
#define ADAPTIXCLIENT_FONTMANAGER_H

#include <QFont>
#include <QFontDatabase>
#include <QObject>
#include <QString>
#include <QMap>

struct AppTypography {
    int     baseSize = 10;
    QString family;

    QFont regular;   // N
    QFont bold;      // N Bold
    QFont mono;      // N (same family; used for IDs / console / terminal)
    QFont primary;   // N+1 Bold  — main titles, agent names, group headers
    QFont body;      // N         — secondary text, dates
    QFont caption;   // max(N-1,7) — badges
    QFont micro;     // max(N-2,6) — progress labels, tiny chips
    QFont tag;       // N         — tag pills (readable, not as loud as primary)

    int rowHeightNormal  = 54;
    int rowHeightCompact = 30;
    int iconNormal       = 22;
    int iconCompact      = 18;
    int tagBadgeHeight   = 20;
    int tagFontSize      = 10;
    int lineH1           = 18;  // first line slot in 2-line feed rows
    int lineH2           = 16;  // second line
    int lineH1Compact    = 15;
    int lineGap          = 3;
    int blockGap         = 12;
    int toolbarHeight    = 36;
    int controlHeight    = 28;   // combos / search fields / history bar
    int controlInnerH    = 22;   // buttons inside chrome bars
    int historyBarHeight = 28;

    int chromeFontPx  = 11;  // secondary chrome (labels, tool buttons)
    int captionFontPx = 10;  // uppercase section headers
    int titleFontPx   = 13;  // dialog headers
    int chipFontPx    = 13;  // agent profile chips

    int tabBarHeight   = 24;  // project tabs in MainUI
    int mainToolbarH   = 40;  // top/bottom Adaptix toolbar
    int sideToolbarW   = 72;  // left/right Adaptix toolbar
    int segmentHeight  = 26;  // SegmentedControl strips

    int chatAvatarSize  = 32;
    int chatReplyH      = 32;
    int chatNameH       = 16;
    int chatInputH      = 68;
    int chatReactionH   = 22;
    int chatTimeH       = 14;

    qreal graphNodeSize  = 100.0;
    qreal graphNoteH     = 50.0;
    qreal graphBadgeW    = 42.0;
    qreal graphBadgeH    = 24.0;
    qreal graphBadgeOffX = 38.0;
    qreal graphBadgeOffY = -6.0;
};

class FontManager : public QObject
{
    Q_OBJECT

    QMap<QString, QString> m_loadedFonts;
    bool m_initialized = false;
    AppTypography m_typography;

    FontManager();
    ~FontManager() override = default;
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    void loadApplicationFonts();
    QString findBestMonospaceFont();
    static QFont makeFont(const QString& family, int pointSize, QFont::Weight weight = QFont::Normal);

public:
    static FontManager& instance();

    void    initialize();
    QFont   getFont(const QString& fontName, int pointSize = -1);
    bool    isFontAvailable(const QString& fontName);
    QString resolveFamily(const QString& fontName);
    QFont   getDefaultMonospaceFont(int pointSize = -1);

    void rebuildTypography(const QString& family, int pointSize);

    const AppTypography& typography() const { return m_typography; }

    QFont appMonoFont() const { return m_typography.mono; }
    QFont appRegularFont() const { return m_typography.regular; }

Q_SIGNALS:
    void typographyChanged();
};

#endif
