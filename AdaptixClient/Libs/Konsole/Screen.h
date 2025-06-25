#ifndef SCREEN_H
#define SCREEN_H

#include <QRect>
#include <QSet>
#include <QTextStream>
#include <QVarLengthArray>

#include "util/Character.h"
#include "util/History.h"

#define MODE_Origin    0
#define MODE_Wrap      1
#define MODE_Insert    2
#define MODE_Screen    3
#define MODE_Cursor    4
#define MODE_NewLine   5
#define MODES_SCREEN   6

class TerminalCharacterDecoder;

class Screen
{
public:
    Screen(int lines, int columns);
    ~Screen();

    void cursorUp(int n);
    void cursorDown(int n);
    void cursorLeft(int n);
    void cursorNextLine(int n);
    void cursorPreviousLine(int n);
    void cursorRight(int n);
    void setCursorY(int y);
    void setCursorX(int x);
    void setCursorYX(int y, int x);
    void setMargins(int topLine , int bottomLine);
    int topMargin() const;
    int bottomMargin() const;

    void setDefaultMargins();

    void newLine();
    void nextLine();

    void index();
    void reverseIndex();

    void scrollUp(int n);
    void scrollDown(int n);
    void toStartOfLine();
    void backspace();
    void tab(int n = 1);
    void backtab(int n);

    void eraseChars(int n);
    void deleteChars(int n);
    void insertChars(int n);
    void repeatChars(int count);
    void deleteLines(int n);
    void insertLines(int n);
    void clearTabStops();
    void changeTabStop(bool set);

    void resetMode(int mode);
    void setMode(int mode);
    void saveMode(int mode);
    void restoreMode(int mode);
    bool getMode(int mode) const;

    void saveCursor();
    void restoreCursor();

    void clearEntireScreen();
    void clearToEndOfScreen();
    void clearToBeginOfScreen();
    void clearEntireLine();
    void clearToEndOfLine();
    void clearToBeginOfLine();

    void helpAlign();

    void setRendition(int rendition);
    void resetRendition(int rendition);

    void setForeColor(int space, int color);
    void setBackColor(int space, int color);
    void setDefaultRendition();

    int  getCursorX() const;
    int  getCursorY() const;
    
    QString getScreenText(int row1, int col1, int row2, int col2, int mode);

    void clear();
    void home();
    void reset(bool clearScreen = true);

    void displayCharacter(wchar_t c);

    void compose(const QString& compose);

    void resizeImage(int new_lines, int new_columns);

    void getImage( Character* dest , int size , int startLine , int endLine ) const;

    QVector<LineProperty> getLineProperties( int startLine , int endLine ) const;


    int getLines() const { return lines; }
    int getColumns() const { return columns; }
    int getHistLines() const;
    void setScroll(const HistoryType& , bool copyPreviousScroll = true);
    const HistoryType& getScroll() const;
    bool hasScroll() const;

    void setSelectionStart(const int column, const int line, const bool blockSelectionMode);

    void setSelectionEnd(const int column, const int line);

    void getSelectionStart(int& column , int& line) const;

    void getSelectionEnd(int& column , int& line) const;

    void clearSelection();
    bool isClearSelection();

    bool isSelected(const int column,const int line) const;

    QString selectedText(bool preserveLineBreaks) const;

    void writeLinesToStream(TerminalCharacterDecoder* decoder, int fromLine, int toLine) const;

    void writeSelectionToStream(TerminalCharacterDecoder* decoder , bool  preserveLineBreaks = true) const;

    void checkSelection(int from, int to);

    void setLineProperty(LineProperty property , bool enable);

    int scrolledLines() const;

    QRect lastScrolledRegion() const;

    void resetScrolledLines();

    int droppedLines() const;

    void resetDroppedLines();

    static void fillWithDefaultChar(Character* dest, int count);
    
    QSet<uint> usedExtendedChars() const {
        QSet<uint> result;
        for (int i = 0; i < lines; ++i) {
            const ImageLine &il = screenLines[i];
            for (int j = 0; j < columns; ++j) {
                if (il[j].rendition & RE_EXTENDED_CHAR) {
                    result << il[j].character;
                }
            }
        }
        return result;
    }

private:
    Screen(const Screen &) = delete;
    Screen &operator=(const Screen &) = delete;

    int  copyLineToStream(int line, int start, int count, TerminalCharacterDecoder* decoder, bool appendNewLine, bool preserveLineBreaks) const;

    void clearImage(int loca, int loce, char c);

    void moveImage(int dest, int sourceBegin, int sourceEnd);
    void scrollUp(int from, int i);
    void scrollDown(int from, int i);

    void addHistLine();

    void initTabStops();

    void updateEffectiveRendition();
    void reverseRendition(Character& p) const;

    bool isSelectionValid() const;
    void writeToStream(TerminalCharacterDecoder* decoder, int startIndex, int endIndex, bool preserveLineBreaks = true) const;
    void copyFromScreen(Character* dest, int startLine, int count) const;
    void copyFromHistory(Character* dest, int startLine, int count) const;

    int lines;
    int columns;

    typedef QVector<Character> ImageLine;
    ImageLine*          screenLines;

    int _scrolledLines;
    QRect _lastScrolledRegion;

    int _droppedLines;

    QVarLengthArray<LineProperty,64> lineProperties;

    HistoryScroll* history;

    int cuX;
    int cuY;

    CharacterColor currentForeground;
    CharacterColor currentBackground;
    quint8 currentRendition;

    int _topMargin;
    int _bottomMargin;

    bool currentModes[MODES_SCREEN];
    bool savedModes[MODES_SCREEN];

    QBitArray tabStops;

    int selBegin;
    int selTopLeft;
    int selBottomRight;
    bool blockSelectionMode;

    CharacterColor effectiveForeground;
    CharacterColor effectiveBackground;
    quint8 effectiveRendition;

    class SavedState {
    public:
        SavedState()
        : cursorColumn(0),cursorLine(0),rendition(0) {}

        int cursorColumn;
        int cursorLine;
        quint8 rendition;
        CharacterColor foreground;
        CharacterColor background;
    };
    SavedState savedState;

    int lastPos;

    unsigned short lastDrawnChar;

    static Character defaultChar;
};

#endif
