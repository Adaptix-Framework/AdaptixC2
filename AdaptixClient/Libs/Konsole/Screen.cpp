#include "Screen.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <QDate>
#include <QTextStream>

#include "util/CharWidth.h"
#include "util/TerminalCharacterDecoder.h"

#ifndef loc
#define loc(X, Y) ((Y) * columns + (X))
#endif

Character Screen::defaultChar = Character(
        ' ', CharacterColor(COLOR_SPACE_DEFAULT, DEFAULT_FORE_COLOR),
        CharacterColor(COLOR_SPACE_DEFAULT, DEFAULT_BACK_COLOR), DEFAULT_RENDITION);

Screen::Screen(int l, int c)
        : lines(l), columns(c), screenLines(new ImageLine[lines + 1]),
            _scrolledLines(0), _droppedLines(0), history(new HistoryScrollNone()),
            cuX(0), cuY(0), currentRendition(0), _topMargin(0), _bottomMargin(0),
            selBegin(0), selTopLeft(0), selBottomRight(0), blockSelectionMode(false),
            effectiveForeground(CharacterColor()),
            effectiveBackground(CharacterColor()), effectiveRendition(0),
            lastPos(-1) {
    lineProperties.resize(lines + 1);
    for (int i = 0; i < lines + 1; i++)
            lineProperties[i] = LINE_DEFAULT;

    initTabStops();
    clearSelection();
    reset();
}

Screen::~Screen() {
    delete[] screenLines;
    delete history;
}

void Screen::cursorUp(int n) {
    if (n == 0)
        n = 1;
    int stop = cuY < _topMargin ? 0 : _topMargin;
    cuX = qMin(columns - 1, cuX); /* nowrap! */
    cuY = qMax(stop, cuY - n);
}

void Screen::cursorDown(int n) {
    if (n == 0)
        n = 1;
    int stop = cuY > _bottomMargin ? lines - 1 : _bottomMargin;
    cuX = qMin(columns - 1, cuX); /* nowrap! */
    cuY = qMin(stop, cuY + n);
}

void Screen::cursorLeft(int n) {
    if (n == 0)
        n = 1;                      /* Default */
    cuX = qMin(columns - 1, cuX); /* nowrap! */
    cuX = qMax(0, cuX - n);
}

void Screen::cursorNextLine(int n) {
    if (n == 0) {
        n = 1; /* Default */
    }
    cuX = 0;
    while (n > 0) {
        if (cuY < lines - 1) {
            cuY += 1;
        }
        n--;
    }
}

void Screen::cursorPreviousLine(int n) {
    if (n == 0) {
        n = 1; /* Default */
    }
    cuX = 0;
    while (n > 0) {
        if (cuY > 0) {
            cuY -= 1;
        }
        n--;
    }
}

void Screen::cursorRight(int n) {
    if (n == 0)
        n = 1; /* Default */
    cuX = qMin(columns - 1, cuX + n);
}

void Screen::setMargins(int top, int bot) {
    if (top == 0)
        top = 1; /* Default */
    if (bot == 0)
        bot = lines; /* Default */
    top = top - 1;
    bot = bot - 1;
    if (!(0 <= top && top < bot && bot < lines)) {
        return; /* Default */
    }
    _topMargin = top;
    _bottomMargin = bot;
    cuX = 0;
    cuY = getMode(MODE_Origin) ? top : 0;
}

int Screen::topMargin() const { return _topMargin; }
int Screen::bottomMargin() const { return _bottomMargin; }

void Screen::index() {
    if (cuY == _bottomMargin)
        scrollUp(1);
    else if (cuY < lines - 1)
        cuY += 1;
}

void Screen::reverseIndex() {
    if (cuY == _topMargin)
        scrollDown(_topMargin, 1);
    else if (cuY > 0)
        cuY -= 1;
}

void Screen::nextLine() {
    toStartOfLine();
    index();
}

void Screen::eraseChars(int n) {
    if (n == 0)
        n = 1;
    int p = qMax(0, qMin(cuX + n - 1, columns - 1));
    clearImage(loc(cuX, cuY), loc(p, cuY), ' ');
}

void Screen::deleteChars(int n) {
    Q_ASSERT(n >= 0);

    if (n == 0)
        n = 1;

    if (cuX >= screenLines[cuY].count())
        return;

    if (cuX + n > screenLines[cuY].count())
        n = screenLines[cuY].count() - cuX;

    Q_ASSERT(n >= 0);
    Q_ASSERT(cuX + n <= screenLines[cuY].count());

    screenLines[cuY].remove(cuX, n);
}

void Screen::insertChars(int n) {
    if (n == 0)
        n = 1;

    if (screenLines[cuY].size() < cuX)
        screenLines[cuY].resize(cuX);

    screenLines[cuY].insert(cuX, n, ' ');

    if (screenLines[cuY].count() > columns)
        screenLines[cuY].resize(columns);
}

void Screen::repeatChars(int count) {
    if (count == 0) {
        count = 1;
    }
    /**
     * From ECMA-48 version 5, section 8.3.103
     * If the character preceding REP is a control function or part of a
     * control function, the effect of REP is not defined by this Standard.
     *
     * So, a "normal" program should always use REP immediately after a visible
     * character (those other than escape sequences). So, lastDrawnChar can be
     * safely used.
     */
    for (int i = 0; i < count; i++) {
        displayCharacter(lastDrawnChar);
    }
}

void Screen::deleteLines(int n) {
    if (n == 0)
        n = 1;
    scrollUp(cuY, n);
}

void Screen::insertLines(int n) {
    if (n == 0)
        n = 1;
    scrollDown(cuY, n);
}

void Screen::setMode(int m) {
    currentModes[m] = true;
    switch (m) {
    case MODE_Origin:
        cuX = 0;
        cuY = _topMargin;
        break;
    }
}

void Screen::resetMode(int m) {
    currentModes[m] = false;
    switch (m) {
    case MODE_Origin:
        cuX = 0;
        cuY = 0;
        break;
    }
}

void Screen::saveMode(int m) { savedModes[m] = currentModes[m]; }

void Screen::restoreMode(int m) { currentModes[m] = savedModes[m]; }

bool Screen::getMode(int m) const { return currentModes[m]; }

void Screen::saveCursor() {
    savedState.cursorColumn = cuX;
    savedState.cursorLine = cuY;
    savedState.rendition = currentRendition;
    savedState.foreground = currentForeground;
    savedState.background = currentBackground;
}

void Screen::restoreCursor() {
    cuX = qMin(savedState.cursorColumn, columns - 1);
    cuY = qMin(savedState.cursorLine, lines - 1);
    currentRendition = savedState.rendition;
    currentForeground = savedState.foreground;
    currentBackground = savedState.background;
    updateEffectiveRendition();
}

void Screen::resizeImage(int new_lines, int new_columns) {
    if ((new_lines == lines) && (new_columns == columns))
        return;

    if (cuY > new_lines - 1) {
        _bottomMargin = lines - 1;
        for (int i = 0; i < cuY - (new_lines - 1); i++) {
            addHistLine();
            scrollUp(0, 1);
        }
    }

    ImageLine *newScreenLines = new ImageLine[new_lines + 1];
    for (int i = 0; i < qMin(lines, new_lines + 1); i++)
        newScreenLines[i] = screenLines[i];
    for (int i = lines; (i > 0) && (i < new_lines + 1); i++)
        newScreenLines[i].resize(new_columns);

    lineProperties.resize(new_lines + 1);
    for (int i = lines; (i > 0) && (i < new_lines + 1); i++)
        lineProperties[i] = LINE_DEFAULT;

    clearSelection();

    delete[] screenLines;
    screenLines = newScreenLines;

    lines = new_lines;
    columns = new_columns;
    cuX = qMin(cuX, columns - 1);
    cuY = qMin(cuY, lines - 1);

    _topMargin = 0;
    _bottomMargin = lines - 1;
    initTabStops();
    clearSelection();
}

void Screen::setDefaultMargins() {
    _topMargin = 0;
    _bottomMargin = lines - 1;
}

/*
 Clarifying rendition here and in the display.
 
 currently, the display's color table is
 0       1       2 .. 9    10 .. 17
 dft_fg, dft_bg, dim 0..7, intensive 0..7
 
 currentForeground, currentBackground contain values 0..8;
 - 0    = default color
 - 1..8 = ansi specified color
 
 re_fg, re_bg contain values 0..17
 due to the TerminalDisplay's color table
 
 rendition attributes are
 
 attr           widget screen
 -------------- ------ ------
 RE_UNDERLINE     XX     XX    affects foreground only
 RE_BLINK         XX     XX    affects foreground only
 RE_BOLD          XX     XX    affects foreground only
 RE_REVERSE       --     XX
 RE_TRANSPARENT   XX     --    affects background only
 RE_INTENSIVE     XX     --    affects foreground only
 
 Note that RE_BOLD is used in both widget
 and screen rendition. Since xterm/vt102
 is to poor to distinguish between bold
 (which is a font attribute) and intensive
 (which is a color attribute), we translate
 this and RE_BOLD in falls eventually apart
 into RE_BOLD and RE_INTENSIVE.
*/
void Screen::reverseRendition(Character &p) const {
    CharacterColor f = p.foregroundColor;
    CharacterColor b = p.backgroundColor;

    p.foregroundColor = b;
    p.backgroundColor = f;
}

void Screen::updateEffectiveRendition() {
    effectiveRendition = currentRendition;
    if (currentRendition & RE_REVERSE) {
        effectiveForeground = currentBackground;
        effectiveBackground = currentForeground;
    } else {
        effectiveForeground = currentForeground;
        effectiveBackground = currentBackground;
    }

    if (currentRendition & RE_BOLD)
        effectiveForeground.setIntensive();
}

void Screen::copyFromHistory(Character *dest, int startLine, int count) const {
    Q_ASSERT(startLine >= 0 && count > 0 &&
                     startLine + count <= history->getLines());

    for (int line = startLine; line < startLine + count; line++) {
        const int length = qMin(columns, history->getLineLen(line));
        const int destLineOffset = (line - startLine) * columns;

        history->getCells(line, 0, length, dest + destLineOffset);

        for (int column = length; column < columns; column++)
            dest[destLineOffset + column] = defaultChar;

        if (selBegin != -1) {
            for (int column = 0; column < columns; column++) {
                if (isSelected(column, line)) {
                    reverseRendition(dest[destLineOffset + column]);
                }
            }
        }
    }
}

void Screen::copyFromScreen(Character *dest, int startLine, int count) const {
    Q_ASSERT(startLine >= 0 && count > 0 && startLine + count <= lines);

    for (int line = startLine; line < (startLine + count); line++) {
        int srcLineStartIndex = line * columns;
        int destLineStartIndex = (line - startLine) * columns;

        for (int column = 0; column < columns; column++) {
            int srcIndex = srcLineStartIndex + column;
            int destIndex = destLineStartIndex + column;

            dest[destIndex] = screenLines[srcIndex / columns].value(
                    srcIndex % columns, defaultChar);

            if (selBegin != -1 && isSelected(column, line + history->getLines()))
                reverseRendition(dest[destIndex]);
        }
    }
}

void Screen::getImage(Character *dest, int size, int startLine,
                                            int endLine) const {
    Q_ASSERT(startLine >= 0);
    Q_ASSERT(endLine >= startLine && endLine < history->getLines() + lines);

    const int mergedLines = endLine - startLine + 1;

    Q_ASSERT(size >= mergedLines * columns);
    Q_UNUSED(size);

    const int linesInHistoryBuffer =
            qBound(0, history->getLines() - startLine, mergedLines);
    const int linesInScreenBuffer = mergedLines - linesInHistoryBuffer;

    if (linesInHistoryBuffer > 0)
        copyFromHistory(dest, startLine, linesInHistoryBuffer);

    if (linesInScreenBuffer > 0)
        copyFromScreen(dest + linesInHistoryBuffer * columns, startLine + linesInHistoryBuffer - history->getLines(), linesInScreenBuffer);

    if (getMode(MODE_Screen)) {
        for (int i = 0; i < mergedLines * columns; i++)
            reverseRendition(dest[i]);
    }

    int cursorIndex = loc(cuX, cuY + linesInHistoryBuffer);
    if (getMode(MODE_Cursor) && cursorIndex < columns * mergedLines)
        dest[cursorIndex].rendition |= RE_CURSOR;
}

QVector<LineProperty> Screen::getLineProperties(int startLine,
                                                                                                int endLine) const {
    Q_ASSERT(startLine >= 0);
    Q_ASSERT(endLine >= startLine && endLine < history->getLines() + lines);

    const int mergedLines = endLine - startLine + 1;
    const int linesInHistory =
            qBound(0, history->getLines() - startLine, mergedLines);
    const int linesInScreen = mergedLines - linesInHistory;

    QVector<LineProperty> result(mergedLines);
    int index = 0;

    for (int line = startLine; line < startLine + linesInHistory; line++) {
        if (history->isWrappedLine(line)) {
            result[index] = (LineProperty)(result[index] | LINE_WRAPPED);
        }
        index++;
    }

    const int firstScreenLine = startLine + linesInHistory - history->getLines();
    for (int line = firstScreenLine; line < firstScreenLine + linesInScreen;
             line++) {
        result[index] = lineProperties[line];
        index++;
    }

    return result;
}

void Screen::reset(bool clearScreen) {
    setMode(MODE_Wrap);
    saveMode(MODE_Wrap);
    resetMode(MODE_Origin);
    saveMode(MODE_Origin);
    resetMode(MODE_Insert);
    saveMode(MODE_Insert);
    setMode(MODE_Cursor);
    resetMode(MODE_Screen);
    resetMode(MODE_NewLine);

    _topMargin = 0;
    _bottomMargin = lines - 1;

    setDefaultRendition();
    saveCursor();

    if (clearScreen)
        clear();
}

void Screen::clear() {
    clearEntireScreen();
    home();
}

void Screen::backspace() {
    cuX = qMin(columns - 1, cuX);
    cuX = qMax(0, cuX - 1);

    if (screenLines[cuY].size() < cuX + 1)
        screenLines[cuY].resize(cuX + 1);
#if 0
    wchar_t c = 0;
    if(cuX <= 0) {
        if(cuY > 0) {
            uint32_t endx = screenLines[cuY-1].size();
            if(endx > 0) {
                    c = screenLines[cuY-1][endx-1].character;
            }
        }
    } else {
        c = screenLines[cuY][cuX-1].character;
    }
    if(c) {
        int ow = CharWidth::unicode_width(c,false);
        int w = CharWidth::unicode_width(c,true);
        if(w == 2 && ow == 1) {
            cuX = qMin(columns-1,cuX);
            cuX = qMax(0,cuX-1);

            if (screenLines[cuY].size() < cuX+1)
                    screenLines[cuY].resize(cuX+1);
        }
    }
#endif
}

void Screen::tab(int n) {
    if (n == 0)
        n = 1;
    while ((n > 0) && (cuX < columns - 1)) {
        cursorRight(1);
        while ((cuX < columns - 1) && !tabStops[cuX])
            cursorRight(1);
        n--;
    }
}

void Screen::backtab(int n) {
    if (n == 0)
        n = 1;
    while ((n > 0) && (cuX > 0)) {
        cursorLeft(1);
        while ((cuX > 0) && !tabStops[cuX])
            cursorLeft(1);
        n--;
    }
}

void Screen::clearTabStops() {
    for (int i = 0; i < columns; i++)
        tabStops[i] = false;
}

void Screen::changeTabStop(bool set) {
    if (cuX >= columns)
        return;
    tabStops[cuX] = set;
}

void Screen::initTabStops() {
    tabStops.resize(columns);

    for (int i = 0; i < columns; i++)
        tabStops[i] = (i % 8 == 0 && i != 0);
}

void Screen::newLine() {
    if (getMode(MODE_NewLine))
        toStartOfLine();
    index();
}

void Screen::checkSelection(int from, int to) {
    if (selBegin == -1)
        return;
    int scr_TL = loc(0, history->getLines());
    if ((selBottomRight >= (from + scr_TL)) && (selTopLeft <= (to + scr_TL)))
        clearSelection();
}

void Screen::displayCharacter(wchar_t c) {
    int w = CharWidth::unicode_width(c);
    if (w < 0)
        return;

    if (w == 0) {
        if (QChar(c).category() != QChar::Mark_NonSpacing)
            return;
        int charToCombineWithX = qMin(cuX, screenLines[cuY].length());
        int charToCombineWithY = cuY;
        bool previousChar = true;
        do {
            if (charToCombineWithX > 0) {
                --charToCombineWithX;
            } else if (charToCombineWithY > 0 && lineProperties.at(charToCombineWithY - 1) & LINE_WRAPPED) { 
                --charToCombineWithY;
                charToCombineWithX = screenLines[charToCombineWithY].length() - 1;
            } else {
                previousChar = false;
                break;
            }

            if (charToCombineWithX < 0) {
                previousChar = false;
                break;
            }
        } while (screenLines[charToCombineWithY][charToCombineWithX] == 0);

        if (!previousChar) {
            w = 2;
            goto notcombine;
        }

        Character& currentChar = screenLines[charToCombineWithY][charToCombineWithX];
        if ((currentChar.rendition & RE_EXTENDED_CHAR) == 0) {
            uint chars[2] = { static_cast<uint>(currentChar.character), static_cast<uint>(c) };
            currentChar.rendition |= RE_EXTENDED_CHAR;
            currentChar.character = ExtendedCharTable::instance.createExtendedChar(chars, 2);
        } else {
            ushort extendedCharLength;
            const uint* oldChars = ExtendedCharTable::instance.lookupExtendedChar(currentChar.character, extendedCharLength);
            Q_ASSERT(oldChars);
            if (oldChars && extendedCharLength < 8) {
                Q_ASSERT(extendedCharLength > 1);
                Q_ASSERT(extendedCharLength < 65535);
                auto chars = std::make_unique<uint[]>(extendedCharLength + 1);
                std::copy_n(oldChars, extendedCharLength, chars.get());
                chars[extendedCharLength] = c;
                currentChar.character = ExtendedCharTable::instance.createExtendedChar(chars.get(), extendedCharLength + 1);
            }
        }
        return;
    }

notcombine:
    if (cuX + w > columns) {
        if (getMode(MODE_Wrap)) {
            lineProperties[cuY] = (LineProperty)(lineProperties[cuY] | LINE_WRAPPED);
            nextLine();
        } else {
            cuX = columns - w;
        }
    }

    int size = screenLines[cuY].size();
    if (size < cuX + w) {
        screenLines[cuY].resize(cuX + w);
    }

    if (getMode(MODE_Insert))
        insertChars(w);

    lastPos = loc(cuX, cuY);

    checkSelection(lastPos, lastPos);

    Character &currentChar = screenLines[cuY][cuX];

    currentChar.character = c;
    currentChar.foregroundColor = effectiveForeground;
    currentChar.backgroundColor = effectiveBackground;
    currentChar.rendition = effectiveRendition;

    lastDrawnChar = c;

    int i = 0;
    int newCursorX = cuX + w--;
    while (w) {
        i++;

        if (screenLines[cuY].size() < cuX + i + 1)
            screenLines[cuY].resize(cuX + i + 1);

        Character &ch = screenLines[cuY][cuX + i];
        ch.character = 0;
        ch.foregroundColor = effectiveForeground;
        ch.backgroundColor = effectiveBackground;
        ch.rendition = effectiveRendition;

        w--;
    }
    cuX = newCursorX;
}

void Screen::compose(const QString & /*compose*/) {
    Q_ASSERT(0 /*Not implemented yet*/);

    /*  if (lastPos == -1)
            return;

            QChar c(image[lastPos].character);
            compose.prepend(c);
    compose.compose(); ### FIXME!
    image[lastPos].character = compose[0].unicode();*/
}

int Screen::scrolledLines() const { return _scrolledLines; }
int Screen::droppedLines() const { return _droppedLines; }
void Screen::resetDroppedLines() { _droppedLines = 0; }
void Screen::resetScrolledLines() { _scrolledLines = 0; }

void Screen::scrollUp(int n) {
    if (n == 0)
        n = 1;
    if (_topMargin == 0)
        addHistLine();
    scrollUp(_topMargin, n);
}

QRect Screen::lastScrolledRegion() const { return _lastScrolledRegion; }

void Screen::scrollUp(int from, int n) {
    if (n <= 0)
        return;
    if (from > _bottomMargin)
        return;
    if (from + n > _bottomMargin)
        n = _bottomMargin + 1 - from;

    _scrolledLines -= n;
    _lastScrolledRegion =
            QRect(0, _topMargin, columns - 1, (_bottomMargin - _topMargin));

    moveImage(loc(0, from), loc(0, from + n), loc(columns, _bottomMargin));
    clearImage(loc(0, _bottomMargin - n + 1), loc(columns - 1, _bottomMargin),
                         ' ');
}

void Screen::scrollDown(int n) {
    if (n == 0)
        n = 1;
    scrollDown(_topMargin, n);
}

void Screen::scrollDown(int from, int n) {
    _scrolledLines += n;

    if (n <= 0)
        return;
    if (from > _bottomMargin)
        return;
    if (from + n > _bottomMargin)
        n = _bottomMargin - from;
    moveImage(loc(0, from + n), loc(0, from),
                        loc(columns - 1, _bottomMargin - n));
    clearImage(loc(0, from), loc(columns - 1, from + n - 1), ' ');
}

void Screen::setCursorYX(int y, int x) {
    setCursorY(y);
    setCursorX(x);
}

void Screen::setCursorX(int x) {
    if (x == 0)
        x = 1;
    x -= 1;
    cuX = qMax(0, qMin(columns - 1, x));
}

void Screen::setCursorY(int y) {
    if (y == 0)
        y = 1;
    y -= 1;
    cuY = qMax(0, qMin(lines - 1, y + (getMode(MODE_Origin) ? _topMargin : 0)));
}

void Screen::home() {
    cuX = 0;
    cuY = 0;
}

void Screen::toStartOfLine() { cuX = 0; }

int Screen::getCursorX() const { return cuX; }

int Screen::getCursorY() const { return cuY; }

QString Screen::getScreenText(int row1, int col1, int row2, int col2, int mode) {
    Q_ASSERT(row1 >= 0 && row1 < lines);
    Q_ASSERT(row2 >= 0 && row2 < lines);
    Q_ASSERT(col1 >= 0 && col1 < columns);
    Q_ASSERT(col2 >= 0 && col2 < columns);

    QString text;
    int startLine = qMin(row1, row2);
    int endLine = qMax(row1, row2);
    int startCol = qMin(col1, col2);
    int endCol = qMax(col1, col2);

    if (mode == 1) {
        for (int i = startLine; i <= endLine; i++) {
            if (screenLines->size() <= i)
                break;
            for (int j = startCol; j <= endCol; j++) {
                if (screenLines[i].count() <= j)
                    break;
                wchar_t c = screenLines[i][j].character;
                text += QChar(c);
            }
        }
    } else if (mode == 2) {
        for (int i = startLine; i <= endLine; i++) {
            if (screenLines->size() <= i)
                break;
            int size = 0;
            for (int j = startCol; j <= endCol; j++) {
                if (screenLines[i].count() <= j)
                    break;
                wchar_t c = screenLines[i][j].character;
                text += QChar(c);
                size++;
            }
            if (size != 0) {
                text += '\n';
            }
        }
    }

    return text;
}

void Screen::clearImage(int loca, int loce, char c) {
    int scr_TL = loc(0, history->getLines());

    if ((selBottomRight > (loca + scr_TL)) && (selTopLeft < (loce + scr_TL))) {
        clearSelection();
    }

    int topLine = loca / columns;
    int bottomLine = loce / columns;

    Character clearCh(c, currentForeground, currentBackground, DEFAULT_RENDITION);

    bool isDefaultCh = (clearCh == Character());

    for (int y = topLine; y <= bottomLine; y++) {
        lineProperties[y] = 0;

        int endCol = (y == bottomLine) ? loce % columns : columns - 1;
        int startCol = (y == topLine) ? loca % columns : 0;

        QVector<Character> &line = screenLines[y];

        if (isDefaultCh && endCol == columns - 1) {
            line.resize(startCol);
        } else {
            if (line.size() < endCol + 1)
                line.resize(endCol + 1);

            Character *data = line.data();
            for (int i = startCol; i <= endCol; i++)
                data[i] = clearCh;
        }
    }
}

void Screen::moveImage(int dest, int sourceBegin, int sourceEnd) {
    Q_ASSERT(sourceBegin <= sourceEnd);

    int lines = (sourceEnd - sourceBegin) / columns;

    if (dest < sourceBegin) {
        for (int i = 0; i <= lines; i++) {
            screenLines[(dest / columns) + i] =
                    screenLines[(sourceBegin / columns) + i];
            lineProperties[(dest / columns) + i] =
                    lineProperties[(sourceBegin / columns) + i];
        }
    } else {
        for (int i = lines; i >= 0; i--) {
            screenLines[(dest / columns) + i] =
                    screenLines[(sourceBegin / columns) + i];
            lineProperties[(dest / columns) + i] =
                    lineProperties[(sourceBegin / columns) + i];
        }
    }

    if (lastPos != -1) {
        int diff = dest - sourceBegin;
        lastPos += diff;
        if ((lastPos < 0) || (lastPos >= (lines * columns)))
            lastPos = -1;
    }

    if (selBegin != -1) {
        bool beginIsTL = (selBegin == selTopLeft);
        int diff = dest - sourceBegin;
        int scr_TL = loc(0, history->getLines());
        int srca = sourceBegin + scr_TL;
        int srce = sourceEnd + scr_TL;
        int desta = srca + diff;
        int deste = srce + diff;

        if ((selTopLeft >= srca) && (selTopLeft <= srce))
            selTopLeft += diff;
        else if ((selTopLeft >= desta) && (selTopLeft <= deste))
            selBottomRight = -1;

        if ((selBottomRight >= srca) && (selBottomRight <= srce))
            selBottomRight += diff;
        else if ((selBottomRight >= desta) && (selBottomRight <= deste))
            selBottomRight = -1;

        if (selBottomRight < 0) {
            clearSelection();
        } else {
            if (selTopLeft < 0)
                selTopLeft = 0;
        }

        if (beginIsTL)
            selBegin = selTopLeft;
        else
            selBegin = selBottomRight;
    }
}

void Screen::clearToEndOfScreen() {
    clearImage(loc(cuX, cuY), loc(columns - 1, lines - 1), ' ');
}

void Screen::clearToBeginOfScreen() {
    clearImage(loc(0, 0), loc(cuX, cuY), ' ');
}

void Screen::clearEntireScreen() {
    for (int i = 0; i < (lines - 1); i++) {
        addHistLine();
        scrollUp(0, 1);
    }

    clearImage(loc(0, 0), loc(columns - 1, lines - 1), ' ');
}

void Screen::helpAlign() {
    clearImage(loc(0, 0), loc(columns - 1, lines - 1), 'E');
}

void Screen::clearToEndOfLine() {
    clearImage(loc(cuX, cuY), loc(columns - 1, cuY), ' ');
}

void Screen::clearToBeginOfLine() {
    clearImage(loc(0, cuY), loc(cuX, cuY), ' ');
}

void Screen::clearEntireLine() {
    clearImage(loc(0, cuY), loc(columns - 1, cuY), ' ');
}

void Screen::setRendition(int re) {
    currentRendition |= re;
    updateEffectiveRendition();
}

void Screen::resetRendition(int re) {
    currentRendition &= ~re;
    updateEffectiveRendition();
}

void Screen::setDefaultRendition() {
    setForeColor(COLOR_SPACE_DEFAULT, DEFAULT_FORE_COLOR);
    setBackColor(COLOR_SPACE_DEFAULT, DEFAULT_BACK_COLOR);
    currentRendition = DEFAULT_RENDITION;
    updateEffectiveRendition();
}

void Screen::setForeColor(int space, int color) {
    currentForeground = CharacterColor(space, color);

    if (currentForeground.isValid())
        updateEffectiveRendition();
    else
        setForeColor(COLOR_SPACE_DEFAULT, DEFAULT_FORE_COLOR);
}

void Screen::setBackColor(int space, int color) {
    currentBackground = CharacterColor(space, color);

    if (currentBackground.isValid())
        updateEffectiveRendition();
    else
        setBackColor(COLOR_SPACE_DEFAULT, DEFAULT_BACK_COLOR);
}

void Screen::clearSelection() {
    selBottomRight = -1;
    selTopLeft = -1;
    selBegin = -1;
}

bool Screen::isClearSelection() {
    return selBottomRight == -1 && selTopLeft == -1 && selBegin == -1;
}

void Screen::getSelectionStart(int &column, int &line) const {
    if (selTopLeft != -1) {
        column = selTopLeft % columns;
        line = selTopLeft / columns;
    } else {
        column = cuX + getHistLines();
        line = cuY + getHistLines();
    }
}
void Screen::getSelectionEnd(int &column, int &line) const {
    if (selBottomRight != -1) {
        column = selBottomRight % columns;
        line = selBottomRight / columns;
    } else {
        column = cuX + getHistLines();
        line = cuY + getHistLines();
    }
}
void Screen::setSelectionStart(const int x, const int y, const bool mode) {
    selBegin = loc(x, y);
    /* FIXME, HACK to correct for x too far to the right... */
    if (x == columns)
        selBegin--;

    selBottomRight = selBegin;
    selTopLeft = selBegin;
    blockSelectionMode = mode;
}

void Screen::setSelectionEnd(const int x, const int y) {
    if (selBegin == -1)
        return;

    int endPos = loc(x, y);

    if (endPos < selBegin) {
        selTopLeft = endPos;
        selBottomRight = selBegin;
    } else {
        /* FIXME, HACK to correct for x too far to the right... */
        if (x == columns)
            endPos--;

        selTopLeft = selBegin;
        selBottomRight = endPos;
    }

    if (blockSelectionMode) {
        int topRow = selTopLeft / columns;
        int topColumn = selTopLeft % columns;
        int bottomRow = selBottomRight / columns;
        int bottomColumn = selBottomRight % columns;

        selTopLeft = loc(qMin(topColumn, bottomColumn), topRow);
        selBottomRight = loc(qMax(topColumn, bottomColumn), bottomRow);
    }
}

bool Screen::isSelected(const int x, const int y) const {
    bool columnInSelection = true;
    if (blockSelectionMode) {
        columnInSelection =
                x >= (selTopLeft % columns) && x <= (selBottomRight % columns);
    }

    int pos = loc(x, y);
    return pos >= selTopLeft && pos <= selBottomRight && columnInSelection;
}

QString Screen::selectedText(bool preserveLineBreaks) const {
    QString result;
    QTextStream stream(&result, QIODevice::ReadWrite);

    PlainTextDecoder decoder;
    decoder.begin(&stream);
    writeSelectionToStream(&decoder, preserveLineBreaks);
    decoder.end();

    return result;
}

bool Screen::isSelectionValid() const {
    return selTopLeft >= 0 && selBottomRight >= 0;
}

void Screen::writeSelectionToStream(TerminalCharacterDecoder *decoder,
                                                                        bool preserveLineBreaks) const {
    if (!isSelectionValid())
        return;
    writeToStream(decoder, selTopLeft, selBottomRight, preserveLineBreaks);
}

void Screen::writeToStream(TerminalCharacterDecoder *decoder, int startIndex,
                            int endIndex, bool preserveLineBreaks) const {
    int top = startIndex / columns;
    int left = startIndex % columns;

    int bottom = endIndex / columns;
    int right = endIndex % columns;

    Q_ASSERT(top >= 0 && left >= 0 && bottom >= 0 && right >= 0);

    for (int y = top; y <= bottom; y++) {
        int start = 0;
        if (y == top || blockSelectionMode)
            start = left;

        int count = -1;
        if (y == bottom || blockSelectionMode)
            count = right - start + 1;

        const bool appendNewLine = (y != bottom);
        int copied = copyLineToStream(y, start, count, decoder, appendNewLine,
                                                                    preserveLineBreaks);
        if (y == bottom && copied < count) {
            Character newLineChar('\n');
            decoder->decodeLine(&newLineChar, 1, 0);
        }
    }
}

int Screen::copyLineToStream(int line, int start, int count, TerminalCharacterDecoder *decoder, bool appendNewLine, bool preserveLineBreaks) const {
    static const int MAX_CHARS = 1024;
    static Character characterBuffer[MAX_CHARS];

    Q_ASSERT(count < MAX_CHARS);

    LineProperty currentLineProperties = 0;

    if (line < history->getLines()) {
        const int lineLength = history->getLineLen(line);

        start = qMin(start, qMax(0, lineLength - 1));

        if (count == -1) {
            count = lineLength - start;
        } else {
            count = qMin(start + count, lineLength) - start;
        }

        Q_ASSERT(start >= 0);
        Q_ASSERT(count >= 0);
        Q_ASSERT((start + count) <= history->getLineLen(line));

        history->getCells(line, start, count, characterBuffer);

        if (history->isWrappedLine(line))
            currentLineProperties |= LINE_WRAPPED;
    } else {
        if (count == -1)
            count = columns - start;

        Q_ASSERT(count >= 0);

        const int screenLine = line - history->getLines();

        Character *data = screenLines[screenLine].data();
        int length = screenLines[screenLine].count();

        for (int i = start; i < qMin(start + count, length); i++) {
            characterBuffer[i - start] = data[i];
        }

        count = qBound(0, count, length >= start ? length - start : 0);

        Q_ASSERT(screenLine < lineProperties.count());
        currentLineProperties |= lineProperties[screenLine];
    }

    const bool omitLineBreak = (currentLineProperties & LINE_WRAPPED) || !preserveLineBreaks;

    if (!omitLineBreak && appendNewLine && (count + 1 < MAX_CHARS)) {
        characterBuffer[count] = '\n';
        count++;
    }

    decoder->decodeLine((Character *)characterBuffer, count, currentLineProperties);

    return count;
}

void Screen::writeLinesToStream(TerminalCharacterDecoder *decoder, int fromLine,
                                int toLine) const {
    writeToStream(decoder, loc(0, fromLine), loc(columns - 1, toLine));
}

void Screen::addHistLine() {

    if (hasScroll()) {
        int oldHistLines = history->getLines();

        history->addCellsVector(screenLines[0]);
        history->addLine(lineProperties[0] & LINE_WRAPPED);

        int newHistLines = history->getLines();

        bool beginIsTL = (selBegin == selTopLeft);

        if (newHistLines == oldHistLines)
            _droppedLines++;

        if (newHistLines > oldHistLines) {
            if (selBegin != -1) {
                selTopLeft += columns;
                selBottomRight += columns;
            }
        }

        if (selBegin != -1) {
            int top_BR = loc(0, 1 + newHistLines);

            if (selTopLeft < top_BR)
                selTopLeft -= columns;

            if (selBottomRight < top_BR)
                selBottomRight -= columns;

            if (selBottomRight < 0)
                clearSelection();
            else {
                if (selTopLeft < 0)
                    selTopLeft = 0;
            }

            if (beginIsTL)
                selBegin = selTopLeft;
            else
                selBegin = selBottomRight;
        }
    }
}

int Screen::getHistLines() const { return history->getLines(); }

void Screen::setScroll(const HistoryType &t, bool copyPreviousScroll) {
    clearSelection();

    if (copyPreviousScroll)
        history = t.scroll(history);
    else {
        HistoryScroll *oldScroll = history;
        history = t.scroll(nullptr);
        delete oldScroll;
    }
}

bool Screen::hasScroll() const { return history->hasScroll(); }

const HistoryType &Screen::getScroll() const { return history->getType(); }

void Screen::setLineProperty(LineProperty property, bool enable) {
    if (enable)
        lineProperties[cuY] = (LineProperty)(lineProperties[cuY] | property);
    else
        lineProperties[cuY] = (LineProperty)(lineProperties[cuY] & ~property);
}

void Screen::fillWithDefaultChar(Character *dest, int count) {
    for (int i = 0; i < count; i++)
        dest[i] = defaultChar;
}
