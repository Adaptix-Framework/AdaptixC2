#ifndef VT102EMULATION_H
#define VT102EMULATION_H

#include <cstdio>

#include <QKeyEvent>
#include <QHash>
#include <QTimer>

#include "Emulation.h"
#include "Screen.h"

#define MODE_AppScreen       (MODES_SCREEN+0)   // Mode #1
#define MODE_AppCuKeys       (MODES_SCREEN+1)   // Application cursor keys (DECCKM)
#define MODE_AppKeyPad       (MODES_SCREEN+2)   //
#define MODE_Mouse1000       (MODES_SCREEN+3)   // Send mouse X,Y position on press and release
#define MODE_Mouse1001       (MODES_SCREEN+4)   // Use Highlight mouse tracking
#define MODE_Mouse1002       (MODES_SCREEN+5)   // Use cell motion mouse tracking
#define MODE_Mouse1003       (MODES_SCREEN+6)   // Use all motion mouse tracking
#define MODE_Mouse1005       (MODES_SCREEN+7)   // Xterm-style extended coordinates
#define MODE_Mouse1006       (MODES_SCREEN+8)   // 2nd Xterm-style extended coordinates
#define MODE_Mouse1015       (MODES_SCREEN+9)   // Urxvt-style extended coordinates
#define MODE_Ansi            (MODES_SCREEN+10)   // Use US Ascii for character sets G0-G3 (DECANM)
#define MODE_132Columns      (MODES_SCREEN+11)  // 80 <-> 132 column mode switch (DECCOLM)
#define MODE_Allow132Columns (MODES_SCREEN+12)  // Allow DECCOLM mode
#define MODE_BracketedPaste  (MODES_SCREEN+13)  // Xterm-style bracketed paste mode
#define MODE_total           (MODES_SCREEN+14)

struct CharCodes
{
  char charset[4]; //
  int  cu_cs;      // actual charset.
  bool graphic;    // Some VT100 tricks
  bool pound  ;    // Some VT100 tricks
  bool sa_graphic; // saved graphic
  bool sa_pound;   // saved pound
};

class Vt102Emulation : public Emulation
{
Q_OBJECT

public:
  Vt102Emulation();
  ~Vt102Emulation() override;

  void clearEntireScreen() override;
  void reset() override;
  char eraseChar() const override;

signals:
  void changeBackgroundColorRequest(const QColor &);
  void openUrlRequest(const QString & url);

public slots:
  void sendString(const char*,int length = -1) override;
  void sendText(const QString& text) override;
  void sendKeyEvent(QKeyEvent*, bool fromPaste) override;
  void sendMouseEvent(int buttons, int column, int line, int eventType) override;
  virtual void focusLost();
  virtual void focusGained();

protected:
  void setMode(int mode) override;
  void resetMode(int mode) override;
  void receiveChar(wchar_t cc) override;

private slots:
  void updateTitle();

private:
  void doTitleChanged( int what, const QString & caption );
  wchar_t applyCharset(wchar_t c);
  void setCharset(int n, int cs);
  void useCharset(int n);
  void setAndUseCharset(int n, int cs);
  void saveCursor();
  void restoreCursor();
  void resetCharset(int scrno);

  void setMargins(int top, int bottom);
  void setDefaultMargins();

  bool getMode    (int mode);
  void saveMode   (int mode);
  void restoreMode(int mode);
  void resetModes();

  void resetTokenizer();
  #define MAX_TOKEN_LENGTH 100000 // Max length of tokens (e.g. window title)
  void addToCurrentToken(wchar_t cc);
  wchar_t tokenBuffer[MAX_TOKEN_LENGTH]; //FIXME: overflow?
  int tokenBufferPos;
#define MAXARGS 15
  void addDigit(int dig);
  void addArgument();
  int argv[MAXARGS];
  int argc;
  void initTokenizer();
  int prevCC;

  int charClass[256];

  void reportDecodingError();

  void processToken(int code, wchar_t p, int q);
  void processOSC();
  void processWindowAttributeChange(int attributeToChange, QString newValue);
  void requestWindowAttribute(int);

  void reportTerminalType();
  void reportSecondaryAttributes();
  void reportStatus();
  void reportAnswerBack();
  void reportCursorPosition();
  void reportTerminalParms(int p);

  void onScrollLock();
  void scrollLock(const bool lock);

  void clearScreenAndSetColumns(int columnCount);

  CharCodes _charset[2];

  class TerminalState
  {
  public:
    TerminalState()
    { memset(&mode,false,MODE_total * sizeof(bool)); }

    bool mode[MODE_total];
  };

  TerminalState _currentModes;
  TerminalState _savedModes;

  QHash<int,QString> _pendingTitleUpdates;
  QTimer* _titleUpdateTimer;

  bool _reportFocusEvents;
  QStringEncoder _toUtf8;

  bool _isTitleChanged; ///< flag if the title/icon was changed by user
  QString _userTitle;
  QString _iconText; // as set by: echo -en '\033]1;IconText\007
  QString _nameTitle;
  QString _iconName;
  QColor _modifiedBackground; // as set by: echo -en '\033]11;Color\007
};

#endif // VT102EMULATION_H
