#ifndef SCREENWINDOW_H
#define SCREENWINDOW_H

#include <QObject>
#include <QPoint>
#include <QRect>

#include "util/Character.h"
#include "util/KeyboardTranslator.h"

class Screen;

class ScreenWindow : public QObject
{
    Q_OBJECT
public:
    ScreenWindow(QObject* parent = nullptr);
    ~ScreenWindow() override;

    void setScreen(Screen* screen);
    Screen* screen() const;

    Character* getImage();

    QVector<LineProperty> getLineProperties();

    int scrollCount() const;

    void resetScrollCount();

    QRect scrollRegion() const;

    void setSelectionStart( int column , int line , bool columnMode );
    void setSelectionEnd( int column , int line );
    void getSelectionStart( int& column , int& line );
    void getSelectionEnd( int& column , int& line );
    bool isSelected( int column , int line );
    void clearSelection();
    bool isClearSelection();

    void setWindowLines(int lines);
    int windowLines() const;
    int windowColumns() const;

    int lineCount() const;
    int columnCount() const;

    int currentLine() const;

    QPoint cursorPosition() const;

    int getCursorX() const;
    int getCursorY() const;
    void setCursorX(int x);
    void setCursorY(int y);
    
    bool atEndOfOutput() const;

    void scrollTo( int line );

    enum RelativeScrollMode {
        ScrollLines,
        ScrollPages
    };

    void scrollBy( RelativeScrollMode mode , int amount );

    void setTrackOutput(bool trackOutput);
    bool trackOutput() const;

    QString selectedText( bool preserveLineBreaks ) const;

    QString getScreenText(int row1, int col1, int row2, int col2, int mode);

public slots:
    void notifyOutputChanged();

    void handleCommandFromKeyboard(KeyboardTranslator::Command command);

signals:
    void outputChanged();

    void scrolled(int line);

    void selectionChanged();

    void scrollToEnd();

    void handleCtrlC(void);

private:
    int endWindowLine() const;
    void fillUnusedArea();

    Screen* _screen;
    Character* _windowBuffer;
    int _windowBufferSize;
    bool _bufferNeedsUpdate;

    int  _windowLines;
    int  _currentLine;
    bool _trackOutput;
    int  _scrollCount;
};

#endif // SCREENWINDOW_H
