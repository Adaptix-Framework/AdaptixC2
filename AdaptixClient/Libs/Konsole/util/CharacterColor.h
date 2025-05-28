#ifndef CHARACTERCOLOR_H
#define CHARACTERCOLOR_H

#include <QColor>

class ColorEntry
{
public:
    enum FontWeight {
        Bold,
        Normal,
        UseCurrentFormat
    };

    ColorEntry(QColor c, bool tr, FontWeight weight = UseCurrentFormat) : color(c), transparent(tr), fontWeight(weight) {}

    ColorEntry() : transparent(false), fontWeight(UseCurrentFormat) {}

    QColor color;

    bool   transparent;
    FontWeight fontWeight;
};

#define BASE_COLORS   (2+8)
#define INTENSITIES   2
#define TABLE_COLORS  (INTENSITIES*BASE_COLORS)

#define DEFAULT_FORE_COLOR 0
#define DEFAULT_BACK_COLOR 1

extern const ColorEntry base_color_table[TABLE_COLORS];

#define COLOR_SPACE_UNDEFINED   0
#define COLOR_SPACE_DEFAULT     1
#define COLOR_SPACE_SYSTEM      2
#define COLOR_SPACE_256         3
#define COLOR_SPACE_RGB         4

class CharacterColor
{
    friend class Character;

public:
    CharacterColor()
        : _colorSpace(COLOR_SPACE_UNDEFINED)
        , _u(0)
        , _v(0)
        , _w(0) {
    }

    CharacterColor(quint8 colorSpace, int co)
        : _colorSpace(colorSpace)
        , _u(0)
        , _v(0)
        , _w(0) {
        switch (colorSpace) {
            case COLOR_SPACE_DEFAULT:
                _u = co & 1;
                break;
            case COLOR_SPACE_SYSTEM:
                _u = co & 7;
                _v = (co >> 3) & 1;
                break;
            case COLOR_SPACE_256:
                _u = co & 255;
                break;
            case COLOR_SPACE_RGB:
                _u = co >> 16;
                _v = co >> 8;
                _w = co;
                break;
            default:
                _colorSpace = COLOR_SPACE_UNDEFINED;
        }
    }

    CharacterColor(quint8 colorSpace, QColor color)
        : _colorSpace(colorSpace)
        , _u(0)
        , _v(0)
        , _w(0) {
        switch (colorSpace) {
            case COLOR_SPACE_RGB:
                _u = color.red();
                _v = color.green();
                _w = color.blue();
                break;
            default:
                _colorSpace = COLOR_SPACE_UNDEFINED;
                break;
        }
    }

    bool isValid() const {
        return _colorSpace != COLOR_SPACE_UNDEFINED;
    }

    void setIntensive();

    QColor color(const ColorEntry* palette) const;

    friend bool operator == (const CharacterColor& a, const CharacterColor& b);

    friend bool operator != (const CharacterColor& a, const CharacterColor& b);

private:
    quint8 _colorSpace;

    // bytes storing the character color
    quint8 _u;
    quint8 _v;
    quint8 _w;
};

inline bool operator == (const CharacterColor& a, const CharacterColor& b) {
    return a._colorSpace == b._colorSpace &&
           a._u == b._u &&
           a._v == b._v &&
           a._w == b._w;
}

inline bool operator != (const CharacterColor& a, const CharacterColor& b) {
    return !operator==(a,b);
}

inline QColor color256(quint8 u, const ColorEntry *base) {
    if (u <   8) return base[u+2            ].color;
    u -= 8;
    if (u <   8) return base[u+2+BASE_COLORS].color;
    u -= 8;

    //  16..231: 6x6x6 rgb color cube
    if (u < 216) return QColor(((u/36)%6) ? (40*((u/36)%6)+55) : 0,
                                ((u/ 6)%6) ? (40*((u/ 6)%6)+55) : 0,
                                ((u/ 1)%6) ? (40*((u/ 1)%6)+55) : 0);
    u -= 216;

    // 232..255: gray, leaving out black and white
    int gray = u*10+8; return QColor(gray,gray,gray);
}

inline QColor CharacterColor::color(const ColorEntry* base) const {
    switch (_colorSpace) {
        case COLOR_SPACE_DEFAULT: return base[_u+0+(_v?BASE_COLORS:0)].color;
        case COLOR_SPACE_SYSTEM: return base[_u+2+(_v?BASE_COLORS:0)].color;
        case COLOR_SPACE_256: return color256(_u,base);
        case COLOR_SPACE_RGB: return {_u,_v,_w};
        case COLOR_SPACE_UNDEFINED: return QColor();
        default: ;
    }

    Q_ASSERT(false);
    return QColor();
}

inline void CharacterColor::setIntensive() {
    if (_colorSpace == COLOR_SPACE_SYSTEM || _colorSpace == COLOR_SPACE_DEFAULT) {
        _v = 1;
    }
}

#endif // CHARACTERCOLOR_H