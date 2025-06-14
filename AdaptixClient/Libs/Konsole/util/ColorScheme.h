#ifndef COLORSCHEME_H
#define COLORSCHEME_H

#include <QHash>
#include <QList>
#include <QMetaType>
#include <QIODevice>
#include <QSet>
#include <QSettings>

#include "CharacterColor.h"

class QIODevice;

class ColorScheme
{
public:
    ColorScheme();
    ColorScheme(const ColorScheme& other);
    ~ColorScheme();

    void setDescription(const QString& description);
    QString description() const;

    void setName(const QString& name);
    QString name() const;

#if 0
    /** Reads the color scheme from the specified configuration source */
    void read(KConfig& config);
    /** Writes the color scheme to the specified configuration source */
    void write(KConfig& config) const;
#endif
    void read(const QString & filename);

    void setColorTableEntry(int index , const ColorEntry& entry);

    void getColorTable(ColorEntry* table) const;

    ColorEntry colorEntry(int index) const;

    QColor foregroundColor() const;

    QColor backgroundColor() const;

    bool hasDarkBackground() const;

    void setOpacity(qreal opacity);
    qreal opacity() const;
    void setRandomizedBackgroundColor(bool randomize);

    bool randomizedBackgroundColor() const;

    static QString colorNameForIndex(int index);
    static QString translatedColorNameForIndex(int index);

private:
    class RandomizationRange {
    public:
        RandomizationRange() : hue(0) , saturation(0) , value(0) {}

        bool isNull() const {
            return ( hue == 0 && saturation == 0 && value == 0 );
        }

        quint16 hue;
        quint8  saturation;
        quint8  value;
    };

    const ColorEntry* colorTable() const;

#if 0
    void readColorEntry(KConfig& config , int index);
    void writeColorEntry(KConfig& config , const QString& colorName, const ColorEntry& entry,const RandomizationRange& range) const;
#endif
    void readColorEntry(QSettings *s, int index);

    void setRandomizationRange( int index , quint16 hue , quint8 saturation , quint8 value );

    QString _description;
    QString _name;
    qreal _opacity;
    ColorEntry* _table;

    static const quint16 MAX_HUE = 340;

    RandomizationRange* _randomTable;

    static const char* const colorNames[TABLE_COLORS];
    static const char* const translatedColorNames[TABLE_COLORS];

    static const ColorEntry defaultTable[];
    
    ColorScheme& operator=(const ColorScheme&) = delete;
};

class AccessibleColorScheme : public ColorScheme
{
public:
    AccessibleColorScheme();
};

class ColorSchemeManager
{
public:

    ColorSchemeManager();

    ~ColorSchemeManager();

    const ColorScheme* defaultColorScheme() const;

    const ColorScheme* findColorScheme(const QString& name);

    bool deleteColorScheme(const QString& name);

    QList<const ColorScheme*> allColorSchemes();

    static ColorSchemeManager* instance();

    bool loadCustomColorScheme(const QString& path);

private:
    bool loadColorScheme(const QString& path);
    const static QStringList get_color_schemes_dirs();
    QList<QString> listColorSchemes();
    void loadAllColorSchemes();
    QString findColorSchemePath(const QString& name) const;

    QHash<QString,const ColorScheme*> _colorSchemes;
    QSet<ColorScheme*> _modifiedSchemes;

    bool _haveLoadedAll;

    static const ColorScheme _defaultColorScheme;
};

Q_DECLARE_METATYPE(const ColorScheme*)

#endif
