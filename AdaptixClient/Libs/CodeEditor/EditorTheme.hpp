#pragma once

#include <SyntaxStyle.h>

#include <oclero/qlementine/style/Theme.hpp>

#include <QColor>
#include <QSet>
#include <cmath>

namespace EditorTheme {

namespace detail {

inline bool isLightTheme(const QColor& bg)
{
    double lum = 0.2126 * bg.redF() + 0.7152 * bg.greenF() + 0.0722 * bg.blueF();
    return lum > 0.4;
}

inline QColor adjustForBackground(const QColor& base, const QColor& bg, bool light)
{
    float h, s, v;
    base.getHsvF(&h, &s, &v);

    s = qBound(0.3f, s, 0.9f);

    if (light) {
        v = qBound(0.1f, v, 0.55f);
        s = qBound(0.55f, s, 1.0f);
    }
    else {
        v = qBound(0.65f, v, 1.0f);
        s = qBound(0.35f, s, 0.9f);
    }

    QColor result;
    result.setHsvF(h, s, v);
    return result;
}

inline QColor shiftHue(const QColor& base, qreal hueShift)
{
    float h, s, v;
    base.getHsvF(&h, &s, &v);
    h = std::fmod(h + (float)hueShift + 1.0f, 1.0f);
    QColor result;
    result.setHsvF(h, s, v);
    return result;
}

inline QColor desaturate(const QColor& base, qreal factor = 0.4)
{
    float h, s, v;
    base.getHsvF(&h, &s, &v);
    QColor result;
    result.setHsvF(h, s * (float)factor, v);
    return result;
}

inline QColor makeDistinct(QColor candidate, const QSet<QString>& used, const QColor& bg, bool light, qreal hueShift)
{
    int attempts = 0;
    while ((used.contains(candidate.name()) || candidate == bg) && attempts < 10) {
        candidate = detail::shiftHue(candidate, hueShift);
        candidate = detail::adjustForBackground(candidate, bg, light);
        attempts++;
    }
    return candidate;
}

}

inline SyntaxStyle* createFromQlementine(const oclero::qlementine::Theme& theme, QObject* parent = nullptr)
{
    auto* style = new SyntaxStyle(parent);

    QColor bg    = theme.backgroundColorMain1;
    QColor fg    = theme.secondaryColor;
    QColor muted = theme.secondaryAlternativeColor;
    bool light   = detail::isLightTheme(bg);

    QColor baseAccent = theme.primaryColor.isValid() ? theme.primaryColor : (light ? QColor(0x1a6eaa) : QColor(0x569cd6));
    float baseH, baseS, baseV;
    baseAccent.getHsvF(&baseH, &baseS, &baseV);

    QSet<QString> used;

    auto genColor = [&](qreal hueOffset, qreal satMul, qreal valOverride = -1) -> QColor {
        float h = std::fmod(baseH + (float)hueOffset + 1.0f, 1.0f);
        float s = qBound(0.3f, baseS * (float)satMul, 0.95f);
        float v = (valOverride >= 0) ? (float)valOverride : baseV;
        QColor c;
        c.setHsvF(h, s, v);
        c = detail::adjustForBackground(c, bg, light);
        c = detail::makeDistinct(c, used, bg, light, 0.05);
        used.insert(c.name());
        return c;
    };

    QColor c_keyword    = genColor(0.33, 1.0);
    QColor c_type       = genColor(0.50, 0.8);
    QColor c_function   = genColor(0.00, 1.0);
    QColor c_string     = genColor(0.58, 1.1);
    QColor c_number     = genColor(0.75, 0.9);
    QColor c_comment    = detail::desaturate(detail::shiftHue(baseAccent, 0.1), light ? 0.25 : 0.35);
    c_comment = detail::adjustForBackground(c_comment, bg, light);
    c_comment = detail::makeDistinct(c_comment, used, bg, light, 0.08);
    used.insert(c_comment.name());

    QColor c_preprocess = genColor(0.08, 0.7);

    QColor c_axglobal   = genColor(0.00, 1.0);
    QColor c_axagent    = genColor(0.62, 0.9);
    QColor c_axcommand  = genColor(0.55, 0.85);
    QColor c_axbof      = genColor(0.40, 0.85);
    QColor c_axfile     = genColor(0.38, 0.75);
    QColor c_axscript   = genColor(0.83, 0.8);
    QColor c_axencoding = genColor(0.08, 0.9);
    QColor c_axutil     = genColor(0.05, 0.75);

    auto ensureContrast = [&](QColor c, qreal minCR) -> QColor {
        for (int i = 0; i < 5; ++i) {
            double l1 = 0.2126 * bg.redF() + 0.7152 * bg.greenF() + 0.0722 * bg.blueF();
            double l2 = 0.2126 * c.redF()   + 0.7152 * c.greenF()   + 0.0722 * c.blueF();
            double cr = (qMax(l1,l2) + 0.05) / (qMin(l1,l2) + 0.05);
            if (cr >= minCR)
                break;

            float h, s, v;
            c.getHsvF(&h, &s, &v);
            if (light)
                v = qMax(0.0f, v - 0.1f);
            else
                v = qMin(1.0f, v + 0.1f);
            c.setHsvF(h, s, v);
        }
        return c;
    };

    c_keyword    = ensureContrast(c_keyword, 4.0);
    c_type       = ensureContrast(c_type, 3.5);
    c_function   = ensureContrast(c_function, 3.0);
    c_string     = ensureContrast(c_string, 4.0);
    c_number     = ensureContrast(c_number, 3.5);
    c_comment    = ensureContrast(c_comment, 2.5);
    c_preprocess = ensureContrast(c_preprocess, 2.0);
    c_axglobal   = ensureContrast(c_axglobal,   3.0);
    c_axagent    = ensureContrast(c_axagent,    3.0);
    c_axcommand  = ensureContrast(c_axcommand,  3.0);
    c_axbof      = ensureContrast(c_axbof,      3.0);
    c_axfile     = ensureContrast(c_axfile,     3.0);
    c_axscript   = ensureContrast(c_axscript,   3.0);
    c_axencoding = ensureContrast(c_axencoding, 3.0);
    c_axutil     = ensureContrast(c_axutil,     3.0);

    QColor curLine = light ? bg.darker(105) : bg.lighter(115);
    QColor foldBg  = light ? bg.darker(110) : bg.lighter(120);
    QColor selBg   = theme.primaryColor.isValid() ? theme.primaryColor : (light ? QColor(0x308cc6) : QColor(0x264f78));
    QColor selFg   = theme.primaryColorForeground.isValid() ? theme.primaryColorForeground : Qt::white;

    QString xml = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<style-scheme version=\"1.0\" name=\"Qlementine\">"
        "<style name=\"Text\" foreground=\"%1\" background=\"%2\"/>"
        "<style name=\"Selection\" foreground=\"%3\" background=\"%4\"/>"
        "<style name=\"LineNumber\" foreground=\"%5\"/>"
        "<style name=\"Parentheses\" foreground=\"%6\" background=\"%7\"/>"
        "<style name=\"CurrentLine\" background=\"%8\"/>"
        "<style name=\"CurrentLineNumber\" foreground=\"%9\" bold=\"true\"/>"
        "<style name=\"Occurrences\" background=\"%10\"/>"
        "<style name=\"Number\" foreground=\"%11\"/>"
        "<style name=\"String\" foreground=\"%12\"/>"
        "<style name=\"Type\" foreground=\"%13\"/>"
        "<style name=\"Function\" foreground=\"%14\"/>"
        "<style name=\"Keyword\" foreground=\"%15\"/>"
        "<style name=\"PrimitiveType\" foreground=\"%16\"/>"
        "<style name=\"Preprocessor\" foreground=\"%17\"/>"
        "<style name=\"Comment\" foreground=\"%18\"/>"
        "<style name=\"AxGlobal\" foreground=\"%25\"/>"
        "<style name=\"AxAgent\" foreground=\"%26\"/>"
        "<style name=\"AxCommand\" foreground=\"%27\"/>"
        "<style name=\"AxBOF\" foreground=\"%28\"/>"
        "<style name=\"AxFile\" foreground=\"%29\"/>"
        "<style name=\"AxScript\" foreground=\"%30\"/>"
        "<style name=\"AxEncoding\" foreground=\"%31\"/>"
        "<style name=\"AxUtil\" foreground=\"%32\"/>"
        "<style name=\"Operator\" foreground=\"%19\"/>"
        "<style name=\"Error\" underlineColor=\"%20\" underlineStyle=\"WaveUnderline\"/>"
        "<style name=\"Warning\" underlineColor=\"%21\" underlineStyle=\"WaveUnderline\"/>"
        "<style name=\"FoldMarker\" foreground=\"%22\" background=\"%23\"/>"
        "<style name=\"FoldMarkerHighlight\" foreground=\"%23\" background=\"%22\"/>"
        "<style name=\"FoldRegion\" background=\"%24\"/>"
        "</style-scheme>"
    )
    .arg(fg.name())            // 1  Text fg
    .arg(bg.name())            // 2  Text bg
    .arg(selFg.name())         // 3  Selection fg
    .arg(selBg.name())         // 4  Selection bg
    .arg(muted.name())         // 5  LineNumber
    .arg(c_number.name())      // 6  Parentheses fg
    .arg(foldBg.name())        // 7  Parentheses bg
    .arg(curLine.name())       // 8  CurrentLine
    .arg(fg.name())            // 9  CurrentLineNumber
    .arg(theme.neutralColor.name()) // 10 Occurrences
    .arg(c_number.name())      // 11 Number
    .arg(c_string.name())      // 12 String
    .arg(c_type.name())        // 13 Type
    .arg(c_function.name())    // 14 Function
    .arg(c_keyword.name())     // 15 Keyword
    .arg(c_keyword.name())     // 16 PrimitiveType
    .arg(c_preprocess.name())  // 17 Preprocessor
    .arg(c_comment.name())     // 18 Comment
    .arg(fg.name())            // 19 Operator
    .arg(c_number.name())      // 20 Error underline
    .arg(c_string.name())      // 21 Warning underline
    .arg(muted.name())         // 22 FoldMarker fg
    .arg(bg.name())            // 23 FoldMarker bg
    .arg(foldBg.name())        // 24 FoldRegion
    .arg(c_axglobal.name())    // 25 AxGlobal
    .arg(c_axagent.name())     // 26 AxAgent
    .arg(c_axcommand.name())   // 27 AxCommand
    .arg(c_axbof.name())       // 28 AxBOF
    .arg(c_axfile.name())      // 29 AxFile
    .arg(c_axscript.name())    // 30 AxScript
    .arg(c_axencoding.name())  // 31 AxEncoding
    .arg(c_axutil.name());     // 32 AxUtil

    style->load(xml);
    return style;
}

inline SyntaxStyle* createLightTheme(QObject* parent = nullptr)
{
    oclero::qlementine::Theme theme;
    return createFromQlementine(theme, parent);
}

inline SyntaxStyle* createDarkTheme(QObject* parent = nullptr)
{
    oclero::qlementine::Theme theme;

    theme.backgroundColorMain1 = QColor(0x1e1e1e);
    theme.backgroundColorMain2 = QColor(0x252526);
    theme.backgroundColorMain3 = QColor(0x2d2d30);
    theme.neutralColor = QColor(0x3e3e42);
    theme.secondaryColor = QColor(0xd4d4d4);
    theme.secondaryAlternativeColor = QColor(0x808080);
    theme.primaryColor = QColor(0x569cd6);
    theme.primaryAlternativeColor = QColor(0xdcddaa);
    theme.primaryColorForeground = QColor(0xffffff);
    theme.statusColorSuccess = QColor(0x6a9955);
    theme.statusColorWarning = QColor(0xce9178);
    theme.statusColorError = QColor(0xf44747);
    theme.statusColorInfo = QColor(0x4ec9b0);
    theme.statusColorSuccessDisabled = QColor(0x264f36);

    return createFromQlementine(theme, parent);
}

inline SyntaxStyle* createFromJsonPath(const QString& jsonPath, QObject* parent = nullptr)
{
    auto themeOpt = oclero::qlementine::Theme::fromJsonPath(jsonPath);
    if (themeOpt.has_value())
        return createFromQlementine(themeOpt.value(), parent);

    qWarning() << "EditorTheme: failed to load theme from" << jsonPath;
    return createDarkTheme(parent);
}

}