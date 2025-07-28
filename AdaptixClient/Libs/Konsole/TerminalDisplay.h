#ifndef TERMINALDISPLAY_H
#define TERMINALDISPLAY_H

#include <QColor>
#include <QPointer>
#include <QWidget>
#include <QClipboard>
#include <QMovie>
#include <QPlainTextEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollBar>

#include "util/Filter.h"
#include "util/Character.h"
#include "util/CharWidth.h"
#include "konsole.h"

class QDrag;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QTimer;
class QEvent;
class QGridLayout;
class QKeyEvent;
class QShowEvent;
class QHideEvent;
class QTimerEvent;
class QWidget;

enum MotionAfterPasting
{
    NoMoveScreenWindow = 0,
    MoveStartScreenWindow = 1,
    MoveEndScreenWindow = 2
};

enum BackgroundMode {
    None,
    Stretch,
    Zoom,
    Fit,
    Center,
    Tile
};

class ScreenWindow;
class ScrollBar;

class TerminalDisplay : public QWidget
{
   Q_OBJECT

public:
    TerminalDisplay(QWidget *parent=nullptr);
    ~TerminalDisplay() override;

    const ColorEntry* colorTable() const;
    void setColorTable(const ColorEntry table[]);

    void setOpacity(qreal opacity);

    void setBackgroundPixmap(QPixmap *backgroundImage);
    void reloadBackgroundPixmap(void);
    void setBackgroundImage(const QString& backgroundImage);
    void setBackgroundMovie(const QString& backgroundImage);
    void setBackgroundVideo(const QString& backgroundImage);

    void setBackgroundMode(BackgroundMode mode);

    void setScrollBarPosition(QTermWidget::ScrollBarPosition position);

    void setScroll(int cursor, int lines);

    void scrollToEnd();

    FilterChain* filterChain() const;

    void processFilters();

    QList<QAction*> filterActions(const QPoint& position);

    bool blinkingCursor() { return _hasBlinkingCursor; }
    void setBlinkingCursor(bool blink);

    void setBlinkingTextEnabled(bool blink);

    void setCtrlDrag(bool enable) { _ctrlDrag=enable; }
    bool ctrlDrag() { return _ctrlDrag; }

    enum TripleClickMode
    {
        SelectWholeLine,
        SelectForwardsFromCursor
    };
    void setTripleClickMode(TripleClickMode mode) { _tripleClickMode = mode; }
    TripleClickMode tripleClickMode() { return _tripleClickMode; }

    void setLineSpacing(uint);
    void setMargin(int);

    int margin() const;
    uint lineSpacing() const;

    void emitSelection(bool useXselection,bool appendReturn);

    void bracketText(QString& text) const;

    void setKeyboardCursorShape(QTermWidget::KeyboardCursorShape shape);

    QTermWidget::KeyboardCursorShape keyboardCursorShape() const;

    void setKeyboardCursorColor(bool useForegroundColor , const QColor& color);

    QColor keyboardCursorColor() const;

    int  lines()   { return _lines;   }

    int  columns() { return _columns; }

    int  fontHeight()   { return _fontHeight;   }

    int  fontWidth()    { return _fontWidth; }

    void setSize(int cols, int lins);
    void setFixedSize(int cols, int lins);

    QSize sizeHint() const override;

    void setWordCharacters(const QString& wc);

    QString wordCharacters() { return _wordCharacters; }

    void setBellMode(int mode);

    int bellMode() { return _bellMode; }


    enum BellMode
    {
        SystemBeepBell=0,
        NotifyBell=1,
        VisualBell=2,
        NoBell=3
    };

    void setSelection(const QString &t);

    virtual void setFont(const QFont &);

    QFont getVTFont() { return font(); }

    void setVTFont(const QFont& font);

    static void setAntialias( bool antialias ) { _antialiasText = antialias; }
    static bool antialias()                 { return _antialiasText;   }

    void setDrawLineChars(bool drawLineChars) { _drawLineChars = drawLineChars; }

    void setBoldIntense(bool value) { _boldIntense = value; }
    bool getBoldIntense() { return _boldIntense; }

    void setTerminalSizeHint(bool on) { _terminalSizeHint=on; }
    bool terminalSizeHint() { return _terminalSizeHint; }
    void setTerminalSizeStartup(bool on) { _terminalSizeStartup=on; }

    void setBidiEnabled(bool set) { _bidiEnabled=set; }
    bool isBidiEnabled() { return _bidiEnabled; }

    void setScreenWindow( ScreenWindow* window );
    ScreenWindow* screenWindow() const;

    void setMotionAfterPasting(MotionAfterPasting action);
    int motionAfterPasting();
    void setConfirmMultilinePaste(bool confirmMultilinePaste);
    void setTrimPastedTrailingNewlines(bool trimPastedTrailingNewlines);

    void getCharacterPosition(const QPointF& widgetPoint,int& line,int& column) const;

    void disableBracketedPasteMode(bool disable) { _disabledBracketedPasteMode = disable; }
    bool bracketedPasteModeIsDisabled() const { return _disabledBracketedPasteMode; }

    void setShowResizeNotificationEnabled(bool enabled) { _showResizeNotificationEnabled = enabled; }

    void setPreeditColorIndex(int index) {
        _preeditColorIndex = index;
    }

    int mouseAutohideDelay() const { return _mouseAutohideDelay; }

    void autoHideMouseAfter(int delay);

public slots:

    void updateImage();

    void updateFilters();

    void updateLineProperties();

    void copyClipboard(QClipboard::Mode mode = QClipboard::Clipboard);
    void pasteClipboard();
    void pasteSelection();

    void selectAll();

    void setFlowControlWarningEnabled(bool enabled);
    bool flowControlWarningEnabled() const
    { return _flowControlWarningEnabled; }

    void outputSuspended(bool suspended);

    void setUsesMouse(bool usesMouse);

    bool usesMouse() const;

    void usingPrimaryScreen(bool use);

    void setBracketedPasteMode(bool bracketedPasteMode);
    bool bracketedPasteMode() const;

    void bell();

    void setBackgroundColor(const QColor& color);

    void setForegroundColor(const QColor& color);
    void setColorTableColor(const int colorId, const QColor &color);
    void selectionChanged();

    void setSelectionOpacity(qreal opacity) {
        _selectedTextOpacity = opacity;
    }

    int  getCursorX() const;
    int  getCursorY() const;

    void setCursorX(int x);
    void setCursorY(int y);

    QString screenGet(int row1, int col1, int row2, int col2, int mode);

    void setLocked(bool enabled) { _isLocked = enabled; }
    void repaintDisplay(void) {

    #if defined(Q_OS_LINUX)
        this->hide();
        QTimer::singleShot(100, this, [this](){ this->show(); });
    #endif
    }
    void setMessageParentWidget(QWidget *parent) { messageParentWidget = parent; }
    
    void set_fix_quardCRT_issue33(bool fix) { _fix_quardCRT_issue33 = fix; }

signals:

    void keyPressedSignal(QKeyEvent *e, bool fromPaste);

    void mouseSignal(int button, int column, int line, int eventType);
    void mousePressEventForwarded(QMouseEvent* event);
    void changedFontMetricSignal(int height, int width);
    void changedContentSizeSignal(int height, int width);
    void changedContentCountSignal(int line, int column);

    void configureRequest(const QPoint& position);

    void overrideShortcutCheck(QKeyEvent* keyEvent,bool& override);

   void isBusySelecting(bool);
   void sendStringToEmu(const char*);

	void copyAvailable(bool);
	void termGetFocus();
	void termLostFocus();

    void notifyBell();
    void usesMouseChanged();

    void handleCtrlC(void);

protected:
    bool event( QEvent * ) override;

    void paintEvent( QPaintEvent * ) override;

    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;
    void resizeEvent(QResizeEvent*) override;

    virtual void fontChange(const QFont &font);
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    void mousePressEvent( QMouseEvent* ) override;
    void mouseReleaseEvent( QMouseEvent* ) override;
    void mouseMoveEvent( QMouseEvent* ) override;
    virtual void extendSelection( const QPoint& pos );
    void wheelEvent( QWheelEvent* ) override;

    bool focusNextPrevChild( bool next ) override;

    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void doDrag();
    enum DragState { diNone, diPending, diDragging };

    struct _dragInfo {
      DragState       state;
      QPoint          start;
      QDrag           *dragObject;
    } dragInfo;

    QChar charClass(const Character &ch) const;

    void clearImage();

    void mouseTripleClickEvent(QMouseEvent* ev);

    void inputMethodEvent ( QInputMethodEvent* event ) override;
    QVariant inputMethodQuery( Qt::InputMethodQuery query ) const override;

protected slots:

    void scrollBarPositionChanged(int value);
    void blinkEvent();
    void blinkCursorEvent();

    void enableBell();

private slots:

    void swapColorTable();
    void tripleClickTimeout();

private:

    int textWidth(int startColumn, int length, int line) const;
    QRect calculateTextArea(int topLeftX, int topLeftY, int startColumn, int line, int length);

    void drawContents(QPainter &paint, const QRect &rect);
    void drawTextFragment(QPainter& painter, const QRect& rect, const std::wstring& text, Character* style, bool tooWide, bool isSelection);
    void drawBackground(QPainter& painter, const QRect& rect, const QColor& color, bool useOpacitySetting);
    void drawCursor(QPainter& painter, const QRect& rect , const QColor& foregroundColor, const QColor& backgroundColor , bool& invertColors, bool preedit = false);
    void drawCharacters(QPainter& painter, const QRect& rect,  const std::wstring& text, const Character* style, bool invertCharacterColor, bool tooWide = false);
    void drawLineCharString(QPainter& painter, int x, int y, const std::wstring& str, const Character* attributes) const;
    void drawLineCharString(QPainter& painter, int x, int y, wchar_t ch, const Character* attributes) const;

    void drawInputMethodPreeditString(QPainter& painter , const QRect& rect);

    QRect imageToWidget(const QRect& imageArea) const;

    QRect preeditRect() const;

    void showResizeNotification();

    void scrollImage(int lines , const QRect& region);

    bool multilineConfirmation(QString& text);

    void calcGeometry();
    void propagateSize();
    void updateImageSize();
    void makeImage();

    void paintFilters(QPainter& painter);

    void calDrawTextAdditionHeight(QPainter& painter);

    QRegion hotSpotRegion() const;

    QPoint cursorPosition() const;

    void updateCursor();

    bool handleShortcutOverrideEvent(QKeyEvent* event);

    bool isLineChar(Character c) const;
    bool isLineCharString(const std::wstring& string) const;

    void hideStaleMouse() const;

    QPointer<ScreenWindow> _screenWindow;

    bool _allowBell;

    QGridLayout* _gridLayout;

    CharWidth *_charWidth;
    bool _fixedFont;
    bool _fixedFont_original;
    int  _fontHeight;
    int  _fontWidth;
    int  _fontAscent;
    bool _boldIntense;
    int  _drawTextAdditionHeight;
    bool _drawTextTestFlag;

    int _leftMargin;
    int _topMargin;

    int _lines;
    int _columns;

    int _usedLines;

    int _usedColumns;

    int _contentHeight;
    int _contentWidth;
    Character* _image;

    int _imageSize;
    QVector<LineProperty> _lineProperties;

    ColorEntry _colorTable[TABLE_COLORS];

    bool _resizing;
    bool _terminalSizeHint;
    bool _terminalSizeStartup;
    bool _bidiEnabled;
    bool _mouseMarks;
    bool _isPrimaryScreen;
    bool _bracketedPasteMode;
    bool _disabledBracketedPasteMode;
    bool _showResizeNotificationEnabled;

    QPoint  _iPntSel;
    QPoint  _pntSel;
    QPoint  _tripleSelBegin;
    int     _actSel;
    bool    _wordSelectionMode;
    bool    _lineSelectionMode;
    bool    _preserveLineBreaks;
    bool    _columnSelectionMode;

    QClipboard*  _clipboard;
    ScrollBar* _scrollBar;
    QTermWidget::ScrollBarPosition _scrollbarLocation;
    QString     _wordCharacters;
    int         _bellMode;

    bool _blinking;
    bool _hasBlinker;
    bool _cursorBlinking;
    bool _hasBlinkingCursor;
    bool _allowBlinkingText;
    bool _ctrlDrag;
    TripleClickMode _tripleClickMode;
    bool _isFixedSize;
    QTimer* _blinkTimer;
    QTimer* _blinkCursorTimer;
    static std::shared_ptr<QTimer> _hideMouseTimer;

    QString _dropText;
    int _dndFileCount;

    bool _possibleTripleClick;

    QLabel* _resizeWidget;
    QTimer* _resizeTimer;

    bool _flowControlWarningEnabled;

    QLabel* _outputSuspendedLabel;

    uint _lineSpacing;

    bool _colorsInverted;

    QSize _size;

    qreal _opacity;

    QPixmap *_backgroundPixmapRef = nullptr;
    QPixmap _backgroundImage;
    QPixmap _backgroundVideoFrame;
    bool _isLocked;
    QPixmap _lockbackgroundImage;
    BackgroundMode _backgroundMode;

    qreal _selectedTextOpacity;

    TerminalImageFilterChain* _filterChain;
    QRegion _mouseOverHotspotArea;

    QTermWidget::KeyboardCursorShape _cursorShape;

    QColor _cursorColor;


    MotionAfterPasting mMotionAfterPasting;
    bool _confirmMultilinePaste = true;
    bool _trimPastedTrailingNewlines = true;

    struct InputMethodData
    {
        std::wstring preeditString;
        QRect previousPreeditRect;
    };
    InputMethodData _inputMethodData;

    static bool _antialiasText;

    static const int TEXT_BLINK_DELAY = 500;

    int _leftBaseMargin;
    int _topBaseMargin;

    bool _drawLineChars;

    int _mouseAutohideDelay;

    int _preeditColorIndex = 16;

    int shiftSelectionStartX = -1;
    int shiftSelectionStartY = -1;

    QWidget *messageParentWidget = nullptr;

    bool _fix_quardCRT_issue33 = false;
};

class AutoScrollHandler : public QObject
{
    Q_OBJECT
public:
    AutoScrollHandler(QWidget* parent);
protected:
    void timerEvent(QTimerEvent* event) override;
    bool eventFilter(QObject* watched,QEvent* event) override;
private:
    QWidget* widget() const { return static_cast<QWidget*>(parent()); }
    int _timerId;
};

class ScrollBar : public QScrollBar
{
Q_OBJECT

public:
    ScrollBar(QWidget* parent = nullptr);
protected:
    void enterEvent(QEnterEvent* event) override;
};

class MultilineConfirmationMessageBox : public QDialog {
    Q_OBJECT

public:
    explicit MultilineConfirmationMessageBox(QWidget *parent = nullptr) : QDialog(parent) {
        setModal(true);
        setSizeGripEnabled(true);
        resize(500, 300);
        QVBoxLayout *layout = new QVBoxLayout(this);
        messageText = new QLabel(this);
        detailedText = new QPlainTextEdit(this);
        buttonBox = new QDialogButtonBox(QDialogButtonBox::Yes|QDialogButtonBox::No);
        layout->addWidget(messageText);
        layout->addWidget(detailedText);
        layout->addWidget(buttonBox);
        setLayout(layout);
        detailedText->setLineWrapMode(QPlainTextEdit::NoWrap);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
    ~MultilineConfirmationMessageBox() override {
        delete messageText;
        delete detailedText;
        delete buttonBox;
    }
    void setText(const QString &text) {
        messageText->setText(text);
    }
    void setDetailedText(const QString &text) {
        detailedText->setPlainText(text);
    }
    QString getDetailedText() const {
        return detailedText->toPlainText();
    }

private:
    QLabel *messageText;
    QPlainTextEdit *detailedText;
    QDialogButtonBox *buttonBox;
};

#endif
