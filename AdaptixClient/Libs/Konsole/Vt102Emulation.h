#ifndef VT102EMULATION_H
#define VT102EMULATION_H

#include <cstdio>

#include <QKeyEvent>
#include <QHash>
#include <QTimer>

#include "Emulation.h"
#include "Screen.h"

#define MODE_AppScreen       (MODES_SCREEN+0)
#define MODE_AppCuKeys       (MODES_SCREEN+1)
#define MODE_AppKeyPad       (MODES_SCREEN+2)
#define MODE_Mouse1000       (MODES_SCREEN+3)
#define MODE_Mouse1001       (MODES_SCREEN+4)
#define MODE_Mouse1002       (MODES_SCREEN+5)
#define MODE_Mouse1003       (MODES_SCREEN+6)
#define MODE_Mouse1005       (MODES_SCREEN+7)
#define MODE_Mouse1006       (MODES_SCREEN+8)
#define MODE_Mouse1015       (MODES_SCREEN+9)
#define MODE_Ansi            (MODES_SCREEN+10)
#define MODE_132Columns      (MODES_SCREEN+11)
#define MODE_Allow132Columns (MODES_SCREEN+12)
#define MODE_BracketedPaste  (MODES_SCREEN+13)
#define MODE_total           (MODES_SCREEN+14)

struct CharCodes
{
  char charset[4];
  int  cu_cs;
  bool graphic;
  bool pound  ;
  bool sa_graphic;
  bool sa_pound;
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
  #define MAX_TOKEN_LENGTH 100000
  void addToCurrentToken(wchar_t cc);
  wchar_t tokenBuffer[MAX_TOKEN_LENGTH];
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

  bool _isTitleChanged;
  QString _userTitle;
  QString _iconText;
  QString _nameTitle;
  QString _iconName;
  QColor _modifiedBackground;
};

#endif
