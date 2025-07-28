#include "ColorScheme.h"

#include <QBrush>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QStringView>
#include <QtDebug>

const ColorEntry ColorScheme::defaultTable[TABLE_COLORS] = {
    ColorEntry(QColor(0x00, 0x00, 0x00), false),
    ColorEntry(QColor(0xFF, 0xFF, 0xFF), true) ,
    ColorEntry(QColor(0x00, 0x00, 0x00), false),
    ColorEntry(QColor(0xB2, 0x18, 0x18), false),
    ColorEntry(QColor(0x18, 0xB2, 0x18), false),
    ColorEntry(QColor(0xB2, 0x68, 0x18), false),
    ColorEntry(QColor(0x18, 0x18, 0xB2), false),
    ColorEntry(QColor(0xB2, 0x18, 0xB2), false),
    ColorEntry(QColor(0x18, 0xB2, 0xB2), false),
    ColorEntry(QColor(0xB2, 0xB2, 0xB2), false),
    ColorEntry(QColor(0x00, 0x00, 0x00), false),
    ColorEntry(QColor(0xFF, 0xFF, 0xFF), true) ,
    ColorEntry(QColor(0x68, 0x68, 0x68), false),
    ColorEntry(QColor(0xFF, 0x54, 0x54), false),
    ColorEntry(QColor(0x54, 0xFF, 0x54), false),
    ColorEntry(QColor(0xFF, 0xFF, 0x54), false),
    ColorEntry(QColor(0x54, 0x54, 0xFF), false),
    ColorEntry(QColor(0xFF, 0x54, 0xFF), false),
    ColorEntry(QColor(0x54, 0xFF, 0xFF), false),
    ColorEntry(QColor(0xFF, 0xFF, 0xFF), false),
};

const char *const ColorScheme::colorNames[TABLE_COLORS] = {
    "Foreground",
    "Background",
    "Color0",
    "Color1",
    "Color2",
    "Color3",
    "Color4",
    "Color5",
    "Color6",
    "Color7",
    "ForegroundIntense",
    "BackgroundIntense",
    "Color0Intense",
    "Color1Intense",
    "Color2Intense",
    "Color3Intense",
    "Color4Intense",
    "Color5Intense",
    "Color6Intense",
    "Color7Intense"
};

#define tr_NOOP
const char *const ColorScheme::translatedColorNames[TABLE_COLORS] = {
    tr_NOOP("Foreground"),
    tr_NOOP("Background"),
    tr_NOOP("Color 1"),
    tr_NOOP("Color 2"),
    tr_NOOP("Color 3"),
    tr_NOOP("Color 4"),
    tr_NOOP("Color 5"),
    tr_NOOP("Color 6"),
    tr_NOOP("Color 7"),
    tr_NOOP("Color 8"),
    tr_NOOP("Foreground (Intense)"),
    tr_NOOP("Background (Intense)"),
    tr_NOOP("Color 1 (Intense)"),
    tr_NOOP("Color 2 (Intense)"),
    tr_NOOP("Color 3 (Intense)"),
    tr_NOOP("Color 4 (Intense)"),
    tr_NOOP("Color 5 (Intense)"),
    tr_NOOP("Color 6 (Intense)"),
    tr_NOOP("Color 7 (Intense)"),
    tr_NOOP("Color 8 (Intense)")
};

ColorScheme::ColorScheme() {
    _table = nullptr;
    _randomTable = nullptr;
    _opacity = 1.0;
}

ColorScheme::ColorScheme(const ColorScheme &other)
    : _opacity(other._opacity), _table(nullptr), _randomTable(nullptr) {
    setName(other.name());
    setDescription(other.description());

    if(other._table != nullptr) {
        for (int i = 0; i < TABLE_COLORS; i++)
        setColorTableEntry(i, other._table[i]);
    }

    if(other._randomTable != nullptr) {
        for(int i = 0; i < TABLE_COLORS; i++) {
            const RandomizationRange &range = other._randomTable[i];
            setRandomizationRange(i, range.hue, range.saturation, range.value);
        }
    }
}

ColorScheme::~ColorScheme() {
    delete[] _table;
    delete[] _randomTable;
}

void ColorScheme::setDescription(const QString &description) {
    _description = description;
}

QString ColorScheme::description() const { 
    return _description; 
}

void ColorScheme::setName(const QString &name) { 
    _name = name; 
}

QString ColorScheme::name() const { 
    return _name; 
}

void ColorScheme::setColorTableEntry(int index, const ColorEntry &entry) {
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    if (!_table) {
        _table = new ColorEntry[TABLE_COLORS];

        for (int i = 0; i < TABLE_COLORS; i++)
            _table[i] = defaultTable[i];
    }

    _table[index] = entry;
}

ColorEntry ColorScheme::colorEntry(int index) const {
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    ColorEntry entry = colorTable()[index];

    if (_randomTable != nullptr && !_randomTable[index].isNull()) {
        const RandomizationRange &range = _randomTable[index];

        int hueDifference = range.hue
                ? QRandomGenerator::global()->bounded(range.hue) - range.hue / 2
                : 0;
        int saturationDifference = range.saturation
                ? QRandomGenerator::global()->bounded(range.saturation) -
                    range.saturation / 2
                : 0;
        int valueDifference = range.value
                ? QRandomGenerator::global()->bounded(range.value) - range.value / 2
                : 0;

        QColor &color = entry.color;

        int newHue = qAbs((color.hue() + hueDifference) % MAX_HUE);
        int newValue = qMin(qAbs(color.value() + valueDifference), 255);
        int newSaturation = qMin(qAbs(color.saturation() + saturationDifference), 255);

        color.setHsv(newHue, newSaturation, newValue);
    }

    return entry;
}

void ColorScheme::getColorTable(ColorEntry *table) const {
    for (int i = 0; i < TABLE_COLORS; i++)
        table[i] = colorEntry(i);
}

bool ColorScheme::randomizedBackgroundColor() const {
    return _randomTable == nullptr ? false : !_randomTable[1].isNull();
}

void ColorScheme::setRandomizedBackgroundColor(bool randomize) {
    if (randomize) {
        setRandomizationRange(1 /* background color index */, MAX_HUE, 255, 0);
    } else {
        if (_randomTable)
            setRandomizationRange(1 /* background color index */, 0, 0, 0);
    }
}

void ColorScheme::setRandomizationRange(int index, quint16 hue,
                                        quint8 saturation, quint8 value) {
    Q_ASSERT(hue <= MAX_HUE);
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    if (_randomTable == nullptr)
        _randomTable = new RandomizationRange[TABLE_COLORS];

    _randomTable[index].hue = hue;
    _randomTable[index].value = value;
    _randomTable[index].saturation = saturation;
}

const ColorEntry *ColorScheme::colorTable() const {
    if (_table)
        return _table;
    else
        return defaultTable;
}

QColor ColorScheme::foregroundColor() const { 
    return colorTable()[0].color; 
}

QColor ColorScheme::backgroundColor() const { 
    return colorTable()[1].color; 
}

bool ColorScheme::hasDarkBackground() const {
    return backgroundColor().value() < 127;
}

void ColorScheme::setOpacity(qreal opacity) { 
    _opacity = opacity; 
}

qreal ColorScheme::opacity() const { 
    return _opacity; 
}

void ColorScheme::read(const QString &fileName) {
    QSettings s(fileName, QSettings::IniFormat);
    s.beginGroup(QLatin1String("General"));

    _description = s.value(QLatin1String("Description"),
                            QObject::tr("Un-named Color Scheme")).toString();
    _opacity = s.value(QLatin1String("Opacity"), qreal(1.0)).toDouble();
    s.endGroup();

    for (int i = 0; i < TABLE_COLORS; i++) {
        readColorEntry(&s, i);
    }
}

#if 0
void ColorScheme::read(KConfig& config) {
    KConfigGroup configGroup = config.group("General");

    QString description = configGroup.readEntry("Description", QObject::tr("Un-named Color Scheme"));

    _description = tr(description.toUtf8());
    _opacity = configGroup.readEntry("Opacity",qreal(1.0));

    for (int i=0 ; i < TABLE_COLORS ; i++) {
        readColorEntry(config,i);
    }
}

void ColorScheme::write(KConfig& config) const {
    KConfigGroup configGroup = config.group("General");

    configGroup.writeEntry("Description",_description);
    configGroup.writeEntry("Opacity",_opacity);

    for (int i=0 ; i < TABLE_COLORS ; i++) {
        RandomizationRange random = _randomTable != 0 ? _randomTable[i] : RandomizationRange();
        writeColorEntry(config,colorNameForIndex(i),colorTable()[i],random);
    }
}
#endif

QString ColorScheme::colorNameForIndex(int index) {
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    return QString::fromLatin1(colorNames[index]);
}

QString ColorScheme::translatedColorNameForIndex(int index) {
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    return QString::fromLatin1(translatedColorNames[index]);
}

void ColorScheme::readColorEntry(QSettings *s, int index) {
    QString colorName = colorNameForIndex(index);

    s->beginGroup(colorName);

    ColorEntry entry;

    QVariant colorValue = s->value(QLatin1String("Color"));
    QString colorStr;
    int r, g, b;
    bool ok = false;
    if (colorValue.typeId() == QMetaType::QStringList) {
        QStringList rgbList = colorValue.toStringList();
        colorStr = rgbList.join(QLatin1Char(','));
        if (rgbList.count() == 3) {
            bool parse_ok;

            ok = true;
            r = rgbList[0].toInt(&parse_ok);
            ok = ok && parse_ok && (r >= 0 && r <= 0xff);
            g = rgbList[1].toInt(&parse_ok);
            ok = ok && parse_ok && (g >= 0 && g <= 0xff);
            b = rgbList[2].toInt(&parse_ok);
            ok = ok && parse_ok && (b >= 0 && b <= 0xff);
        }
    } else {
        colorStr = colorValue.toString();
        QRegularExpression hexColorPattern(
            QLatin1String("^#[0-9a-f]{6}$"),
            QRegularExpression::CaseInsensitiveOption);
        if (hexColorPattern.match(colorStr).hasMatch()) {
            r = QStringView{colorStr}.mid(1, 2).toInt(nullptr, 16);
            g = QStringView{colorStr}.mid(3, 2).toInt(nullptr, 16);
            b = QStringView{colorStr}.mid(5, 2).toInt(nullptr, 16);
            ok = true;
        }
    }
    if (!ok) {
        r = g = b = 0;
    }
    entry.color = QColor(r, g, b);

    entry.transparent = s->value(QLatin1String("Transparent"), false).toBool();

    if (s->contains(QLatin1String("Bold")))
        entry.fontWeight = s->value(QLatin1String("Bold"), false).toBool()
                            ? ColorEntry::Bold
                            : ColorEntry::UseCurrentFormat;

    quint16 hue = s->value(QLatin1String("MaxRandomHue"), 0).toInt();
    quint8 value = s->value(QLatin1String("MaxRandomValue"), 0).toInt();
    quint8 saturation = s->value(QLatin1String("MaxRandomSaturation"), 0).toInt();

    setColorTableEntry(index, entry);

    if (hue != 0 || value != 0 || saturation != 0)
        setRandomizationRange(index, hue, saturation, value);

    s->endGroup();
}

#if 0
void ColorScheme::writeColorEntry(KConfig& config , const QString& colorName,
        const ColorEntry& entry , const RandomizationRange& random) const {
    KConfigGroup configGroup(&config,colorName);

    configGroup.writeEntry("Color",entry.color);
    configGroup.writeEntry("Transparency",(bool)entry.transparent);
    if (entry.fontWeight != ColorEntry::UseCurrentFormat) {
        configGroup.writeEntry("Bold",entry.fontWeight == ColorEntry::Bold);
    }

    if ( !random.isNull() || configGroup.hasKey("MaxRandomHue") ) {
        configGroup.writeEntry("MaxRandomHue",static_cast<int>(random.hue));
        configGroup.writeEntry("MaxRandomValue",static_cast<int>(random.value));
        configGroup.writeEntry("MaxRandomSaturation",static_cast<int>(random.saturation));
    }
}
#endif

AccessibleColorScheme::AccessibleColorScheme() : ColorScheme() {
#if 0
    setName("accessible");
    setDescription(QObject::tr("Accessible Color Scheme"));

    const int ColorRoleCount = 8;

    const KColorScheme colorScheme(QPalette::Active);

    QBrush colors[ColorRoleCount] = {
        colorScheme.foreground( colorScheme.NormalText ),
        colorScheme.background( colorScheme.NormalBackground ),

        colorScheme.foreground( colorScheme.InactiveText ),
        colorScheme.foreground( colorScheme.ActiveText ),
        colorScheme.foreground( colorScheme.LinkText ),
        colorScheme.foreground( colorScheme.VisitedText ),
        colorScheme.foreground( colorScheme.NegativeText ),
        colorScheme.foreground( colorScheme.NeutralText )
    };

    for ( int i = 0 ; i < TABLE_COLORS ; i++ ) {
        ColorEntry entry;
        entry.color = colors[ i % ColorRoleCount ].color();

        setColorTableEntry( i , entry );
    }
#endif
}

ColorSchemeManager::ColorSchemeManager() 
    : _haveLoadedAll(false) {
}

ColorSchemeManager::~ColorSchemeManager() {
    QHashIterator<QString, const ColorScheme *> iter(_colorSchemes);
    while (iter.hasNext()) {
        iter.next();
        delete iter.value();
    }
}

void ColorSchemeManager::loadAllColorSchemes() {
    QList<QString> nativeColorSchemes = listColorSchemes();
    QListIterator<QString> nativeIter(nativeColorSchemes);
    while (nativeIter.hasNext()) {
        loadColorScheme(nativeIter.next());
    }

    _haveLoadedAll = true;
}

QList<const ColorScheme *> ColorSchemeManager::allColorSchemes() {
    if (!_haveLoadedAll) {
        loadAllColorSchemes();
    }

    return _colorSchemes.values();
}

bool ColorSchemeManager::loadCustomColorScheme(const QString &path) {
    if (path.endsWith(QLatin1String(".colorscheme")))
        return loadColorScheme(path);

    return false;
}

bool ColorSchemeManager::loadColorScheme(const QString &filePath) {
    if (!filePath.endsWith(QLatin1String(".colorscheme")) ||
        !QFile::exists(filePath))
        return false;

    QFileInfo info(filePath);

    const QString &schemeName = info.baseName();

    ColorScheme *scheme = new ColorScheme();
    scheme->setName(schemeName);
    scheme->read(filePath);

    if (scheme->name().isEmpty()) {
        delete scheme;
        return false;
    }

    if (!_colorSchemes.contains(schemeName)) {
        _colorSchemes.insert(schemeName, scheme);
    } else {
        delete scheme;
    }

    return true;
}

const QStringList ColorSchemeManager::get_color_schemes_dirs() {
    QStringList list;
    list.append(":/konsole/color-schemes");
    return list;
}

QList<QString> ColorSchemeManager::listColorSchemes() {
    QList<QString> ret;
    for (const QString &scheme_dir : get_color_schemes_dirs()) {
        const QString dname(scheme_dir);
        QDir dir(dname);
        QStringList filters;
        filters << QLatin1String("*.colorscheme");
        dir.setNameFilters(filters);
        const QStringList list = dir.entryList(filters);
        for (const QString &i : list)
            ret << dname + QLatin1Char('/') + i;
    }
    return ret;
}

const ColorScheme ColorSchemeManager::_defaultColorScheme;
const ColorScheme *ColorSchemeManager::defaultColorScheme() const {
    return &_defaultColorScheme;
}

bool ColorSchemeManager::deleteColorScheme(const QString &name) {
    Q_ASSERT(_colorSchemes.contains(name));

    QString path = findColorSchemePath(name);
    if (QFile::remove(path)) {
        _colorSchemes.remove(name);
        return true;
    } else {
        return false;
    }
}

QString ColorSchemeManager::findColorSchemePath(const QString &name) const {
    QStringList dirs = get_color_schemes_dirs();
    if (dirs.isEmpty())
        return QString();

    const QString dir = dirs.first();
    QString path(dir + QLatin1Char('/') + name + QLatin1String(".colorscheme"));
    if (!path.isEmpty())
        return path;

    path = dir + QLatin1Char('/') + name + QLatin1String(".schema");

    return path;
}

const ColorScheme *ColorSchemeManager::findColorScheme(const QString &name) {
    if (name.isEmpty())
        return defaultColorScheme();

    if (_colorSchemes.contains(name)) {
        return _colorSchemes[name];
    } else {
        QString path = findColorSchemePath(name);
        if (!path.isEmpty() && loadColorScheme(path)) {
            return findColorScheme(name);
        }

        return nullptr;
    }
}

Q_GLOBAL_STATIC(ColorSchemeManager, theColorSchemeManager)
ColorSchemeManager *ColorSchemeManager::instance() {
    return theColorSchemeManager;
}
