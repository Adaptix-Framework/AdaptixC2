#ifndef CHARACTER_H
#define CHARACTER_H

#include <QHash>
#include <QSet>

#include "CharacterColor.h"

typedef unsigned char LineProperty;

static const int LINE_DEFAULT          = 0;
static const int LINE_WRAPPED          = (1 << 0);
static const int LINE_DOUBLEWIDTH      = (1 << 1);
static const int LINE_DOUBLEHEIGHT     = (1 << 2);

#define DEFAULT_RENDITION  0
#define RE_BOLD            (1 << 0)
#define RE_BLINK           (1 << 1)
#define RE_UNDERLINE       (1 << 2)
#define RE_REVERSE         (1 << 3) // Screen only
#define RE_INTENSIVE       (1 << 3) // Widget only
#define RE_ITALIC          (1 << 4)
#define RE_CURSOR          (1 << 5)
#define RE_EXTENDED_CHAR   (1 << 6)
#define RE_FAINT           (1 << 7)
#define RE_STRIKEOUT       (1 << 8)
#define RE_CONCEAL         (1 << 9)
#define RE_OVERLINE        (1 << 10)

class ScreenWindow;

class Character
{
public:

    inline Character(quint16 _c = ' ',
            CharacterColor  _f = CharacterColor(COLOR_SPACE_DEFAULT,DEFAULT_FORE_COLOR),
            CharacterColor  _b = CharacterColor(COLOR_SPACE_DEFAULT,DEFAULT_BACK_COLOR),
            quint8  _r = DEFAULT_RENDITION)
        : character(_c)
        , rendition(_r)
        , foregroundColor(_f)
        , backgroundColor(_b) {
    }

    wchar_t character;

    quint8  rendition;

    CharacterColor  foregroundColor;
    CharacterColor  backgroundColor;

    bool   isTransparent(const ColorEntry* palette) const;

    ColorEntry::FontWeight fontWeight(const ColorEntry* base) const;

    bool equalsFormat(const Character &other) const;

    friend bool operator == (const Character& a, const Character& b);

    friend bool operator != (const Character& a, const Character& b);

    inline bool isLineChar() const {
        return (rendition & RE_EXTENDED_CHAR) ? false : ((character & 0xFF80) == 0x2500);
    }

    inline bool isSpace() const {
        return (rendition & RE_EXTENDED_CHAR) ? false : QChar(character).isSpace();
    }
};

inline bool operator == (const Character& a, const Character& b) {
    return a.character == b.character &&
           a.rendition == b.rendition &&
           a.foregroundColor == b.foregroundColor &&
           a.backgroundColor == b.backgroundColor;
}

inline bool operator != (const Character& a, const Character& b) {
    return a.character != b.character ||
           a.rendition != b.rendition ||
           a.foregroundColor != b.foregroundColor ||
           a.backgroundColor != b.backgroundColor;
}

inline bool Character::isTransparent(const ColorEntry* base) const {
    return ((backgroundColor._colorSpace == COLOR_SPACE_DEFAULT) &&
             base[backgroundColor._u+0+(backgroundColor._v?BASE_COLORS:0)].transparent)
        || ((backgroundColor._colorSpace == COLOR_SPACE_SYSTEM) &&
             base[backgroundColor._u+2+(backgroundColor._v?BASE_COLORS:0)].transparent);
}

inline bool Character::equalsFormat(const Character& other) const {
    return backgroundColor==other.backgroundColor &&
           foregroundColor==other.foregroundColor &&
           rendition==other.rendition;
}

inline ColorEntry::FontWeight Character::fontWeight(const ColorEntry* base) const {
    if (backgroundColor._colorSpace == COLOR_SPACE_DEFAULT)
        return base[backgroundColor._u+0+(backgroundColor._v?BASE_COLORS:0)].fontWeight;
    else if (backgroundColor._colorSpace == COLOR_SPACE_SYSTEM)
        return base[backgroundColor._u+2+(backgroundColor._v?BASE_COLORS:0)].fontWeight;
    else
        return ColorEntry::UseCurrentFormat;
}

class ExtendedCharTable
{
public:
    ExtendedCharTable();
    ~ExtendedCharTable();

    uint createExtendedChar(uint* unicodePoints , ushort length);

    uint* lookupExtendedChar(uint hash , ushort& length) const;

    QSet<ScreenWindow*> windows;

    static ExtendedCharTable instance;

private:
    uint extendedCharHash(uint* unicodePoints , ushort length) const;
    bool extendedCharMatch(uint hash , uint* unicodePoints , ushort length) const;

    QHash<uint,uint*> extendedCharTable;
};

Q_DECLARE_TYPEINFO(Character, Q_MOVABLE_TYPE);

#endif // CHARACTER_H