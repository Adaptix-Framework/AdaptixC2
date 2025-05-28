#include "TerminalCharacterDecoder.h"
#include "CharWidth.h"
#include <QTextStream>
#include <cwctype>

PlainTextDecoder::PlainTextDecoder()
    : _output(nullptr), _includeTrailingWhitespace(true),
    _recordLinePositions(false) {
}

void PlainTextDecoder::setTrailingWhitespace(bool enable) {
    _includeTrailingWhitespace = enable;
}

bool PlainTextDecoder::trailingWhitespace() const {
    return _includeTrailingWhitespace;
}

void PlainTextDecoder::begin(QTextStream *output) {
    _output = output;
    if (!_linePositions.isEmpty())
        _linePositions.clear();
}

void PlainTextDecoder::end() { 
    _output = nullptr; 
}

void PlainTextDecoder::setRecordLinePositions(bool record) {
    _recordLinePositions = record;
}

QList<int> PlainTextDecoder::linePositions() const { 
    return _linePositions; 
}

void PlainTextDecoder::decodeLine(const Character* const characters, int count, LineProperty /*properties*/) {
    Q_ASSERT(_output);

    if (_recordLinePositions && _output->string()) {
        int pos = _output->string()->size();
        _linePositions << pos;
    }

    if (characters == nullptr) {
        return;
    }

    std::wstring plainText;
    plainText.reserve(count);

    int outputCount = count;

    if (!_includeTrailingWhitespace) {
        for (int i = count - 1; i >= 0; i--) {
            if (!characters[i].isSpace())
                break;
            else
                outputCount--;
        }
    }

    for (int i = 0; i < outputCount;) {
        if (characters[i].rendition & RE_EXTENDED_CHAR) {
            ushort extendedCharLength = 0;
            const uint* chars = ExtendedCharTable::instance.lookupExtendedChar(characters[i].character, extendedCharLength);
            if (chars) {
                std::wstring str;
                for (ushort nchar = 0; nchar < extendedCharLength; nchar++) {
                    str.push_back(chars[nchar]);
                }
                plainText += str;
                i += qMax(1, CharWidth::string_unicode_width(str));
            } else {
                ++i;
            }
        } else {
            plainText.push_back(characters[i].character);
            i += qMax(1, CharWidth::unicode_width(characters[i].character));
        }
    }
    *_output << QString::fromStdWString(plainText);
}

HTMLDecoder::HTMLDecoder()
    : _output(nullptr)
    , _colorTable(base_color_table)
    , _innerSpanOpen(false)
    , _lastRendition(DEFAULT_RENDITION) {
}

void HTMLDecoder::begin(QTextStream *output) {
    _output = output;

    std::wstring text;

    // open monospace span
    openSpan(text, QLatin1String("font-family:monospace"));

    *output << QString::fromStdWString(text);
}

void HTMLDecoder::end() {
    Q_ASSERT(_output);

    std::wstring text;

    closeSpan(text);

    *_output << QString::fromStdWString(text);

    _output = nullptr;
}

// TODO: Support for LineProperty (mainly double width , double height)
void HTMLDecoder::decodeLine(const Character *const characters, int count, LineProperty /*properties*/) {
    Q_ASSERT(_output);

    std::wstring text;

    int spaceCount = 0;

    for (int i = 0; i < count; i++) {
        if (characters[i].rendition != _lastRendition ||
            characters[i].foregroundColor != _lastForeColor ||
            characters[i].backgroundColor != _lastBackColor) {
            if (_innerSpanOpen)
                closeSpan(text);

            _lastRendition = characters[i].rendition;
            _lastForeColor = characters[i].foregroundColor;
            _lastBackColor = characters[i].backgroundColor;

            QString style;

            bool useBold;
            ColorEntry::FontWeight weight = characters[i].fontWeight(_colorTable);
            if (weight == ColorEntry::UseCurrentFormat)
                useBold = _lastRendition & RE_BOLD;
            else
                useBold = weight == ColorEntry::Bold;

            if (useBold)
                style.append(QLatin1String("font-weight:bold;"));

            if (_lastRendition & RE_UNDERLINE)
                style.append(QLatin1String("font-decoration:underline;"));

            if (_colorTable) {
                style.append(QString::fromLatin1("color:%1;").arg(_lastForeColor.color(_colorTable).name()));

                if (!characters[i].isTransparent(_colorTable)) {
                    style.append(QString::fromLatin1("background-color:%1;").arg(_lastBackColor.color(_colorTable).name()));
                }
            }

            openSpan(text, style);
            _innerSpanOpen = true;
        }

        if (characters[i].isSpace())
            spaceCount++;
        else
            spaceCount = 0;

        if (spaceCount < 2) {
            if (characters[i].rendition & RE_EXTENDED_CHAR) {
                ushort extendedCharLength = 0;
                const uint* chars = ExtendedCharTable::instance.lookupExtendedChar(characters[i].character, extendedCharLength);
                if (chars) {
                    for (ushort nchar = 0; nchar < extendedCharLength; nchar++) {
                        text.push_back(chars[nchar]);
                    }
                }
            } else {
                wchar_t ch(characters[i].character);
                if ( ch == '<' ) {
                    text.append(L"&lt;");
                } else if (ch == '>') {
                    text.append(L"&gt;");
                } else if (ch == '&') {
                    text.append(L"&amp;");
                } else {
                    text.push_back(ch);
                }
            }
        } else {
            text.append(L"&#160;");
        }
    }

    if (_innerSpanOpen)
        closeSpan(text);

    text.append(L"<br>");

    *_output << QString::fromStdWString(text);
}

void HTMLDecoder::openSpan(std::wstring &text, const QString &style) {
    text.append(QString(QLatin1String("<span style=\"%1\">")).arg(style).toStdWString());
}

void HTMLDecoder::closeSpan(std::wstring &text) { 
    text.append(L"</span>"); 
}

void HTMLDecoder::setColorTable(const ColorEntry *table) {
    _colorTable = table;
}
