#ifndef HISTORY_H
#define HISTORY_H

#include <QBitRef>
#include <QHash>
#include <QVector>
#include <QTemporaryFile>

#include "Character.h"

class HistoryType;
class HistoryScroll
{
public:
    HistoryScroll(HistoryType*);
    virtual ~HistoryScroll();

    virtual bool hasScroll();

    virtual int  getLines() = 0;
    virtual int  getLineLen(int lineno) = 0;
    virtual void getCells(int lineno, int colno, int count, Character res[]) = 0;
    virtual bool isWrappedLine(int lineno) = 0;

    Character   getCell(int lineno, int colno) { Character res; getCells(lineno,colno,1,&res); return res; }

    virtual void addCells(const Character a[], int count) = 0;
    virtual void addCellsVector(const QVector<Character>& cells) {
        addCells(cells.data(),cells.size());
    }

    virtual void addLine(bool previousWrapped=false) = 0;

    const HistoryType& getType() const { return *m_histType; }

protected:
    HistoryType* m_histType;
};

class HistoryScrollBuffer : public HistoryScroll
{
public:
    typedef QVector<Character> HistoryLine;

    HistoryScrollBuffer(unsigned int maxNbLines = 1000);
    ~HistoryScrollBuffer() override;

    int  getLines() override;
    int  getLineLen(int lineno) override;
    void getCells(int lineno, int colno, int count, Character res[]) override;
    bool isWrappedLine(int lineno) override;

    void addCells(const Character a[], int count) override;
    void addCellsVector(const QVector<Character>& cells) override;
    void addLine(bool previousWrapped=false) override;

    void setMaxNbLines(unsigned int nbLines);
    unsigned int maxNbLines() const { return _maxLineCount; }

private:
    int bufferIndex(int lineNumber) const;

    HistoryLine* _historyBuffer;
    QBitArray _wrappedLine;
    int _maxLineCount;
    int _usedLines;
    int _head;
};

class HistoryScrollNone : public HistoryScroll
{
public:
    HistoryScrollNone();
    ~HistoryScrollNone() override;

    bool hasScroll() override;

    int  getLines() override;
    int  getLineLen(int lineno) override;
    void getCells(int lineno, int colno, int count, Character res[]) override;
    bool isWrappedLine(int lineno) override;

    void addCells(const Character a[], int count) override;
    void addLine(bool previousWrapped=false) override;
};


typedef QVector<Character> TextLine;

class CharacterFormat
{
public:
    bool equalsFormat(const CharacterFormat &other) const {
        return other.rendition==rendition && other.fgColor==fgColor && other.bgColor==bgColor;
    }

    bool equalsFormat(const Character &c) const {
        return c.rendition==rendition && c.foregroundColor==fgColor && c.backgroundColor==bgColor;
    }

    void setFormat(const Character& c) {
        rendition=c.rendition;
        fgColor=c.foregroundColor;
        bgColor=c.backgroundColor;
    }

    CharacterColor fgColor, bgColor;
    quint16 startPos;
    quint8 rendition;
};

class HistoryType
{
public:
    HistoryType();
    virtual ~HistoryType();

    virtual bool isEnabled()           const = 0;

    bool isUnlimited() const { return maximumLineCount() == 0; }

    virtual int maximumLineCount()    const = 0;

    virtual HistoryScroll* scroll(HistoryScroll *) const = 0;
};

class HistoryTypeNone : public HistoryType
{
public:
    HistoryTypeNone();

    bool isEnabled() const override;
    int maximumLineCount() const override;

    HistoryScroll* scroll(HistoryScroll *) const override;
};

class HistoryTypeBuffer : public HistoryType
{
    friend class HistoryScrollBuffer;

public:
    HistoryTypeBuffer(unsigned int nbLines);

    bool isEnabled() const override;
    int maximumLineCount() const override;

    HistoryScroll* scroll(HistoryScroll *) const override;

protected:
  unsigned int m_nbLines;
};

#endif
