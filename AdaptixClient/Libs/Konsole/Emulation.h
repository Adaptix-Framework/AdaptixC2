#ifndef EMULATION_H
#define EMULATION_H

#include <cstdio>

#include <QKeyEvent>
#include <QStringDecoder>
#include <QTextStream>
#include <QTimer>
#include <QStringEncoder>

#include "util/KeyboardTranslator.h"

class HistoryType;
class Screen;
class ScreenWindow;
class TerminalCharacterDecoder;

enum
{
    NOTIFYNORMAL=0,
    NOTIFYBELL=1,
    NOTIFYACTIVITY=2,
    NOTIFYSILENCE=3
};

class Emulation : public QObject
{
Q_OBJECT

public:
    enum class KeyboardCursorShape {
        BlockCursor = 0,
        UnderlineCursor = 1,
        IBeamCursor = 2
    };


    Emulation();
    ~Emulation() override;

    ScreenWindow* createWindow();

    QSize imageSize() const;

    int lineCount() const;

    void setHistory(const HistoryType&);
    const HistoryType& history() const;
    void clearHistory();

    virtual void writeToStream(TerminalCharacterDecoder* decoder,int startLine,int endLine);
        
    const QStringEncoder &codec() const { return _fromUtf16; }
    void setCodec(QStringEncoder);

    bool utf8() const;

    virtual char eraseChar() const;

    void setKeyBindings(const QString& name);
    QString keyBindings() const;

    virtual void clearEntireScreen() =0;

    virtual void reset() =0;

    bool programUsesMouse() const;

    bool programBracketedPasteMode() const;

    void setEnableHandleCtrlC(bool enable) { _enableHandleCtrlC = enable; }

public slots:

    virtual void setImageSize(int lines, int columns);

    virtual void sendText(const QString& text) = 0;

    virtual void sendKeyEvent(QKeyEvent*, bool fromPaste);

    virtual void sendMouseEvent(int buttons, int column, int line, int eventType);

    virtual void sendString(const char* string, int length = -1) = 0;

    void receiveData(const char* buffer,int len);

    void dupDisplayCharacter(wchar_t cc);

signals:

    void sendData(const char* data,int len);

    void dupDisplayOutput(const char* data,int len);

    void lockPtyRequest(bool suspend);

    void useUtf8Request(bool);

    void stateSet(int state);

    void zmodemSendDetected();
    void zmodemRecvDetected();


    void changeTabTextColorRequest(int color);

    void programUsesMouseChanged(bool usesMouse);

    void programBracketedPasteModeChanged(bool bracketedPasteMode);

    void outputChanged();

    void titleChanged(int title,const QString& newTitle);

    void imageSizeChanged(int lineCount , int columnCount);

    void imageSizeInitialized();

    void imageResizeRequest(const QSize& size);

    void profileChangeCommandReceived(const QString& text);

    void flowControlKeyPressed(bool suspendKeyPressed);

    void primaryScreenInUse(bool use);

    void cursorChanged(KeyboardCursorShape cursorShape, bool blinkingCursorEnabled);

    void handleCommandFromKeyboard(KeyboardTranslator::Command command);
    void handleCtrlC(void);
    void outputFromKeypressEvent(void);

protected:
    virtual void setMode(int mode) = 0;
    virtual void resetMode(int mode) = 0;

    virtual void receiveChar(wchar_t ch);

    void setScreen(int index);

    enum EmulationCodec {
        LocaleCodec = 0,
        Utf8Codec   = 1
    };
    void setCodec(EmulationCodec codec); // codec number, 0 = locale, 1=utf8

    QList<ScreenWindow*> _windows;

    Screen* _currentScreen;  // pointer to the screen which is currently active,

    Screen* _screen[2];

    QStringEncoder _fromUtf16;
    QStringDecoder _toUtf16;

    const KeyboardTranslator* _keyTranslator; // the keyboard layout
    
    bool _enableHandleCtrlC;

protected slots:

    void bufferedUpdate();
    
    void checkScreenInUse();

private slots:

    void showBulk();

    void usesMouseChanged(bool usesMouse);

    void bracketedPasteModeChanged(bool bracketedPasteMode);

private:
    bool _usesMouse;
    bool _bracketedPasteMode;
    QTimer _bulkTimer1{this};
    QTimer _bulkTimer2{this};
    QStringEncoder _fromUtf8;
    QByteArray dupCache;
};

#endif // EMULATION_H