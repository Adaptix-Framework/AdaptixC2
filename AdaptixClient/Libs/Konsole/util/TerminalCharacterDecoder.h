#ifndef TERMINAL_CHARACTER_DECODER_H
#define TERMINAL_CHARACTER_DECODER_H

#include "Character.h"

#include <QList>

class QTextStream;

class TerminalCharacterDecoder
{
public:
    virtual ~TerminalCharacterDecoder() {}

    virtual void begin(QTextStream* output) = 0;
    virtual void end() = 0;

    virtual void decodeLine(const Character* const characters, int count, LineProperty properties) = 0;
};

class PlainTextDecoder : public TerminalCharacterDecoder
{
public:
    PlainTextDecoder();

    void setTrailingWhitespace(bool enable);
    bool trailingWhitespace() const;
    QList<int> linePositions() const;
    void setRecordLinePositions(bool record);

    void begin(QTextStream* output) override;
    void end() override;

    void decodeLine(const Character* const characters, int count, LineProperty properties) override;


private:
    QTextStream* _output;
    bool _includeTrailingWhitespace;

    bool _recordLinePositions;
    QList<int> _linePositions;
};

class HTMLDecoder : public TerminalCharacterDecoder
{
public:

    HTMLDecoder();

    void setColorTable( const ColorEntry* table );

    void decodeLine(const Character* const characters, int count, LineProperty properties) override;

    void begin(QTextStream* output) override;
    void end() override;

private:
    void openSpan(std::wstring& text , const QString& style);
    void closeSpan(std::wstring& text);

    QTextStream* _output;
    const ColorEntry* _colorTable;
    bool _innerSpanOpen;
    quint8 _lastRendition;
    CharacterColor _lastForeColor;
    CharacterColor _lastBackColor;
};

#endif
