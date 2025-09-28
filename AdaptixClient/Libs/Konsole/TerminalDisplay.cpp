#include "TerminalDisplay.h"

#include <QAbstractButton>
#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QDrag>
#include <QEvent>
#include <QFile>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QMimeData>
#include <QMovie>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QStyle>
#include <QTime>
#include <QTimer>
#include <QToolTip>
#include <QUrl>
#include <QtDebug>

#include "util/Filter.h"
#include "ScreenWindow.h"
#include "util/TerminalCharacterDecoder.h"

#ifndef loc
#define loc(X, Y) ((Y) * _columns + (X))
#endif

#define yMouseScroll 1

#define REPCHAR                                                                  \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"                                                 \
    "abcdefgjijklmnopqrstuvwxyz"                                                 \
    "0123456789./+@"

const ColorEntry base_color_table[TABLE_COLORS] =

{
    ColorEntry(QColor(0x00, 0x00, 0x00), false),
    ColorEntry(QColor(0xB2, 0xB2, 0xB2), true),
    ColorEntry(QColor(0x00, 0x00, 0x00), false),
    ColorEntry(QColor(0xB2, 0x18, 0x18), false),
    ColorEntry(QColor(0x18, 0xB2, 0x18), false),
    ColorEntry(QColor(0xB2, 0x68, 0x18), false),
    ColorEntry(QColor(0x18, 0x18, 0xB2), false),
    ColorEntry(QColor(0xB2, 0x18, 0xB2), false),
    ColorEntry(QColor(0x18, 0xB2, 0xB2), false),
    ColorEntry(QColor(0xB2, 0xB2, 0xB2), false),
    ColorEntry(QColor(0x00, 0x00, 0x00), false),
    ColorEntry(QColor(0xFF, 0xFF, 0xFF), true),
    ColorEntry(QColor(0x68, 0x68, 0x68), false),
    ColorEntry(QColor(0xFF, 0x54, 0x54), false),
    ColorEntry(QColor(0x54, 0xFF, 0x54), false),
    ColorEntry(QColor(0xFF, 0xFF, 0x54), false),
    ColorEntry(QColor(0x54, 0x54, 0xFF), false),
    ColorEntry(QColor(0xFF, 0x54, 0xFF), false),
    ColorEntry(QColor(0x54, 0xFF, 0xFF), false),
    ColorEntry(QColor(0xFF, 0xFF, 0xFF), false)
};

bool TerminalDisplay::_antialiasText = true;

const QChar LTR_OVERRIDE_CHAR(0x202D);

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Colors                                     */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*  Note that we use ANSI color order (bgr), while IBMPC color order is (rgb)

    Code        0       1       2       3       4       5       6       7
    ----------- ------- ------- ------- ------- ------- ------- ------- -------
    ANSI  (bgr) Black   Red     Green   Yellow  Blue    Magenta Cyan    White
    IBMPC (rgb) Black   Blue    Green   Cyan    Red     Magenta Yellow  White
*/

static QPoint gs_deadSpot(-1,-1);
static QPoint gs_futureDeadSpot;
std::shared_ptr<QTimer> TerminalDisplay::_hideMouseTimer;

ScreenWindow *TerminalDisplay::screenWindow() const { return _screenWindow; }
void TerminalDisplay::setScreenWindow(ScreenWindow *window) {
    if (_screenWindow) {
        disconnect(_screenWindow, nullptr, this, nullptr);
    }

    _screenWindow = window;

    if (window) {
        connect(_screenWindow, &ScreenWindow::outputChanged, this,
                        &TerminalDisplay::updateLineProperties);
        connect(_screenWindow, &ScreenWindow::outputChanged, this,
                        &TerminalDisplay::updateImage);
        connect(_screenWindow, &ScreenWindow::outputChanged, this,
                        &TerminalDisplay::updateFilters);
        connect(_screenWindow, &ScreenWindow::scrolled, this,
                        &TerminalDisplay::updateFilters);
        connect(_screenWindow, &ScreenWindow::scrollToEnd, this,
                        &TerminalDisplay::scrollToEnd);
        connect(_screenWindow, &ScreenWindow::handleCtrlC, this,
                        &TerminalDisplay::handleCtrlC);
        window->setWindowLines(_lines);
    }
}

const ColorEntry *TerminalDisplay::colorTable() const { return _colorTable; }

void TerminalDisplay::setBackgroundColor(const QColor &color) {
    _colorTable[DEFAULT_BACK_COLOR].color = color;
    QPalette p = palette();
    p.setColor(backgroundRole(), color);
    setPalette(p);

    _scrollBar->setPalette(QApplication::palette());

    update();
}

void TerminalDisplay::setForegroundColor(const QColor &color) {
    _colorTable[DEFAULT_FORE_COLOR].color = color;

    update();
}

void TerminalDisplay::setColorTableColor(const int colorId, const QColor &color) {
    _colorTable[colorId].color = color;
    update();
}

void TerminalDisplay::setColorTable(const ColorEntry table[]) {
    for (int i = 0; i < TABLE_COLORS; i++)
        _colorTable[i] = table[i];

    setBackgroundColor(_colorTable[DEFAULT_BACK_COLOR].color);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                   Font                                    */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*
 The VT100 has 32 special graphical characters. The usual vt100 extended
 xterm fonts have these at 0x00..0x1f.
 
 QT's iso mapping leaves 0x00..0x7f without any changes. But the graphicals
 come in here as proper unicode characters.
 
 We treat non-iso10646 fonts as VT100 extended and do the required mapping
 from unicode to 0x00..0x1f. The remaining translation is then left to the
 QCodec.
*/

bool TerminalDisplay::isLineChar(Character c) const {
    return _drawLineChars && c.isLineChar();
}

bool TerminalDisplay::isLineCharString(const std::wstring& string) const {
    return string.length() > 0 && _drawLineChars && (string[0] & 0xFF80) == 0x2500;
}

void TerminalDisplay::fontChange(const QFont &) {
    QFontMetrics fm(font());
    _fontHeight = fm.height() + _lineSpacing;

    _fontWidth = qRound((double)fm.horizontalAdvance(QLatin1String(REPCHAR)) /
                                            (double)qstrlen(REPCHAR));

    _fixedFont = true;

    int fw = fm.horizontalAdvance(QLatin1Char(REPCHAR[0]));
    for (unsigned int i = 1; i < qstrlen(REPCHAR); i++) {
        if (fw != fm.horizontalAdvance(QLatin1Char(REPCHAR[i]))) {
            _fixedFont = false;
            break;
        }
    }

    _fixedFont_original = _fixedFont;

    if (_fontWidth < 1)
        _fontWidth = 1;

    _fontAscent = fm.ascent();

    Q_EMIT changedFontMetricSignal(_fontHeight, _fontWidth);
    propagateSize();

    _drawTextTestFlag = true;
    update();
}

void TerminalDisplay::calDrawTextAdditionHeight(QPainter &painter) {
    QRect test_rect, feedback_rect;
    test_rect.setRect(1, 1, _fontWidth * 4, _fontHeight);
    painter.save();
    painter.setOpacity(0);
    painter.drawText(test_rect, Qt::AlignBottom,
                                     LTR_OVERRIDE_CHAR + QLatin1String("Mq"), &feedback_rect);

    painter.restore();

    _drawTextAdditionHeight = qMax(0, (feedback_rect.height() - _fontHeight) / 2);

    _drawTextTestFlag = false;
}

void TerminalDisplay::setVTFont(const QFont &f) {
    QFont font = f;

    if (!QFontInfo(font).fixedPitch()) {
    }

    if (!_antialiasText)
        font.setStyleStrategy(QFont::NoAntialias);

    font.setKerning(false);

    font.setHintingPreference(QFont::PreferFullHinting);

    font.setStyleName(QString());

    QWidget::setFont(font);
    _charWidth->setFont(font);
    fontChange(font);
}

void TerminalDisplay::setFont(const QFont &) {}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                         Constructor / Destructor                          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

TerminalDisplay::TerminalDisplay(QWidget *parent)
        : QWidget(parent), _screenWindow(nullptr), _allowBell(true),
        _gridLayout(nullptr), _fontHeight(1), _fontWidth(1), _fontAscent(1),
        _boldIntense(true), _lines(1), _columns(1), _usedLines(1),
        _usedColumns(1), _contentHeight(1), _contentWidth(1), _image(nullptr),
        _resizing(false), _terminalSizeHint(false), _terminalSizeStartup(true),
        _bidiEnabled(true), _mouseMarks(false), _isPrimaryScreen(true),
        _disabledBracketedPasteMode(false), _showResizeNotificationEnabled(true),
        _actSel(0), _wordSelectionMode(false), _lineSelectionMode(false),
        _preserveLineBreaks(false), _columnSelectionMode(false),
        _scrollbarLocation(QTermWidget::NoScrollBar),
        _wordCharacters(QLatin1String(":@-./_~")), _bellMode(SystemBeepBell),
        _blinking(false), _hasBlinker(false), _cursorBlinking(false),
        _hasBlinkingCursor(false), _allowBlinkingText(true), _ctrlDrag(false),
        _tripleClickMode(SelectWholeLine), _isFixedSize(false),
        _possibleTripleClick(false), _resizeWidget(nullptr),
        _resizeTimer(nullptr), _flowControlWarningEnabled(false),
        _outputSuspendedLabel(nullptr), _lineSpacing(0), _colorsInverted(false),
        _opacity(static_cast<qreal>(1)), _backgroundMode(None),
        _selectedTextOpacity(static_cast<qreal>(1)),
        _filterChain(new TerminalImageFilterChain()),
        _cursorShape(Emulation::KeyboardCursorShape::BlockCursor),
        mMotionAfterPasting(NoMoveScreenWindow), _leftBaseMargin(1),
        _topBaseMargin(1), _drawLineChars(true),_mouseAutohideDelay(-1) {
    _drawTextAdditionHeight = 0;
    _drawTextTestFlag = false;

    setLayoutDirection(Qt::LeftToRight);

    _topMargin = _topBaseMargin;
    _leftMargin = _leftBaseMargin;

    _scrollBar = new ScrollBar(this);
    QString style_sheet = qApp->styleSheet();
    _scrollBar->setStyleSheet(style_sheet);
    if (!_scrollBar->style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, _scrollBar))
        _scrollBar->setAutoFillBackground(true);
    setScroll(0, 0);
    _scrollBar->setCursor(Qt::ArrowCursor);
    connect(_scrollBar, &QScrollBar::valueChanged, this,
                    &TerminalDisplay::scrollBarPositionChanged);
    _scrollBar->hide();

    _blinkTimer = new QTimer(this);
    connect(_blinkTimer, &QTimer::timeout, this, &TerminalDisplay::blinkEvent);
    _blinkCursorTimer = new QTimer(this);
    connect(_blinkCursorTimer, &QTimer::timeout, this,
                    &TerminalDisplay::blinkCursorEvent);

    setUsesMouse(true);
    setBracketedPasteMode(false);
    setColorTable(base_color_table);
    setMouseTracking(true);

    setAcceptDrops(true);
    dragInfo.state = diNone;

    setFocusPolicy(Qt::WheelFocus);

    setAttribute(Qt::WA_InputMethodEnabled, true);
    setInputMethodHints(Qt::ImhSensitiveData | Qt::ImhNoAutoUppercase |
                                            Qt::ImhNoPredictiveText | Qt::ImhMultiLine);

    setAttribute(Qt::WA_OpaquePaintEvent);

    _gridLayout = new QGridLayout(this);
    _gridLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(_gridLayout);

    _charWidth = new CharWidth(font());

    _isLocked = false;
    _lockbackgroundImage = QPixmap(10, 10);
    _lockbackgroundImage.fill(Qt::gray);

    new AutoScrollHandler(this);
}

TerminalDisplay::~TerminalDisplay() {
    disconnect(_blinkTimer);
    disconnect(_blinkCursorTimer);
    if (_hideMouseTimer)
        disconnect(_hideMouseTimer.get());
    qApp->removeEventFilter(this);

    delete[] _image;

    delete _charWidth;
    delete _gridLayout;
    delete _outputSuspendedLabel;
    delete _filterChain;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                             Display Operations                            */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/**
 A table for emulating the simple (single width) unicode drawing chars.
 It represents the 250x - 257x glyphs. If it's zero, we can't use it.
 if it's not, it's encoded as follows: imagine a 5x5 grid where the points are
 numbered 0 to 24 left to top, top to bottom. Each point is represented by the
 corresponding bit.

 Then, the pixels basically have the following interpretation:
 _|||_
 -...-
 -...-
 -...-
 _|||_

 where _ = none
      | = vertical line.
      - = horizontal line.
 */

enum LineEncode {
    TopL = (1 << 1),
    TopC = (1 << 2),
    TopR = (1 << 3),

    LeftT = (1 << 5),
    Int11 = (1 << 6),
    Int12 = (1 << 7),
    Int13 = (1 << 8),
    RightT = (1 << 9),

    LeftC = (1 << 10),
    Int21 = (1 << 11),
    Int22 = (1 << 12),
    Int23 = (1 << 13),
    RightC = (1 << 14),

    LeftB = (1 << 15),
    Int31 = (1 << 16),
    Int32 = (1 << 17),
    Int33 = (1 << 18),
    RightB = (1 << 19),

    BotL = (1 << 21),
    BotC = (1 << 22),
    BotR = (1 << 23)
};

static const quint32 LineChars[] = {
    0x00007c00, 0x000fffe0, 0x00421084, 0x00e739ce, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00427000, 0x004e7380, 0x00e77800, 0x00ef7bc0, 0x00421c00, 0x00439ce0,
    0x00e73c00, 0x00e7bde0, 0x00007084, 0x000e7384, 0x000079ce, 0x000f7bce,
    0x00001c84, 0x00039ce4, 0x00003dce, 0x0007bdee, 0x00427084, 0x004e7384,
    0x004279ce, 0x00e77884, 0x00e779ce, 0x004f7bce, 0x00ef7bc4, 0x00ef7bce,
    0x00421c84, 0x00439ce4, 0x00423dce, 0x00e73c84, 0x00e73dce, 0x0047bdee,
    0x00e7bde4, 0x00e7bdee, 0x00427c00, 0x0043fce0, 0x004e7f80, 0x004fffe0,
    0x004fffe0, 0x00e7fde0, 0x006f7fc0, 0x00efffe0, 0x00007c84, 0x0003fce4,
    0x000e7f84, 0x000fffe4, 0x00007dce, 0x0007fdee, 0x000f7fce, 0x000fffee,
    0x00427c84, 0x0043fce4, 0x004e7f84, 0x004fffe4, 0x00427dce, 0x00e77c84,
    0x00e77dce, 0x0047fdee, 0x004e7fce, 0x00e7fde4, 0x00ef7f84, 0x004fffee,
    0x00efffe4, 0x00e7fdee, 0x00ef7fce, 0x00efffee, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x000f83e0, 0x00a5294a, 0x004e1380, 0x00a57800,
    0x00ad0bc0, 0x004390e0, 0x00a53c00, 0x00a5a1e0, 0x000e1384, 0x0000794a,
    0x000f0b4a, 0x000390e4, 0x00003d4a, 0x0007a16a, 0x004e1384, 0x00a5694a,
    0x00ad2b4a, 0x004390e4, 0x00a52d4a, 0x00a5a16a, 0x004f83e0, 0x00a57c00,
    0x00ad83e0, 0x000f83e4, 0x00007d4a, 0x000f836a, 0x004f93e4, 0x00a57d4a,
    0x00ad836a, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00001c00, 0x00001084, 0x00007000, 0x00421000,
    0x00039ce0, 0x000039ce, 0x000e7380, 0x00e73800, 0x000e7f80, 0x00e73884,
    0x0003fce0, 0x004239ce
};

static void drawLineChar(QPainter &paint, int x, int y, int w, int h, uint8_t code) {
    int cx = x + w / 2;
    int cy = y + h / 2;
    int ex = x + w - 1;
    int ey = y + h - 1;

    quint32 toDraw = LineChars[code];

    if (toDraw & TopL)
        paint.drawLine(cx - 1, y, cx - 1, cy - 2);
    if (toDraw & TopC)
        paint.drawLine(cx, y, cx, cy - 2);
    if (toDraw & TopR)
        paint.drawLine(cx + 1, y, cx + 1, cy - 2);

    if (toDraw & BotL)
        paint.drawLine(cx - 1, cy + 2, cx - 1, ey);
    if (toDraw & BotC)
        paint.drawLine(cx, cy + 2, cx, ey);
    if (toDraw & BotR)
        paint.drawLine(cx + 1, cy + 2, cx + 1, ey);

    if (toDraw & LeftT)
        paint.drawLine(x, cy - 1, cx - 2, cy - 1);
    if (toDraw & LeftC)
        paint.drawLine(x, cy, cx - 2, cy);
    if (toDraw & LeftB)
        paint.drawLine(x, cy + 1, cx - 2, cy + 1);

    if (toDraw & RightT)
        paint.drawLine(cx + 2, cy - 1, ex, cy - 1);
    if (toDraw & RightC)
        paint.drawLine(cx + 2, cy, ex, cy);
    if (toDraw & RightB)
        paint.drawLine(cx + 2, cy + 1, ex, cy + 1);

    if (toDraw & Int11)
        paint.drawPoint(cx - 1, cy - 1);
    if (toDraw & Int12)
        paint.drawPoint(cx, cy - 1);
    if (toDraw & Int13)
        paint.drawPoint(cx + 1, cy - 1);

    if (toDraw & Int21)
        paint.drawPoint(cx - 1, cy);
    if (toDraw & Int22)
        paint.drawPoint(cx, cy);
    if (toDraw & Int23)
        paint.drawPoint(cx + 1, cy);

    if (toDraw & Int31)
        paint.drawPoint(cx - 1, cy + 1);
    if (toDraw & Int32)
        paint.drawPoint(cx, cy + 1);
    if (toDraw & Int33)
        paint.drawPoint(cx + 1, cy + 1);
}

static void drawOtherChar(QPainter &paint, int x, int y, int w, int h, uchar code) {
    const int cx = x + w / 2;
    const int cy = y + h / 2;
    const int ex = x + w - 1;
    const int ey = y + h - 1;

    if (0x4C <= code && code <= 0x4F) {
        const int xHalfGap = qMax(w / 15, 1);
        const int yHalfGap = qMax(h / 15, 1);
        switch (code) {
        case 0x4D:
            paint.drawLine(x, cy - 1, cx - xHalfGap - 1, cy - 1);
            paint.drawLine(x, cy + 1, cx - xHalfGap - 1, cy + 1);
            paint.drawLine(cx + xHalfGap, cy - 1, ex, cy - 1);
            paint.drawLine(cx + xHalfGap, cy + 1, ex, cy + 1);
            /* Falls through. */
        case 0x4C:
            paint.drawLine(x, cy, cx - xHalfGap - 1, cy);
            paint.drawLine(cx + xHalfGap, cy, ex, cy);
            break;
        case 0x4F:
            paint.drawLine(cx - 1, y, cx - 1, cy - yHalfGap - 1);
            paint.drawLine(cx + 1, y, cx + 1, cy - yHalfGap - 1);
            paint.drawLine(cx - 1, cy + yHalfGap, cx - 1, ey);
            paint.drawLine(cx + 1, cy + yHalfGap, cx + 1, ey);
            /* Falls through. */
        case 0x4E:
            paint.drawLine(cx, y, cx, cy - yHalfGap - 1);
            paint.drawLine(cx, cy + yHalfGap, cx, ey);
            break;
        }
    }

    else if (0x6D <= code && code <= 0x70) {
        const int r = w * 3 / 8;
        const int d = 2 * r;
        switch (code) {
        case 0x6D:
            paint.drawLine(cx, cy + r, cx, ey);
            paint.drawLine(cx + r, cy, ex, cy);
            paint.drawArc(cx, cy, d, d, 90 * 16, 90 * 16);
            break;
        case 0x6E:
            paint.drawLine(cx, cy + r, cx, ey);
            paint.drawLine(x, cy, cx - r, cy);
            paint.drawArc(cx - d, cy, d, d, 0 * 16, 90 * 16);
            break;
        case 0x6F:
            paint.drawLine(cx, y, cx, cy - r);
            paint.drawLine(x, cy, cx - r, cy);
            paint.drawArc(cx - d, cy - d, d, d, 270 * 16, 90 * 16);
            break;
        case 0x70:
            paint.drawLine(cx, y, cx, cy - r);
            paint.drawLine(cx + r, cy, ex, cy);
            paint.drawArc(cx, cy - d, d, d, 180 * 16, 90 * 16);
            break;
        }
    }

    else if (0x71 <= code && code <= 0x73) {
        switch (code) {
        case 0x71:
            paint.drawLine(ex, y, x, ey);
            break;
        case 0x72:
            paint.drawLine(x, y, ex, ey);
            break;
        case 0x73:
            paint.drawLine(ex, y, x, ey);
            paint.drawLine(x, y, ex, ey);
            break;
        }
    }
}

void TerminalDisplay::drawLineCharString(QPainter &painter, int x, int y,
                                         const std::wstring &str,
                                         const Character *attributes) const {
    const QPen &currentPen = painter.pen();

#if !defined(Q_OS_WIN)
    if ((attributes->rendition & RE_BOLD) && _boldIntense) {
        QPen boldPen(currentPen);
        boldPen.setWidth(3);
        painter.setPen(boldPen);
    }
#else
    Q_UNUSED(attributes);
#endif

    for (size_t i = 0; i < str.length(); i++) {
        uint8_t code = static_cast<uint8_t>(str[i] & 0xffU);
        if (LineChars[code])
            drawLineChar(painter, static_cast<int>(x + (_fontWidth * i)), y, _fontWidth, _fontHeight,
                                     code);
        else
            drawOtherChar(painter, static_cast<int>(x + (_fontWidth * i)), y, _fontWidth, _fontHeight,
                                        code);
    }

    painter.setPen(currentPen);
}

void TerminalDisplay::drawLineCharString(QPainter &painter, int x, int y,
                                         wchar_t ch,
                                         const Character *attributes) const {
    const QPen &currentPen = painter.pen();

#if !defined(Q_OS_WIN)
    if ((attributes->rendition & RE_BOLD) && _boldIntense) {
        QPen boldPen(currentPen);
        boldPen.setWidth(3);
        painter.setPen(boldPen);
    }
#else
    Q_UNUSED(attributes);
#endif

    uint8_t code = static_cast<uint8_t>(ch & 0xffU);
    if (LineChars[code])
        drawLineChar(painter, x, y, _fontWidth, _fontHeight, code);
    else
        drawOtherChar(painter, x, y, _fontWidth, _fontHeight, code);

    painter.setPen(currentPen);
}

void TerminalDisplay::setKeyboardCursorShape(
        QTermWidget::KeyboardCursorShape shape) {
    _cursorShape = shape;

    updateCursor();
}

QTermWidget::KeyboardCursorShape TerminalDisplay::keyboardCursorShape() const {
    return _cursorShape;
}

void TerminalDisplay::setKeyboardCursorColor(bool useForegroundColor,
                                             const QColor &color) {
    if (useForegroundColor)
        _cursorColor = QColor();

    else
        _cursorColor = color;
}

QColor TerminalDisplay::keyboardCursorColor() const { return _cursorColor; }

void TerminalDisplay::setOpacity(qreal opacity) {
    _opacity = qBound(static_cast<qreal>(0), opacity, static_cast<qreal>(1));
}

void TerminalDisplay::setBackgroundPixmap(QPixmap *backgroundImage) {
    _backgroundPixmapRef = backgroundImage;
    if (backgroundImage != nullptr) {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    } else {
    }
}

void TerminalDisplay::reloadBackgroundPixmap(void) { update(); }

void TerminalDisplay::setBackgroundImage(const QString &backgroundImage) {
    if (!backgroundImage.isEmpty()) {
        _backgroundImage.load(backgroundImage);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    } else {
        _backgroundImage = QPixmap();
    }
}

void TerminalDisplay::setBackgroundMovie(const QString &backgroundImage) {
    QMovie *movie = nullptr;
    if (!backgroundImage.isEmpty()) {
        movie = new QMovie(backgroundImage);
    }
    if (movie && movie->isValid()) {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    } else {
        if (movie)
            delete movie;
    }
}

void TerminalDisplay::setBackgroundVideo(const QString &backgroundVideo) {
    if (!backgroundVideo.isEmpty()) {
        setAttribute(Qt::WA_OpaquePaintEvent, false);
    } else {
        _backgroundVideoFrame = QPixmap();
    }
}

void TerminalDisplay::setBackgroundMode(BackgroundMode mode) {
    _backgroundMode = mode;
}

void TerminalDisplay::drawBackground(QPainter &painter, const QRect &rect,
                                     const QColor &backgroundColor,
                                     bool useOpacitySetting) {
    QPixmap currentBackgroundImage = _backgroundImage;
    if (useOpacitySetting) {
        QColor color(backgroundColor);
        if (currentBackgroundImage.isNull()) {
            color.setAlphaF(1.0);
        } else {
            color.setAlphaF(_opacity);
        }
        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.fillRect(rect, color);
        painter.restore();
    } else
        painter.fillRect(rect, backgroundColor);
}

void TerminalDisplay::drawCursor(QPainter &painter, const QRect &rect,
                                 const QColor &foregroundColor,
                                 const QColor & /*backgroundColor*/,
                                 bool &invertCharacterColor, bool preedit) {
    QRectF cursorRect = rect;
    cursorRect.setHeight(_fontHeight - _lineSpacing - 1);

    if (!_cursorBlinking) {
        if (_cursorColor.isValid())
            painter.setPen(_cursorColor);
        else
            painter.setPen(foregroundColor);

        if (_cursorShape == Emulation::KeyboardCursorShape::BlockCursor) {
            float penWidth = qMax(1, painter.pen().width());

            if (preedit) {
                cursorRect.setWidth(_fontWidth);
            }

            painter.drawRect(cursorRect.adjusted(penWidth / 2, penWidth / 2, -penWidth / 2, -penWidth / 2));

            if (preedit || hasFocus()) {
                painter.fillRect(cursorRect, _cursorColor.isValid() ? _cursorColor
                                                                                                                        : foregroundColor);
                if (!_cursorColor.isValid()) {
                    invertCharacterColor = true;
                }
            }
        } else if (_cursorShape == Emulation::KeyboardCursorShape::UnderlineCursor)
            painter.drawLine(cursorRect.left(), cursorRect.bottom(),
                                             cursorRect.right(), cursorRect.bottom());
        else if (_cursorShape == Emulation::KeyboardCursorShape::IBeamCursor)
            painter.drawLine(cursorRect.left(), cursorRect.top(), cursorRect.left(),
                                             cursorRect.bottom());
    }
}

void TerminalDisplay::drawCharacters(QPainter &painter, const QRect &rect,
                                     const std::wstring &text,
                                     const Character *style,
                                     bool invertCharacterColor,
                                     bool tooWide) {
    if (_blinking && (style->rendition & RE_BLINK))
        return;

    if (style->rendition & RE_CONCEAL)
        return;

    bool useBold =
            ((style->rendition & RE_BOLD) && _boldIntense) || font().bold();
    const bool useUnderline =
            style->rendition & RE_UNDERLINE || font().underline();
    const bool useItalic = style->rendition & RE_ITALIC || font().italic();
    const bool useStrikeOut =
            style->rendition & RE_STRIKEOUT || font().strikeOut();
    const bool useOverline = style->rendition & RE_OVERLINE || font().overline();

    QFont font = painter.font();
    if (font.bold() != useBold || font.underline() != useUnderline ||
            font.italic() != useItalic || font.strikeOut() != useStrikeOut ||
            font.overline() != useOverline) {
#if !defined(Q_OS_WIN)
        font.setBold(useBold);
#endif
        font.setUnderline(useUnderline);
        font.setItalic(useItalic);
        font.setStrikeOut(useStrikeOut);
        font.setOverline(useOverline);
        painter.setFont(font);
    }

    const CharacterColor &textColor =
            (invertCharacterColor ? style->backgroundColor : style->foregroundColor);
    const QColor color = textColor.color(_colorTable);
    QPen pen = painter.pen();
    if (pen.color() != color) {
        pen.setColor(color);
        painter.setPen(color);
    }

    int font_width = _charWidth->string_font_width(text);
    int width = CharWidth::string_unicode_width(text);
    if (_fix_quardCRT_issue33 && font_width != width) {
        int single_rect_width = rect.width() / width;
        for (size_t i = 0; i < text.length(); i++) {
            wchar_t line_char = text[i];
            if (isLineChar(line_char)) {
                drawLineCharString(painter, static_cast<int>(rect.x() + single_rect_width * i), rect.y(),
                                                     line_char, style);
            } else {
                if (_charWidth->font_width(line_char) !=
                        CharWidth::unicode_width(line_char)) {
                    const QList<uint16_t> right_chars = {0x201C, 0x2018, 0x201A, 0x201B};
                    const QList<uint16_t> center_chars = {0x00D7, 0x00F7, 0x2016};
                    const QList<uint16_t> left_chars = {0x201D, 0x2019, 0x2580, 0x2584, 0x2588};
                    if (right_chars.contains(line_char)) {
                        int offset =
                                single_rect_width * (_charWidth->font_width(line_char) -
                                                                         CharWidth::unicode_width(line_char));
                        painter.save();
                        QRect rightHalfRect(static_cast<int>(rect.x() + single_rect_width * i), rect.y(),
                                                                single_rect_width, _fontHeight);
                        painter.setClipRect(rightHalfRect);
                        painter.drawText(static_cast<int>(rect.x() + single_rect_width * i - offset),
                                                         rect.y() + _fontAscent + _lineSpacing,
                                                         QString::fromWCharArray(&line_char, 1));
                        painter.restore();
                    } else if (center_chars.contains(line_char)) {
                        int offset = single_rect_width *
                                                 (_charWidth->font_width(line_char) -
                                                    CharWidth::unicode_width(line_char)) /
                                                 2;
                        painter.save();
                        QRect rightHalfRect(static_cast<int>(rect.x() + single_rect_width * i), rect.y(),
                                                                single_rect_width, _fontHeight);
                        painter.setClipRect(rightHalfRect);
                        painter.drawText(static_cast<int>(rect.x() + single_rect_width * i - offset),
                                                         rect.y() + _fontAscent + _lineSpacing,
                                                         QString::fromWCharArray(&line_char, 1));
                        painter.restore();
                    } else if (left_chars.contains(line_char)) {
                        QRect rectangle(static_cast<int>(rect.x() + single_rect_width * i), rect.y(),
                                                        single_rect_width, _fontHeight);
                        painter.drawText(rectangle, 0,
                                                         QString::fromWCharArray(&line_char, 1));
                    } else {
                        painter.drawText(static_cast<int>(rect.x() + single_rect_width * i),
                                                         rect.y() + _fontAscent + _lineSpacing,
                                                         QString::fromWCharArray(&line_char, 1));
                    }
                } else {
                    painter.drawText(static_cast<int>(rect.x() + single_rect_width * i),
                                                     rect.y() + _fontAscent + _lineSpacing,
                                                     QString::fromWCharArray(&line_char, 1));
                }
            }
        }
    } else {
        if (isLineCharString(text)) {
            drawLineCharString(painter, rect.x(), rect.y(), text, style);
        } else {
            painter.setLayoutDirection(Qt::LeftToRight);

            if (_bidiEnabled) {
                if (tooWide) {
                    QRect drawRect(rect.topLeft(), rect.size());
                    drawRect.setHeight(rect.height() + _drawTextAdditionHeight);
                    painter.drawText(drawRect, Qt::AlignBottom, QString::fromStdWString(text));
                } else {
                    painter.drawText(rect.x(), rect.y() + _fontAscent + _lineSpacing,
                                    QString::fromStdWString(text));
                }
            } else {
                QRect drawRect(rect.topLeft(), rect.size());
                drawRect.setHeight(rect.height() + _drawTextAdditionHeight);
                painter.drawText(drawRect, Qt::AlignBottom, LTR_OVERRIDE_CHAR + QString::fromStdWString(text));
            }
        }
    }
}

void TerminalDisplay::drawTextFragment(QPainter &painter, const QRect &rect,
                                       const std::wstring &text,
                                       Character* style,
                                       bool tooWide,
                                       bool isSelection) {
    painter.save();

    if (_selectedTextOpacity < 1.0) {
        if (isSelection) {
            CharacterColor f = style->foregroundColor;
            CharacterColor b = style->backgroundColor;
            style->foregroundColor = b;
            style->backgroundColor = f;
        }
    }

    const QColor foregroundColor = style->foregroundColor.color(_colorTable);
    const QColor backgroundColor = style->backgroundColor.color(_colorTable);

    if (backgroundColor != _colorTable[DEFAULT_BACK_COLOR].color) {
        drawBackground(painter, rect, backgroundColor,
                                     false /* do not use transparency */);
    }

    bool invertCharacterColor = false;
    if (style->rendition & RE_CURSOR)
        drawCursor(painter, rect, foregroundColor, backgroundColor,
                             invertCharacterColor);

    drawCharacters(painter, rect, text, style, invertCharacterColor, tooWide);

    painter.restore();

    if (_selectedTextOpacity < 1.0) {
        if (isSelection) {
            painter.save();
            painter.setOpacity(_selectedTextOpacity);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
            painter.setRenderHint(QPainter::Antialiasing, false);
            painter.fillRect(rect, CharacterColor(COLOR_SPACE_DEFAULT, DEFAULT_FORE_COLOR)
                                                     .color(_colorTable));
            painter.restore();
            CharacterColor f = style->foregroundColor;
            CharacterColor b = style->backgroundColor;
            style->foregroundColor = b;
            style->backgroundColor = f;
        }
    }
}

#if 0
/*!
        Set XIM Position
*/
void TerminalDisplay::setCursorPos(const int curx, const int cury) {
    QPoint tL  = contentsRect().topLeft();
    int    tLx = tL.x();
    int    tLy = tL.y();

    int xpos, ypos;
    ypos = _topMargin + tLy + _fontHeight*(cury-1) + _fontAscent;
    xpos = _leftMargin + tLx + _fontWidth*curx;
    _cursorLine = cury;
    _cursorCol = curx;
}
#endif

void TerminalDisplay::scrollImage(int lines, const QRect &screenWindowRegion) {
    if (_outputSuspendedLabel && _outputSuspendedLabel->isVisible())
        return;

    QRect region = screenWindowRegion;
    region.setBottom(qMin(region.bottom(), this->_lines - 2));

    if (lines == 0 || _image == nullptr || !region.isValid() ||
            (region.top() + abs(lines)) >= region.bottom() ||
            this->_lines <= region.height())
        return;

    if (_resizeWidget && _resizeWidget->isVisible())
        _resizeWidget->hide();

    int scrollBarWidth =
            _scrollBar->isHidden() ? 0
            : _scrollBar->style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, _scrollBar)
                    ? 0
                    : _scrollBar->width();
    const int SCROLLBAR_CONTENT_GAP = scrollBarWidth == 0 ? 0 : 1;
    QRect scrollRect;
    if (_scrollbarLocation == QTermWidget::ScrollBarLeft) {
        scrollRect.setLeft(scrollBarWidth + SCROLLBAR_CONTENT_GAP);
        scrollRect.setRight(width());
    } else {
        scrollRect.setLeft(0);
        scrollRect.setRight(width() - scrollBarWidth - SCROLLBAR_CONTENT_GAP);
    }
    void *firstCharPos = &_image[region.top() * this->_columns];
    void *lastCharPos = &_image[(region.top() + abs(lines)) * this->_columns];

    int top = _topMargin + (region.top() * _fontHeight);
    int linesToMove = region.height() - abs(lines);
    int bytesToMove = linesToMove * this->_columns * sizeof(Character);

    Q_ASSERT(linesToMove > 0);
    Q_ASSERT(bytesToMove > 0);

    if (lines > 0) {
        Q_ASSERT((char *)lastCharPos + bytesToMove <
                         (char *)(_image + (this->_lines * this->_columns)));

        Q_ASSERT((lines * this->_columns) < _imageSize);

        memmove(firstCharPos, lastCharPos, bytesToMove);

        scrollRect.setTop(top);
    } else {
        Q_ASSERT((char *)firstCharPos + bytesToMove <
                         (char *)(_image + (this->_lines * this->_columns)));

        memmove(lastCharPos, firstCharPos, bytesToMove);

        scrollRect.setTop(top + abs(lines) * _fontHeight);
    }
    scrollRect.setHeight(linesToMove * _fontHeight);

    Q_ASSERT(scrollRect.isValid() && !scrollRect.isEmpty());

    scroll(0, _fontHeight * (-lines), scrollRect);
}

QRegion TerminalDisplay::hotSpotRegion() const {
    QRegion region;
    const auto hotSpots = _filterChain->hotSpots();
    for (Filter::HotSpot *const hotSpot : hotSpots) {
        QRect r;
        if (hotSpot->startLine() == hotSpot->endLine()) {
            r.setLeft(hotSpot->startColumn());
            r.setTop(hotSpot->startLine());
            r.setRight(hotSpot->endColumn());
            r.setBottom(hotSpot->endLine());
            region |= imageToWidget(r);
            ;
        } else {
            r.setLeft(hotSpot->startColumn());
            r.setTop(hotSpot->startLine());
            r.setRight(_columns);
            r.setBottom(hotSpot->startLine());
            region |= imageToWidget(r);
            ;
            for (int line = hotSpot->startLine() + 1; line < hotSpot->endLine();
                     line++) {
                r.setLeft(0);
                r.setTop(line);
                r.setRight(_columns);
                r.setBottom(line);
                region |= imageToWidget(r);
                ;
            }
            r.setLeft(0);
            r.setTop(hotSpot->endLine());
            r.setRight(hotSpot->endColumn());
            r.setBottom(hotSpot->endLine());
            region |= imageToWidget(r);
            ;
        }
    }
    return region;
}

void TerminalDisplay::processFilters() {
    if (!_screenWindow)
        return;

    QRegion preUpdateHotSpots = hotSpotRegion();

    _filterChain->setImage(
            _screenWindow->getImage(), _screenWindow->windowLines(),
            _screenWindow->windowColumns(), _screenWindow->getLineProperties());
    _filterChain->process();

    QRegion postUpdateHotSpots = hotSpotRegion();

    update(preUpdateHotSpots | postUpdateHotSpots);
}

void TerminalDisplay::updateImage() {
    if (!_screenWindow)
        return;

    scrollImage(_screenWindow->scrollCount(), _screenWindow->scrollRegion());
    _screenWindow->resetScrollCount();

    if (!_image) {
        updateImageSize();
    }

    Character *const newimg = _screenWindow->getImage();
    int lines = _screenWindow->windowLines();
    int columns = _screenWindow->windowColumns();

    setScroll(_screenWindow->currentLine(), _screenWindow->lineCount());

    Q_ASSERT(this->_usedLines <= this->_lines);
    Q_ASSERT(this->_usedColumns <= this->_columns);

    int y, x, len;

    QPoint tL = contentsRect().topLeft();
    int tLx = tL.x();
    int tLy = tL.y();
    _hasBlinker = false;

    CharacterColor cf;
    CharacterColor _clipboard;
    int cr = -1;

    const int linesToUpdate = qMin(this->_lines, qMax(0, lines));
    const int columnsToUpdate = qMin(this->_columns, qMax(0, columns));

    wchar_t *disstrU = new wchar_t[columnsToUpdate];
    char *dirtyMask = new char[columnsToUpdate + 2];
    QRegion dirtyRegion;

    for (y = 0; y < linesToUpdate; ++y) {
        const Character *currentLine = &_image[y * this->_columns];
        const Character *const newLine = &newimg[y * columns];

        bool updateLine = false;

        memset(dirtyMask, 0, columnsToUpdate + 2);

        for (x = 0; x < columnsToUpdate; ++x) {
            if (newLine[x] != currentLine[x]) {
                dirtyMask[x] = true;
            }
        }

        QFontMetrics fm(font());
        if (!_resizing)
            for (x = 0; x < columnsToUpdate; ++x) {
                if ((newLine[x].rendition & RE_BLINK) != 0) {
                    _hasBlinker = true;
                }

                if (dirtyMask[x]) {
                    wchar_t c = newLine[x + 0].character;
                    if (!c)
                        continue;
                    int p = 0;
                    disstrU[p++] = c;
                    bool lineDraw = isLineChar(newLine[x+0]);
                    bool doubleWidth = (x + 1 == columnsToUpdate)
                                                                 ? false
                                                                 : (newLine[x + 1].character == 0);
                    int charWidth = fm.horizontalAdvance(QString::fromWCharArray(&c, 1));
                    bool bigWidth = _fixedFont && !doubleWidth && charWidth > _fontWidth;
                    bool smallWidth = _fixedFont && charWidth < _fontWidth;
                    cr = newLine[x].rendition;
                    _clipboard = newLine[x].backgroundColor;
                    if (newLine[x].foregroundColor != cf)
                        cf = newLine[x].foregroundColor;
                    int lln = columnsToUpdate - x;
                    for (len = 1; len < lln; ++len) {
                        const Character &ch = newLine[x + len];

                        if (!ch.character)
                            continue;

                        bool nextIsDoubleWidth =
                                (x + len + 1 == columnsToUpdate)
                                        ? false
                                        : (newLine[x + len + 1].character == 0);

                        int nxtCharWidth = fm.horizontalAdvance(QString::fromWCharArray(&newLine[x+len].character, 1));
                        bool nextIsbigWidth = _fixedFont && !nextIsDoubleWidth && nxtCharWidth > _fontWidth;
                        bool nextIsSmallWidth = _fixedFont && newLine[x+len].character && nxtCharWidth < _fontWidth;

                        if (ch.foregroundColor != cf ||
                            ch.backgroundColor != _clipboard ||
                            ch.rendition != cr ||
                            !dirtyMask[x+len] ||
                            isLineChar(ch) != lineDraw ||
                            nextIsDoubleWidth != doubleWidth ||
                            bigWidth || nextIsbigWidth ||
                            smallWidth || nextIsSmallWidth) {
                            break;
                        }

                        disstrU[p++] = c;
                    }

                    std::wstring unistr(disstrU, p);

                    bool saveFixedFont = _fixedFont;
                    if (lineDraw)
                        _fixedFont = false;
                    if (doubleWidth)
                        _fixedFont = false;

                    updateLine = true;

                    _fixedFont = saveFixedFont;
                    x += len - 1;
                }
            }

        if (_lineProperties.count() > y) {
            if ((_lineProperties[y] & LINE_DOUBLEHEIGHT) != 0) {
                updateLine = true;
            }
        }

        if (updateLine) {
            QRect dirtyRect =
                    QRect(_leftMargin + tLx, _topMargin + tLy + _fontHeight * y,
                                _fontWidth * columnsToUpdate, _fontHeight);

            dirtyRegion |= dirtyRect;
        }

        memcpy((void *)currentLine, (const void *)newLine,
                     columnsToUpdate * sizeof(Character));
    }

    if (linesToUpdate < _usedLines) {
        dirtyRegion |=
                QRect(_leftMargin + tLx, _topMargin + tLy + _fontHeight * linesToUpdate,
                            _fontWidth * this->_columns,
                            _fontHeight * (_usedLines - linesToUpdate));
    }
    _usedLines = linesToUpdate;

    if (columnsToUpdate < _usedColumns) {
        dirtyRegion |=
                QRect(_leftMargin + tLx + columnsToUpdate * _fontWidth,
                            _topMargin + tLy, _fontWidth * (_usedColumns - columnsToUpdate),
                            _fontHeight * this->_lines);
    }
    _usedColumns = columnsToUpdate;

    dirtyRegion |= _inputMethodData.previousPreeditRect;

    update(dirtyRegion);

    if (_hasBlinker && !_blinkTimer->isActive())
        _blinkTimer->start(TEXT_BLINK_DELAY);
    if (!_hasBlinker && _blinkTimer->isActive()) {
        _blinkTimer->stop();
        _blinking = false;
    }
    delete[] dirtyMask;
    delete[] disstrU;
}

void TerminalDisplay::showResizeNotification() {
    if (_terminalSizeHint && isVisible()) {
        if (_terminalSizeStartup) {
            _terminalSizeStartup = false;
            return;
        }
        if (!_resizeWidget) {
            const QString label = tr("Size: XXX x XXX");
            _resizeWidget = new QLabel(label, this);
            _resizeWidget->setMinimumWidth(
                    _resizeWidget->fontMetrics().horizontalAdvance(label));
            _resizeWidget->setMinimumHeight(_resizeWidget->sizeHint().height());
            _resizeWidget->setAlignment(Qt::AlignCenter);

            _resizeWidget->setStyleSheet(QLatin1String(
                    "background-color:palette(window);border-style:solid;border-width:"
                    "1px;border-color:palette(dark);color:palette(windowText);"));

            _resizeTimer = new QTimer(this);
            _resizeTimer->setSingleShot(true);
            connect(_resizeTimer, &QTimer::timeout, _resizeWidget, &QLabel::hide);
        }
        _resizeWidget->setText(tr("Size: %1 x %2").arg(_columns).arg(_lines));
        _resizeWidget->move((width() - _resizeWidget->width()) / 2,
                                                (height() - _resizeWidget->height()) / 2 + 20);
        _resizeWidget->show();
        _resizeTimer->start(1000);
    }
}

void TerminalDisplay::setBlinkingCursor(bool blink) {
    _hasBlinkingCursor = blink;

    if (blink && !_blinkCursorTimer->isActive() && hasFocus()) {
        _blinkCursorTimer->start(std::max(QApplication::cursorFlashTime(), 1000) / 2);
    }

    if (!blink && _blinkCursorTimer->isActive()) {
        _blinkCursorTimer->stop();
        if (_cursorBlinking)
            blinkCursorEvent();
        else
            _cursorBlinking = false;
    }
}

void TerminalDisplay::setBlinkingTextEnabled(bool blink) {
    _allowBlinkingText = blink;

    if (blink && !_blinkTimer->isActive() && hasFocus())
        _blinkTimer->start(TEXT_BLINK_DELAY);

    if (!blink && _blinkTimer->isActive()) {
        _blinkTimer->stop();
        _blinking = false;
    }
}

void TerminalDisplay::focusOutEvent(QFocusEvent *) {
    _cursorBlinking = false;
    updateCursor();
    _blinkCursorTimer->stop();

    if (_blinking)
        blinkEvent();

    _blinkTimer->stop();

    Q_EMIT termLostFocus();
}
void TerminalDisplay::focusInEvent(QFocusEvent *) {
    if (_hasBlinkingCursor) {
        _blinkCursorTimer->start(std::max(QApplication::cursorFlashTime(), 1000) / 2);
    }
    updateCursor();

    if (_hasBlinker)
        _blinkTimer->start(TEXT_BLINK_DELAY);

    Q_EMIT termGetFocus();
}

void TerminalDisplay::enterEvent(QEnterEvent* event)
{
  if (gs_deadSpot.x() < 0 && _hideMouseTimer
      && !_scrollBar->rect().contains(_scrollBar->mapFromParent(event->position().toPoint())))
  {
    gs_futureDeadSpot = event->position().toPoint();
    _hideMouseTimer->start(_mouseAutohideDelay);
  }
  QWidget::enterEvent(event);
}

void TerminalDisplay::leaveEvent(QEvent* event)
{
  if (gs_deadSpot.x() > -1)
  {
    gs_deadSpot = QPoint(-1,-1);
    QApplication::restoreOverrideCursor();
  }
  QWidget::leaveEvent(event);
}

void TerminalDisplay::paintEvent(QPaintEvent *pe) {
    QPainter paint(this);
    QRect cr = contentsRect();

    QPixmap currentBackgroundImage = _backgroundImage;

    if (!currentBackgroundImage.isNull()) {
        QColor background = _colorTable[DEFAULT_BACK_COLOR].color;
        if (_opacity < static_cast<qreal>(1)) {
            background.setAlphaF(_opacity);
            paint.save();
            paint.setCompositionMode(QPainter::CompositionMode_Source);
            paint.fillRect(cr, background);
            paint.restore();
        } else {
            paint.fillRect(cr, background);
        }

        paint.save();
        paint.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

        if (_backgroundMode == Stretch) {
            paint.drawPixmap(cr, currentBackgroundImage, currentBackgroundImage.rect());
        } else if (_backgroundMode == Zoom) {
            QRect r = currentBackgroundImage.rect();
            qreal wRatio = static_cast<qreal>(cr.width()) / r.width();
            qreal hRatio = static_cast<qreal>(cr.height()) / r.height();
            if (wRatio > hRatio) {
                r.setWidth(qRound(r.width() * hRatio));
                r.setHeight(cr.height());
            } else {
                r.setHeight(qRound(r.height() * wRatio));
                r.setWidth(cr.width());
            }
            r.moveCenter(cr.center());
            paint.drawPixmap(r, currentBackgroundImage,
                                             currentBackgroundImage.rect());
        } else if (_backgroundMode == Fit) {
            QRect r = currentBackgroundImage.rect();
            qreal wRatio = static_cast<qreal>(cr.width()) / r.width();
            qreal hRatio = static_cast<qreal>(cr.height()) / r.height();
            if (r.width() > cr.width()) {
                if (wRatio <= hRatio) {
                    r.setHeight(qRound(r.height() * wRatio));
                    r.setWidth(cr.width());
                } else {
                    r.setWidth(qRound(r.width() * hRatio));
                    r.setHeight(cr.height());
                }
            } else if (r.height() > cr.height()) {
                r.setWidth(qRound(r.width() * hRatio));
                r.setHeight(cr.height());
            }
            r.moveCenter(cr.center());
            paint.drawPixmap(r, currentBackgroundImage, currentBackgroundImage.rect());
        } else if (_backgroundMode ==
                             Center) {
            QRect r = currentBackgroundImage.rect();
            r.moveCenter(cr.center());
            paint.drawPixmap(r.topLeft(), currentBackgroundImage);
        } else if (_backgroundMode == Tile) {
            QPixmap scaled = currentBackgroundImage;
            qreal wRatio =
                    static_cast<qreal>(cr.width()) / currentBackgroundImage.width();
            qreal hRatio =
                    static_cast<qreal>(cr.height()) / currentBackgroundImage.height();
            if (wRatio < 1.0 || hRatio < 1.0) {
                if (wRatio > hRatio) {
                    scaled = currentBackgroundImage.scaled(
                            currentBackgroundImage.width() * hRatio,
                            currentBackgroundImage.height() * hRatio);
                } else {
                    scaled = currentBackgroundImage.scaled(
                            currentBackgroundImage.width() * wRatio,
                            currentBackgroundImage.height() * wRatio);
                }
            }
            int x = 0;
            int y = 0;
            while (y < cr.height()) {
                while (x < cr.width()) {
                    paint.drawPixmap(x, y, scaled);
                    x += scaled.width();
                }
                x = 0;
                y += scaled.height();
            }
        } else
        {
            paint.drawPixmap(0, 0, currentBackgroundImage);
        }

        paint.restore();
    }

    if (_drawTextTestFlag) {
        calDrawTextAdditionHeight(paint);
    }

    const QRegion regToDraw = pe->region() & cr;
    for (auto rect = regToDraw.begin(); rect != regToDraw.end(); rect++) {
        drawBackground(paint, *rect, _colorTable[DEFAULT_BACK_COLOR].color,
                                     true /* use opacity setting */);
        drawContents(paint, *rect);
    }
    drawInputMethodPreeditString(paint, preeditRect());

    if (_isLocked) {
        paint.save();
        paint.setOpacity(0.3);
        paint.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        paint.drawPixmap(cr, _lockbackgroundImage, _lockbackgroundImage.rect());
        paint.restore();
    }

    paintFilters(paint);
}

QPoint TerminalDisplay::cursorPosition() const {
    if (_screenWindow)
        return _screenWindow->cursorPosition();
    else
        return {0, 0};
}

QRect TerminalDisplay::preeditRect() const {
    const int preeditLength =
            CharWidth::string_unicode_width(_inputMethodData.preeditString);

    if (preeditLength == 0)
        return {};

    return QRect(_leftMargin + _fontWidth * cursorPosition().x(),
                             _topMargin + _fontHeight * cursorPosition().y(),
                             _fontWidth * preeditLength, _fontHeight);
}

void TerminalDisplay::drawInputMethodPreeditString(QPainter &painter, const QRect &rect) {
    if (_inputMethodData.preeditString.empty())
        return;

    bool invertColors = false;
    QColor background = _colorTable[DEFAULT_BACK_COLOR].color;
    QColor foreground = _colorTable[DEFAULT_FORE_COLOR].color;
    Character style;
    style.character = ' ';
    style.foregroundColor =
            CharacterColor(COLOR_SPACE_RGB, _colorTable[_preeditColorIndex].color);
    style.backgroundColor =
            CharacterColor(COLOR_SPACE_RGB, _colorTable[DEFAULT_BACK_COLOR].color);
    style.rendition = DEFAULT_RENDITION;
    drawBackground(painter, rect, background, true);
    drawCursor(painter, rect, foreground, background, invertColors, true);
    invertColors = false;
    drawCharacters(painter, rect, _inputMethodData.preeditString, &style,
                                 invertColors);

    _inputMethodData.previousPreeditRect = rect;
}

FilterChain *TerminalDisplay::filterChain() const { return _filterChain; }

void TerminalDisplay::paintFilters(QPainter &painter) {
    QPoint cursorPos = mapFromGlobal(QCursor::pos());
    int cursorLine;
    int cursorColumn;
    int leftMargin = _leftBaseMargin +
                    ((_scrollbarLocation == QTermWidget::ScrollBarLeft &&
                        !_scrollBar->style()->styleHint(
                            QStyle::SH_ScrollBar_Transient, nullptr, _scrollBar))
                            ? _scrollBar->width()
                            : 0);

    getCharacterPosition(cursorPos, cursorLine, cursorColumn);
    Character cursorCharacter = _image[loc(cursorColumn, cursorLine)];

    painter.setPen(QPen(cursorCharacter.foregroundColor.color(colorTable())));

    QList<Filter::HotSpot *> spots = _filterChain->hotSpots();
    QListIterator<Filter::HotSpot *> iter(spots);
    while (iter.hasNext()) {
        Filter::HotSpot *spot = iter.next();

        QRegion region;
        if (spot->type() == Filter::HotSpot::Link) {
            QRect r;
            if (spot->startLine() == spot->endLine()) {
                r.setCoords(spot->startColumn() * _fontWidth + 1 + leftMargin,
                                spot->startLine() * _fontHeight + 1 + _topBaseMargin,
                                spot->endColumn() * _fontWidth - 1 + leftMargin,
                                (spot->endLine() + 1) * _fontHeight - 1 + _topBaseMargin);
                region |= r;
            } else {
                r.setCoords(spot->startColumn() * _fontWidth + 1 + leftMargin,
                                spot->startLine() * _fontHeight + 1 + _topBaseMargin,
                                _columns * _fontWidth - 1 + leftMargin,
                                (spot->startLine() + 1) * _fontHeight - 1 + _topBaseMargin);
                region |= r;
                for (int line = spot->startLine() + 1; line < spot->endLine(); line++) {
                    r.setCoords(0 * _fontWidth + 1 + leftMargin,
                                    line * _fontHeight + 1 + _topBaseMargin,
                                    _columns * _fontWidth - 1 + leftMargin,
                                    (line + 1) * _fontHeight - 1 + _topBaseMargin);
                    region |= r;
                }
                r.setCoords(0 * _fontWidth + 1 + leftMargin,
                                spot->endLine() * _fontHeight + 1 + _topBaseMargin,
                                spot->endColumn() * _fontWidth - 1 + leftMargin,
                                (spot->endLine() + 1) * _fontHeight - 1 + _topBaseMargin);
                region |= r;
            }
        }

        for (int line = spot->startLine(); line <= spot->endLine(); line++) {
            int startColumn = 0;
            int endColumn = _columns - 1;
            do {
                if (endColumn <= 0)
                    break;
                uint64_t ucode = _image[loc(startColumn, line)].character;
                if (ucode > 0xffff)
                    break;
                if (QChar(_image[loc(startColumn, line)].character).isSpace())
                    break;
                endColumn--;
            } while (true);

            endColumn++;

            if (line == spot->startLine())
                startColumn = spot->startColumn();
            if (line == spot->endLine())
                endColumn = spot->endColumn();

            QRect r;
            r.setCoords(startColumn * _fontWidth + 1 + leftMargin,
                                    line * _fontHeight + 1 + _topBaseMargin,
                                    endColumn * _fontWidth - 1 + leftMargin,
                                    (line + 1) * _fontHeight - 1 + _topBaseMargin);
            if (spot->type() == Filter::HotSpot::Link) {
                QFontMetrics metrics(font());

                int baseline = r.bottom() - metrics.descent();
                int underlinePos = baseline + metrics.underlinePos();
                if (region.contains(mapFromGlobal(QCursor::pos()))) {
                    painter.drawLine(r.left(), underlinePos, r.right(), underlinePos);
                }
            }
            else if (spot->type() == Filter::HotSpot::Marker) {
                QColor markerColor = spot->color();
                markerColor.setAlpha(120);
                painter.fillRect(r, markerColor);
            }
        }
    }
}

int TerminalDisplay::textWidth(const int startColumn, const int length, const int line) const {
    QFontMetrics fm(font());
    int result = 0;
    for (int column = 0; column < length; column++) {
        auto c = _image[loc(startColumn + column, line)];
        if (_fixedFont_original && !isLineChar(c)) { 
            result += fm.horizontalAdvance(QLatin1Char(REPCHAR[0]));
        } else {
            result += fm.horizontalAdvance(QChar(static_cast<uint>(c.character)));
        }
    }
    return result;
}

QRect TerminalDisplay::calculateTextArea(int topLeftX, int topLeftY,
                                            int startColumn, int line,
                                            int length) {
    int left =
            _fixedFont ? _fontWidth * startColumn : textWidth(0, startColumn, line);
    int top = _fontHeight * line;
    int width =
            _fixedFont ? _fontWidth * length : textWidth(startColumn, length, line);
    return {_leftMargin + topLeftX + left, _topMargin + topLeftY + top, width,
                    _fontHeight};
}

void TerminalDisplay::drawContents(QPainter &paint, const QRect &rect) {
    QPoint tL = contentsRect().topLeft();
    int tLx = tL.x();
    int tLy = tL.y();

    int lux = qMin(_usedColumns - 1, qMax(0, (rect.left() - tLx - _leftMargin) / _fontWidth));
    int luy = qMin(_usedLines - 1, qMax(0, (rect.top() - tLy - _topMargin) / _fontHeight));
    int rlx = qMin(_usedColumns - 1, qMax(0, (rect.right() - tLx - _leftMargin) / _fontWidth));
    int rly = qMin(_usedLines - 1, qMax(0, (rect.bottom() - tLy - _topMargin) / _fontHeight));

    QFontMetrics fm(font());
    const int numberOfColumns = _usedColumns;
    std::wstring unistr;
    unistr.reserve(numberOfColumns);
    for (int y = luy; y <= rly; y++) {
        quint32 c = _image[loc(lux, y)].character;
        int x = lux;
        if (!c && x)
            x--;
        for (; x <= rlx; x++) {
            int len = 1;
            int p = 0;

            int bufferSize = numberOfColumns;
            unistr.resize(bufferSize);

            if (_image[loc(x, y)].rendition & RE_EXTENDED_CHAR) {
                ushort extendedCharLength = 0;
                uint* chars = ExtendedCharTable::instance
                                .lookupExtendedChar(_image[loc(x,y)].character,extendedCharLength);
                if (chars) {
                    Q_ASSERT(extendedCharLength > 1);
                    bufferSize += extendedCharLength - 1;
                    unistr.resize(bufferSize);
                    for ( int index = 0 ; index < extendedCharLength ; index++ ) {
                        Q_ASSERT( p < bufferSize );
                        unistr[p++] = chars[index];
                    }
                }
            } else {
                c = _image[loc(x, y)].character;
                if (c) {
                    Q_ASSERT(p < bufferSize);
                    unistr[p++] = c;
                }
            }

            bool lineDraw = isLineChar(_image[loc(x,y)]);
            bool doubleWidth =
                    (_image[qMin(loc(x, y) + 1, _imageSize)].character == 0);
            int charWidth = fm.horizontalAdvance(QString::fromWCharArray((wchar_t *)&c, 1));
            bool bigWidth = _fixedFont && !doubleWidth && charWidth > _fontWidth;
            bool tooWide = bigWidth && charWidth >= 2 * _fontWidth;
            bool smallWidth = _fixedFont && c && charWidth < _fontWidth;
            CharacterColor currentForeground = _image[loc(x, y)].foregroundColor;
            CharacterColor currentBackground = _image[loc(x, y)].backgroundColor;
            quint8 currentRendition = _image[loc(x, y)].rendition;
            
            quint32 nxtC = 0;
            bool nxtDoubleWidth = false;
            int nxtCharWidth = 0;
            while (x + len <= rlx &&
                        _image[loc(x + len, y)].foregroundColor == currentForeground &&
                        _image[loc(x + len, y)].backgroundColor == currentBackground &&
                        _image[loc(x + len, y)].rendition == currentRendition &&
                        (nxtDoubleWidth = (_image[qMin(loc(x+len,y)+1,_imageSize)].character == 0)) == doubleWidth &&
                        !smallWidth &&
                        !(_fixedFont && (nxtC = _image[loc(x+len,y)].character) && (nxtCharWidth = fm.horizontalAdvance(QString::fromWCharArray((const wchar_t *)(&nxtC), 1))) < _fontWidth) &&
                        !bigWidth &&
                        !(_fixedFont && !nxtDoubleWidth && nxtC && nxtCharWidth > _fontWidth) &&
                        isLineChar(_image[loc(x+len,y)]) == lineDraw)
            {
                c = _image[loc(x+len,y)].character;
                if (_image[loc(x+len,y)].rendition & RE_EXTENDED_CHAR) {
                    ushort extendedCharLength = 0;
                    const uint* chars = ExtendedCharTable::instance.lookupExtendedChar(c, extendedCharLength);
                    if (chars) {
                        Q_ASSERT(extendedCharLength > 1);
                        bufferSize += extendedCharLength - 1;
                        unistr.resize(bufferSize);
                        for ( int index = 0 ; index < extendedCharLength ; index++ ) {
                            Q_ASSERT( p < bufferSize );
                            unistr[p++] = chars[index];
                        }
                    }
                } else {
                    if (c) {
                        Q_ASSERT( p < bufferSize );
                        unistr[p++] = c;
                    }
                }
                if (doubleWidth)
                    len++;
                len++;
            }
            if ((x + len < _usedColumns) && (!_image[loc(x + len, y)].character))
                len++;

            bool save__fixedFont = _fixedFont;
            if (lineDraw)
                _fixedFont = false;
            unistr.resize(p);

            QTransform textScale;

            if (y < _lineProperties.size()) {
                if (_lineProperties[y] & LINE_DOUBLEWIDTH)
                    textScale.scale(2, 1);

                if (_lineProperties[y] & LINE_DOUBLEHEIGHT)
                    textScale.scale(1, 2);
            }

            paint.setWorldTransform(textScale, true);

            QRect textArea = calculateTextArea(tLx, tLy, x, y, len);

            textArea.moveTopLeft(textScale.inverted().map(textArea.topLeft()));

            drawTextFragment(paint, textArea, unistr, &_image[loc(x, y)], tooWide, _screenWindow->isSelected(x, y));

            _fixedFont = save__fixedFont;

            paint.setWorldTransform(textScale.inverted(), true);

            if (y < _lineProperties.size() - 1) {
                if (_lineProperties[y] & LINE_DOUBLEHEIGHT)
                    y++;
            }

            x += len - 1;
        }
    }
}

void TerminalDisplay::blinkEvent() {
    if (!_allowBlinkingText)
        return;

    _blinking = !_blinking;

    update();
}

QRect TerminalDisplay::imageToWidget(const QRect &imageArea) const {
    QRect result;
    result.setLeft(_leftMargin + _fontWidth * imageArea.left());
    result.setTop(_topMargin + _fontHeight * imageArea.top());
    result.setWidth(_fontWidth * imageArea.width());
    result.setHeight(_fontHeight * imageArea.height());

    return result;
}

void TerminalDisplay::updateCursor() {
    QRect cursorRect = imageToWidget(QRect(cursorPosition(), QSize(1, 1)));
    update(cursorRect);
}

void TerminalDisplay::blinkCursorEvent() {
    _cursorBlinking = !_cursorBlinking;
    updateCursor();
}


void TerminalDisplay::resizeEvent(QResizeEvent *) {
    updateImageSize();
    processFilters();
}

void TerminalDisplay::propagateSize() {
    if (_isFixedSize) {
        setSize(_columns, _lines);
        QWidget::setFixedSize(sizeHint());
        parentWidget()->adjustSize();
        parentWidget()->setFixedSize(parentWidget()->sizeHint());
        return;
    }
    if (_image)
        updateImageSize();
}

void TerminalDisplay::updateImageSize() {
    Character *oldimg = _image;
    int oldlin = _lines;
    int oldcol = _columns;

    makeImage();

    int lines = qMin(oldlin, _lines);
    int columns = qMin(oldcol, _columns);

    if (oldimg) {
        for (int line = 0; line < lines; line++) {
            memcpy((void *)&_image[_columns * line], (void *)&oldimg[oldcol * line],
                         columns * sizeof(Character));
        }
        delete[] oldimg;
    }

    if (_screenWindow)
        _screenWindow->setWindowLines(_lines);

    _resizing = (oldlin != _lines) || (oldcol != _columns);

    if (_resizing) {
        if (_showResizeNotificationEnabled)
            showResizeNotification();
        Q_EMIT changedContentSizeSignal(_contentHeight, _contentWidth);
        Q_EMIT changedContentCountSignal(_lines, _columns);
    }

    _resizing = false;
}

void TerminalDisplay::showEvent(QShowEvent *) {
    Q_EMIT changedContentSizeSignal(_contentHeight, _contentWidth);
}

void TerminalDisplay::hideEvent(QHideEvent *) {
    Q_EMIT changedContentSizeSignal(_contentHeight, _contentWidth);
}

void TerminalDisplay::scrollBarPositionChanged(int) {
    if (!_screenWindow)
        return;

    _screenWindow->scrollTo(_scrollBar->value());

    const bool atEndOfOutput = (_scrollBar->value() == _scrollBar->maximum());
    _screenWindow->setTrackOutput(atEndOfOutput);

    updateImage();
}

void TerminalDisplay::setScroll(int cursor, int slines) {
    if (_scrollBar->minimum() == 0 &&
            _scrollBar->maximum() == (slines - _lines) &&
            _scrollBar->value() == cursor) {
        return;
    }

    disconnect(_scrollBar, &QScrollBar::valueChanged, this,
                         &TerminalDisplay::scrollBarPositionChanged);
    _scrollBar->setRange(0, slines - _lines);
    _scrollBar->setSingleStep(1);
    _scrollBar->setPageStep(_lines);
    _scrollBar->setValue(cursor);
    connect(_scrollBar, &QScrollBar::valueChanged, this,
                    &TerminalDisplay::scrollBarPositionChanged);
}

void TerminalDisplay::scrollToEnd() {
    disconnect(_scrollBar, &QScrollBar::valueChanged, this,
                         &TerminalDisplay::scrollBarPositionChanged);
    _scrollBar->setValue(_scrollBar->maximum());
    connect(_scrollBar, &QScrollBar::valueChanged, this,
                    &TerminalDisplay::scrollBarPositionChanged);

    _screenWindow->scrollTo(_scrollBar->value() + 1);
    _screenWindow->setTrackOutput(_screenWindow->atEndOfOutput());
}

void TerminalDisplay::setScrollBarPosition(
        QTermWidget::ScrollBarPosition position) {
    if (_scrollbarLocation == position)
        return;

    if (position == QTermWidget::NoScrollBar)
        _scrollBar->hide();
    else
        _scrollBar->show();

    _topMargin = _leftMargin = 1;
    _scrollbarLocation = position;

    propagateSize();
    update();
}

void TerminalDisplay::mousePressEvent(QMouseEvent *ev) {
    Q_EMIT mousePressEventForwarded(ev);

    if (_possibleTripleClick && (ev->button() == Qt::LeftButton)) {
        mouseTripleClickEvent(ev);
        return;
    }

    if (!contentsRect().contains(ev->pos()))
        return;

    if (!_screenWindow)
        return;

    int charLine;
    int charColumn;
    getCharacterPosition(ev->pos(), charLine, charColumn);
    QPoint pos = QPoint(charColumn, charLine);

    if (ev->button() == Qt::LeftButton) {
        _lineSelectionMode = false;
        _wordSelectionMode = false;

        Q_EMIT isBusySelecting(true);
        bool selected = false;


        selected = _screenWindow->isSelected(pos.x(), pos.y());

        if ((!_ctrlDrag || ev->modifiers() & Qt::ControlModifier) && selected) {
            if ((_mouseMarks) && (ev->modifiers() & Qt::ShiftModifier)) {
                _screenWindow->clearSelection();
                if (shiftSelectionStartX == -1 && shiftSelectionStartY == -1) {
                    shiftSelectionStartX = pos.x();
                    shiftSelectionStartY = pos.y();
                } else {
                    _screenWindow->setSelectionStart(shiftSelectionStartX,
                                                     shiftSelectionStartY,
                                                     ev->modifiers() & Qt::AltModifier);
                    _screenWindow->setSelectionEnd(pos.x(), pos.y());
                }
            } else {
                shiftSelectionStartX = -1;
                shiftSelectionStartY = -1;
                dragInfo.state = diPending;
                dragInfo.start = ev->pos();
            }
        } else {
            dragInfo.state = diNone;

            _preserveLineBreaks = !((ev->modifiers() & Qt::ControlModifier) &&
                                                !(ev->modifiers() & Qt::AltModifier));
            _columnSelectionMode = (ev->modifiers() & Qt::AltModifier) &&
                                                (ev->modifiers() & Qt::ControlModifier);

            if (_mouseMarks) {
                if (ev->modifiers() & Qt::ShiftModifier) {
                    if (_screenWindow->isClearSelection()) {
                        if (shiftSelectionStartX == -1 && shiftSelectionStartY == -1) {
                            shiftSelectionStartX = pos.x();
                            shiftSelectionStartY = pos.y();
                        } else {
                            _screenWindow->setSelectionStart(
                                    shiftSelectionStartX, shiftSelectionStartY,
                                    ev->modifiers() & Qt::AltModifier);
                            _screenWindow->setSelectionEnd(pos.x(), pos.y());
                        }
                    } else {
                        _screenWindow->clearSelection();
                        if (shiftSelectionStartX == -1 && shiftSelectionStartY == -1) {
                            shiftSelectionStartX = pos.x();
                            shiftSelectionStartY = pos.y();
                        } else {
                            _screenWindow->setSelectionStart(
                                    shiftSelectionStartX, shiftSelectionStartY,
                                    ev->modifiers() & Qt::AltModifier);
                            _screenWindow->setSelectionEnd(pos.x(), pos.y());
                        }
                    }
                } else {
                    _screenWindow->clearSelection();
                    shiftSelectionStartX = -1;
                    shiftSelectionStartY = -1;
                    pos.ry() += _scrollBar->value();
                    _iPntSel = _pntSel = pos;
                    _actSel = 1;
                }
            } else {
                if (ev->modifiers() & Qt::ShiftModifier) {
                    _screenWindow->clearSelection();

                    pos.ry() += _scrollBar->value();
                    _iPntSel = _pntSel = pos;
                    _actSel = 1;
                } else {
                    Q_EMIT mouseSignal(
                            0, charColumn + 1,
                            charLine + 1 + _scrollBar->value() - _scrollBar->maximum(), 0);
                }
            }

            if (ev->modifiers() & Qt::ControlModifier) {
                Filter::HotSpot *spot = _filterChain->hotSpotAt(charLine, charColumn);
                if (spot && spot->type() == Filter::HotSpot::Link) {
                    if (spot->hasClickAction()) {
                        spot->clickAction();
                    }
                }
            }
        }
    } else if (ev->button() == Qt::MiddleButton) {
        if (_mouseMarks || (ev->modifiers() & Qt::ShiftModifier))
            emitSelection(true, ev->modifiers() & Qt::ControlModifier);
        else
            Q_EMIT mouseSignal(
                    1, charColumn + 1,
                    charLine + 1 + _scrollBar->value() - _scrollBar->maximum(), 0);
    } else if (ev->button() == Qt::RightButton) {
        if (_mouseMarks || (ev->modifiers() & Qt::ShiftModifier))
            Q_EMIT configureRequest(ev->pos());
        else
            Q_EMIT mouseSignal(
                    2, charColumn + 1,
                    charLine + 1 + _scrollBar->value() - _scrollBar->maximum(), 0);
    }
}

QList<QAction *> TerminalDisplay::filterActions(const QPoint &position) {
    int charLine, charColumn;
    getCharacterPosition(position, charLine, charColumn);

    Filter::HotSpot *spot = _filterChain->hotSpotAt(charLine, charColumn);

    return spot ? spot->actions() : QList<QAction *>();
}

void TerminalDisplay::hideStaleMouse() const
{
    if (gs_deadSpot.x() > -1)
        return;
    if (gs_futureDeadSpot.x() < 0)
        return;
    if (!underMouse())
        return;
    if (QApplication::activeWindow() && QApplication::activeWindow() != window())
        return;
    if (_scrollBar->underMouse())
        return;
    gs_deadSpot = gs_futureDeadSpot;
    QApplication::setOverrideCursor(Qt::BlankCursor);
}

void TerminalDisplay::autoHideMouseAfter(int delay)
{
    if (delay > -1 && !_hideMouseTimer)
    {
        _hideMouseTimer = std::make_shared<QTimer>();
        _hideMouseTimer->setSingleShot(true);
    }
    if ((_mouseAutohideDelay < 0) == (delay < 0))
    {
        _mouseAutohideDelay = delay;
        return;
    }
    if (delay > -1)
        connect(_hideMouseTimer.get(), &QTimer::timeout, this, &TerminalDisplay::hideStaleMouse);
    else if (_hideMouseTimer)
        disconnect(_hideMouseTimer.get(), &QTimer::timeout, this, &TerminalDisplay::hideStaleMouse);
    _mouseAutohideDelay = delay;
}

void TerminalDisplay::mouseMoveEvent(QMouseEvent *ev) {
    if (_mouseAutohideDelay > -1) {
        if (gs_deadSpot.x() > -1 && (ev->pos() - gs_deadSpot).manhattanLength() > 8)
        {
            gs_deadSpot = QPoint(-1,-1);
            QApplication::restoreOverrideCursor();
        }
        gs_futureDeadSpot = ev->position().toPoint();
        Q_ASSERT(_hideMouseTimer);
        _hideMouseTimer->start(_mouseAutohideDelay);
    }

    int charLine = 0;
    int charColumn = 0;
    int leftMargin = _leftBaseMargin +
                    ((_scrollbarLocation == QTermWidget::ScrollBarLeft &&
                        !_scrollBar->style()->styleHint(
                                QStyle::SH_ScrollBar_Transient, nullptr, _scrollBar))
                            ? _scrollBar->width()
                            : 0);

    getCharacterPosition(ev->position().toPoint(), charLine, charColumn);

    Filter::HotSpot *spot = _filterChain->hotSpotAt(charLine, charColumn);
    if (spot && spot->type() == Filter::HotSpot::Link) {
        QRegion previousHotspotArea = _mouseOverHotspotArea;
        _mouseOverHotspotArea = QRegion();
        QRect r;
        if (spot->startLine() == spot->endLine()) {
            r.setCoords(spot->startColumn() * _fontWidth + leftMargin,
                                    spot->startLine() * _fontHeight + _topBaseMargin,
                                    spot->endColumn() * _fontWidth + leftMargin,
                                    (spot->endLine() + 1) * _fontHeight - 1 + _topBaseMargin);
            _mouseOverHotspotArea |= r;
        } else {
            r.setCoords(spot->startColumn() * _fontWidth + leftMargin,
                                    spot->startLine() * _fontHeight + _topBaseMargin,
                                    _columns * _fontWidth - 1 + leftMargin,
                                    (spot->startLine() + 1) * _fontHeight + _topBaseMargin);
            _mouseOverHotspotArea |= r;
            for (int line = spot->startLine() + 1; line < spot->endLine(); line++) {
                r.setCoords(0 * _fontWidth + leftMargin,
                                        line * _fontHeight + _topBaseMargin,
                                        _columns * _fontWidth + leftMargin,
                                        (line + 1) * _fontHeight + _topBaseMargin);
                _mouseOverHotspotArea |= r;
            }
            r.setCoords(0 * _fontWidth + leftMargin,
                                    spot->endLine() * _fontHeight + _topBaseMargin,
                                    spot->endColumn() * _fontWidth + leftMargin,
                                    (spot->endLine() + 1) * _fontHeight + _topBaseMargin);
            _mouseOverHotspotArea |= r;
        }

        update(_mouseOverHotspotArea | previousHotspotArea);

            QToolTip::hideText();
            setCursor(QCursor(_mouseMarks ? Qt::IBeamCursor : Qt::ArrowCursor));
    } else if (!_mouseOverHotspotArea.isEmpty()) {
        update(_mouseOverHotspotArea);
        _mouseOverHotspotArea = QRegion();
        QToolTip::hideText();
        setCursor(QCursor(_mouseMarks ? Qt::IBeamCursor : Qt::ArrowCursor));
    }

    if (ev->buttons() == Qt::NoButton)
        return;

    if (!_mouseMarks && !(ev->modifiers() & Qt::ShiftModifier)) {
        int button = 3;
        if (ev->buttons() & Qt::LeftButton)
            button = 0;
        if (ev->buttons() & Qt::MiddleButton)
            button = 1;
        if (ev->buttons() & Qt::RightButton)
            button = 2;

        Q_EMIT mouseSignal(button, charColumn + 1,
                        charLine + 1 + _scrollBar->value() - _scrollBar->maximum(),
                        1);

        return;
    }

    if (dragInfo.state == diPending) {
        int distance = QApplication::startDragDistance();
        if (ev->position().x() > dragInfo.start.x() + distance ||
                ev->position().x() < dragInfo.start.x() - distance ||
                ev->position().y() > dragInfo.start.y() + distance ||
                ev->position().y() < dragInfo.start.y() - distance) {
            Q_EMIT isBusySelecting(false);

            _screenWindow->clearSelection();
            doDrag();
        }
        return;
    } else if (dragInfo.state == diDragging) {
        return;
    }

    if (_actSel == 0)
        return;

    if (ev->buttons() & Qt::MiddleButton)
        return;

    extendSelection(ev->position().toPoint());
}

void TerminalDisplay::extendSelection(const QPoint &position) {
    QPoint pos = position;

    if (!_screenWindow)
        return;

    QPoint tL = contentsRect().topLeft();
    int tLx = tL.x();
    int tLy = tL.y();
    int scroll = _scrollBar->value();

    int linesBeyondWidget = 0;

    QRect textBounds(tLx + _leftMargin, tLy + _topMargin,
                     _usedColumns * _fontWidth - 1, _usedLines * _fontHeight - 1);

    QPoint oldpos = pos;

    pos.setX(qBound(textBounds.left(), pos.x(), textBounds.right()));
    pos.setY(qBound(textBounds.top(), pos.y(), textBounds.bottom()));

    if (oldpos.y() > textBounds.bottom()) {
        linesBeyondWidget = (oldpos.y() - textBounds.bottom()) / _fontHeight;
        _scrollBar->setValue(_scrollBar->value() + linesBeyondWidget +
                                                 1);
    }
    if (oldpos.y() < textBounds.top()) {
        linesBeyondWidget = (textBounds.top() - oldpos.y()) / _fontHeight;
        _scrollBar->setValue(_scrollBar->value() - linesBeyondWidget -
                                                 1);
    }

    int charColumn = 0;
    int charLine = 0;
    getCharacterPosition(pos, charLine, charColumn);

    QPoint here = QPoint(
            charColumn,
            charLine);
    QPoint ohere;
    QPoint _iPntSelCorr = _iPntSel;
    _iPntSelCorr.ry() -= _scrollBar->value();
    QPoint _pntSelCorr = _pntSel;
    _pntSelCorr.ry() -= _scrollBar->value();
    bool swapping = false;

    if (_wordSelectionMode) {
        int i;
        QChar selClass;

        bool left_not_right =
                (here.y() < _iPntSelCorr.y() ||
                 (here.y() == _iPntSelCorr.y() && here.x() < _iPntSelCorr.x()));
        bool old_left_not_right = (_pntSelCorr.y() < _iPntSelCorr.y() ||
                                    (_pntSelCorr.y() == _iPntSelCorr.y() &&
                                    _pntSelCorr.x() < _iPntSelCorr.x()));
        swapping = left_not_right != old_left_not_right;

        QPoint left = left_not_right ? here : _iPntSelCorr;
        i = loc(left.x(), left.y());
        if (i >= 0 && i <= _imageSize) {
            selClass = charClass(_image[i]);
            while (
                    ((left.x() > 0) ||
                     (left.y() > 0 && (_lineProperties[left.y() - 1] & LINE_WRAPPED))) &&
                    charClass(_image[i-1]) == selClass ) {
                i--;
                if (left.x() > 0)
                    left.rx()--;
                else {
                    left.rx() = _usedColumns - 1;
                    left.ry()--;
                }
            }
        }

        QPoint right = left_not_right ? _iPntSelCorr : here;
        i = loc(right.x(), right.y());
        if (i >= 0 && i <= _imageSize) {
            selClass = charClass(_image[i]);
            while (((right.x() < _usedColumns - 1) ||
                            (right.y() < _usedLines - 1 &&
                             (_lineProperties[right.y()] & LINE_WRAPPED))) &&
                            charClass(_image[i+1]) == selClass) {
                i++;
                if (right.x() < _usedColumns - 1)
                    right.rx()++;
                else {
                    right.rx() = 0;
                    right.ry()++;
                }
            }
        }

        if (left_not_right) {
            here = left;
            ohere = right;
        } else {
            here = right;
            ohere = left;
        }
        ohere.rx()++;
    }

    if (_lineSelectionMode) {
        bool above_not_below = (here.y() < _iPntSelCorr.y());

        QPoint above = above_not_below ? here : _iPntSelCorr;
        QPoint below = above_not_below ? _iPntSelCorr : here;

        while (above.y() > 0 && (_lineProperties[above.y() - 1] & LINE_WRAPPED))
            above.ry()--;
        while (below.y() < _usedLines - 1 &&
                     (_lineProperties[below.y()] & LINE_WRAPPED))
            below.ry()++;

        above.setX(0);
        below.setX(_usedColumns - 1);

        if (above_not_below) {
            here = above;
            ohere = below;
        } else {
            here = below;
            ohere = above;
        }

        QPoint newSelBegin = QPoint(ohere.x(), ohere.y());
        swapping = !(_tripleSelBegin == newSelBegin);
        _tripleSelBegin = newSelBegin;

        ohere.rx()++;
    }

    int offset = 0;
    if (!_wordSelectionMode && !_lineSelectionMode) {
        int i;
        QChar selClass;

        bool left_not_right =
                (here.y() < _iPntSelCorr.y() ||
                 (here.y() == _iPntSelCorr.y() && here.x() < _iPntSelCorr.x()));
        bool old_left_not_right = (_pntSelCorr.y() < _iPntSelCorr.y() ||
                                    (_pntSelCorr.y() == _iPntSelCorr.y() &&
                                    _pntSelCorr.x() < _iPntSelCorr.x()));
        swapping = left_not_right != old_left_not_right;

        QPoint left = left_not_right ? here : _iPntSelCorr;

        QPoint right = left_not_right ? _iPntSelCorr : here;
        if (right.x() > 0 && !_columnSelectionMode) {
            i = loc(right.x(), right.y());
            if (i >= 0 && i <= _imageSize) {
                selClass = charClass(_image[i-1]);
                /* if (selClass == ' ')
                 {
                    while ( right.x() < _usedColumns-1 && charClass(_image[i+1]) == selClass && (right.y()<_usedLines-1) &&
                                    !(_lineProperties[right.y()] & LINE_WRAPPED))
                     { i++; right.rx()++; }
                     if (right.x() < _usedColumns-1)
                         right = left_not_right ? _iPntSelCorr : here;
                     else
                         right.rx()++;
                 }*/
            }
        }

        if (left_not_right) {
            here = left;
            ohere = right;
            offset = 0;
        } else {
            here = right;
            ohere = left;
            offset = -1;
        }
    }

    if ((here == _pntSelCorr) && (scroll == _scrollBar->value()))
        return;

    if (here == ohere)
        return;

    if (_actSel < 2 || swapping) {
        if (_columnSelectionMode && !_lineSelectionMode && !_wordSelectionMode) {
            _screenWindow->setSelectionStart(ohere.x(), ohere.y(), true);
        } else {
            _screenWindow->setSelectionStart(ohere.x() - 1 - offset, ohere.y(), false);
        }
    }

    _actSel = 2;
    _pntSel = here;
    _pntSel.ry() += _scrollBar->value();

    if (_columnSelectionMode && !_lineSelectionMode && !_wordSelectionMode) {
        _screenWindow->setSelectionEnd(here.x(), here.y());
    } else {
        _screenWindow->setSelectionEnd(here.x() + offset, here.y());
    }
}

void TerminalDisplay::mouseReleaseEvent(QMouseEvent *ev) {
    if (!_screenWindow)
        return;

    int charLine;
    int charColumn;
    getCharacterPosition(ev->pos(), charLine, charColumn);

    if (ev->button() == Qt::LeftButton) {
        Q_EMIT isBusySelecting(false);
        if (dragInfo.state == diPending) {
            _screenWindow->clearSelection();
        } else {
            if (_actSel > 1) {
                setSelection(_screenWindow->selectedText(_preserveLineBreaks));
            }

            _actSel = 0;

            if (!_mouseMarks && !(ev->modifiers() & Qt::ShiftModifier))
                Q_EMIT mouseSignal(
                        0, charColumn + 1,
                        charLine + 1 + _scrollBar->value() - _scrollBar->maximum(), 2);
        }
        dragInfo.state = diNone;
    }

    if (!_mouseMarks && ((ev->button() == Qt::RightButton &&
                                                !(ev->modifiers() & Qt::ShiftModifier)) ||
                                             ev->button() == Qt::MiddleButton)) {
        Q_EMIT mouseSignal(ev->button() == Qt::MiddleButton ? 1 : 2, charColumn + 1,
                                         charLine + 1 + _scrollBar->value() - _scrollBar->maximum(),
                                         2);
    }
}

void TerminalDisplay::getCharacterPosition(const QPointF &widgetPoint,
                                           int &line, int &column) const {
    line = (widgetPoint.y() - contentsRect().top() - _topMargin) / _fontHeight;
    if (line < 0)
        line = 0;
    if (line >= _usedLines)
        line = _usedLines - 1;

    int x =
            widgetPoint.x() + _fontWidth / 2 - contentsRect().left() - _leftMargin;
    if (_fixedFont)
        column = x / _fontWidth;
    else {
        column = 0;
        while (column + 1 < _usedColumns && x > textWidth(0, column + 1, line))
            column++;
    }

    if (column < 0)
        column = 0;

    if (column > _usedColumns)
        column = _usedColumns;
}

void TerminalDisplay::updateFilters() {
    if (!_screenWindow)
        return;

    processFilters();
}

void TerminalDisplay::updateLineProperties() {
    if (!_screenWindow)
        return;

    _lineProperties = _screenWindow->getLineProperties();
}

void TerminalDisplay::mouseDoubleClickEvent(QMouseEvent *ev) {
    if (ev->button() != Qt::LeftButton)
        return;
    if (!_screenWindow)
        return;

    int charLine = 0;
    int charColumn = 0;

    getCharacterPosition(ev->pos(), charLine, charColumn);

    QPoint pos(charColumn, charLine);

    if (!_mouseMarks && !(ev->modifiers() & Qt::ShiftModifier)) {
        Q_EMIT mouseSignal(0, pos.x() + 1,pos.y() + 1 + _scrollBar->value() - _scrollBar->maximum(), 0);
        return;
    }

    _screenWindow->clearSelection();
    QPoint bgnSel = pos;
    QPoint endSel = pos;
    int i = loc(bgnSel.x(), bgnSel.y());
    _iPntSel = bgnSel;
    _iPntSel.ry() += _scrollBar->value();

    _wordSelectionMode = true;

    QChar selClass = charClass(_image[i]);
    {
        int x = bgnSel.x();
        while (((x > 0) || (bgnSel.y() > 0 && (_lineProperties[bgnSel.y() - 1] & LINE_WRAPPED))) &&
                     charClass(_image[i-1]) == selClass ) {
            i--;
            if (x > 0)
                x--;
            else {
                x = _usedColumns - 1;
                bgnSel.ry()--;
            }
        }

        bgnSel.setX(x);
        _screenWindow->setSelectionStart(bgnSel.x(), bgnSel.y(), false);

        i = loc(endSel.x(), endSel.y());
        x = endSel.x();
        while (((x < _usedColumns - 1) ||
                (endSel.y() < _usedLines - 1 &&
                (_lineProperties[endSel.y()] & LINE_WRAPPED))) &&
                charClass(_image[i+1]) == selClass ) {
            i++;
            if (x < _usedColumns - 1)
                x++;
            else {
                x = 0;
                endSel.ry()++;
            }
        }

        endSel.setX(x);

        if (QChar(_image[i].character) == QLatin1Char('@') &&
            endSel.x() - bgnSel.x() > 0 &&
            (_image[i].rendition & RE_EXTENDED_CHAR) == 0) {
            endSel.setX( x - 1 );
        }

        _actSel = 2;

        _screenWindow->setSelectionEnd(endSel.x(), endSel.y());

        setSelection(_screenWindow->selectedText(_preserveLineBreaks));
    }

    _possibleTripleClick = true;

    QTimer::singleShot(QApplication::doubleClickInterval(), this, &TerminalDisplay::tripleClickTimeout);
}

void TerminalDisplay::wheelEvent(QWheelEvent *ev) {
    if (ev->angleDelta().y() == 0)
        return;

    if (_mouseMarks && _scrollBar->maximum() > 0) {
        _scrollBar->event(ev);
    } else if (_mouseMarks && !_isPrimaryScreen) {
        int key = ev->angleDelta().y() > 0 ? Qt::Key_Up : Qt::Key_Down;

        int wheelDegrees = ev->angleDelta().y() / 8;
        int linesToScroll = abs(wheelDegrees) / 5;

        QKeyEvent keyScrollEvent(QEvent::KeyPress, key, Qt::NoModifier);

        for (int i = 0; i < linesToScroll; i++)
            Q_EMIT keyPressedSignal(&keyScrollEvent, false);
    } else if (!_mouseMarks) {
        int charLine;
        int charColumn;
        getCharacterPosition(ev->position(), charLine, charColumn);

        Q_EMIT mouseSignal(ev->angleDelta().y() > 0 ? 4 : 5, charColumn + 1,
                        charLine + 1 + _scrollBar->value() - _scrollBar->maximum(), 0);
    }
}

void TerminalDisplay::tripleClickTimeout() { _possibleTripleClick = false; }

void TerminalDisplay::mouseTripleClickEvent(QMouseEvent *ev) {
    if (!_screenWindow)
        return;

    int charLine;
    int charColumn;
    getCharacterPosition(ev->pos(), charLine, charColumn);
    _iPntSel = QPoint(charColumn, charLine);

    _screenWindow->clearSelection();

    _lineSelectionMode = true;
    _wordSelectionMode = false;

    _actSel = 2;
    Q_EMIT isBusySelecting(true);

    while (_iPntSel.y() > 0 && (_lineProperties[_iPntSel.y() - 1] & LINE_WRAPPED))
        _iPntSel.ry()--;

    if (_tripleClickMode == SelectForwardsFromCursor) {
        int i = loc(_iPntSel.x(), _iPntSel.y());
        QChar selClass = charClass(_image[i]);
        int x = _iPntSel.x();

        while (((x > 0) || (_iPntSel.y() > 0 &&
                (_lineProperties[_iPntSel.y() - 1] & LINE_WRAPPED))) &&
                charClass(_image[i-1]) == selClass) {
            i--;
            if (x > 0)
                x--;
            else {
                x = _columns - 1;
                _iPntSel.ry()--;
            }
        }

        _screenWindow->setSelectionStart(x, _iPntSel.y(), false);
        _tripleSelBegin = QPoint(x, _iPntSel.y());
    } else if (_tripleClickMode == SelectWholeLine) {
        _screenWindow->setSelectionStart(0, _iPntSel.y(), false);
        _tripleSelBegin = QPoint(0, _iPntSel.y());
    }

    while (_iPntSel.y() < _lines - 1 &&
                 (_lineProperties[_iPntSel.y()] & LINE_WRAPPED))
        _iPntSel.ry()++;

    _screenWindow->setSelectionEnd(_columns - 1, _iPntSel.y());

    setSelection(_screenWindow->selectedText(_preserveLineBreaks));

    _iPntSel.ry() += _scrollBar->value();
}

bool TerminalDisplay::focusNextPrevChild(bool next) {
    if (next)
        return false;
    return QWidget::focusNextPrevChild(next);
}

QChar TerminalDisplay::charClass(const Character &ch) const {
    if (ch.rendition & RE_EXTENDED_CHAR) {
        ushort extendedCharLength = 0;
        const uint* chars = ExtendedCharTable::instance.lookupExtendedChar(ch.character, extendedCharLength);
        if (chars && extendedCharLength > 0) {
            std::wstring str;
            for (ushort nchar = 0; nchar < extendedCharLength; nchar++) {
                str.push_back(chars[nchar]);
            }
            const QString s = QString::fromStdWString(str);
            if (_wordCharacters.contains(s, Qt::CaseInsensitive))
                return QLatin1Char('a');
            bool allLetterOrNumber = true;
            for (int i = 0; allLetterOrNumber && i < s.size(); ++i)
                allLetterOrNumber = s.at(i).isLetterOrNumber();
            return allLetterOrNumber ? QLatin1Char('a') : s.at(0);
        }
        return QChar(0);
    } else {
        if(ch.character > 0xffff) {
            return QLatin1Char('a');
        }
        const QChar qch(ch.character);
        if (qch.isSpace())
            return QLatin1Char(' ');
        if (qch.isLetterOrNumber() || _wordCharacters.contains(qch, Qt::CaseInsensitive ))
            return QLatin1Char('a');
        return qch;
    }
}

void TerminalDisplay::setWordCharacters(const QString &wc) {
    _wordCharacters = wc.toLatin1();
}

void TerminalDisplay::setUsesMouse(bool on) {
    if (_mouseMarks != on) {
        _mouseMarks = on;
        setCursor(_mouseMarks ? Qt::IBeamCursor : Qt::ArrowCursor);
        Q_EMIT usesMouseChanged();
    }
}
bool TerminalDisplay::usesMouse() const { return _mouseMarks; }

void TerminalDisplay::usingPrimaryScreen(bool use) { _isPrimaryScreen = use; }

void TerminalDisplay::setBracketedPasteMode(bool on) {
    _bracketedPasteMode = on;
}
bool TerminalDisplay::bracketedPasteMode() const { return _bracketedPasteMode; }

#undef KeyPress

void TerminalDisplay::emitSelection(bool useXselection, bool appendReturn) {
    if (!_screenWindow)
        return;

    QString text = QApplication::clipboard()->text(
            useXselection ? QClipboard::Selection : QClipboard::Clipboard);
    if (!text.isEmpty()) {
        text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
        text.replace(QLatin1Char('\n'), QLatin1Char('\r'));

        if (_trimPastedTrailingNewlines) {
            text.replace(QRegularExpression(QStringLiteral("\\r+$")), QString());
        }

        if (_confirmMultilinePaste && text.contains(QLatin1Char('\r'))) {
            if (!multilineConfirmation(text)) {
                return;
            }
        }

        bracketText(text);

        if (appendReturn) {
            text.append(QLatin1Char('\r'));
        }

        QKeyEvent e(QEvent::KeyPress, 0, Qt::NoModifier, text);
        Q_EMIT keyPressedSignal(&e, true);

        _screenWindow->clearSelection();

        switch (mMotionAfterPasting) {
        case MoveStartScreenWindow:
            _screenWindow->setTrackOutput(false);
            _screenWindow->scrollTo(0);
            break;
        case MoveEndScreenWindow:
            scrollToEnd();
            break;
        case NoMoveScreenWindow:
            break;
        }
    }
}

void TerminalDisplay::bracketText(QString &text) const {
    if (bracketedPasteMode() && !_disabledBracketedPasteMode) {
        text.prepend(QLatin1String("\033[200~"));
        text.append(QLatin1String("\033[201~"));
    }
}

bool TerminalDisplay::multilineConfirmation(QString &text) {
    MultilineConfirmationMessageBox confirmation(messageParentWidget);
    confirmation.setWindowTitle(tr("Paste multiline text"));
    confirmation.setText(tr("Are you sure you want to paste this text?"));
    confirmation.setDetailedText(text);
    if (confirmation.exec() == QDialog::Accepted) {
        text = confirmation.getDetailedText();
        return true;
    }
    return false;
}

void TerminalDisplay::setSelection(const QString &t) {
    if (QApplication::clipboard()->supportsSelection()) {
        QApplication::clipboard()->setText(t, QClipboard::Selection);
    }
}

void TerminalDisplay::copyClipboard(QClipboard::Mode mode) {
    if (!_screenWindow)
        return;

    QString text = _screenWindow->selectedText(_preserveLineBreaks);
    if (!text.isEmpty())
        QApplication::clipboard()->setText(text, mode);
}

void TerminalDisplay::pasteClipboard() { emitSelection(false, false); }

void TerminalDisplay::pasteSelection() { emitSelection(true, false); }

void TerminalDisplay::selectAll() {
    if (!_screenWindow)
        return;

    _screenWindow->clearSelection();
    _screenWindow->setSelectionStart(0, 0, false);
    _screenWindow->setSelectionEnd(_columns - 1, _lines - 1);
    setSelection(_screenWindow->selectedText(_preserveLineBreaks));
}

void TerminalDisplay::setConfirmMultilinePaste(bool confirmMultilinePaste) {
    _confirmMultilinePaste = confirmMultilinePaste;
}

void TerminalDisplay::setTrimPastedTrailingNewlines(
        bool trimPastedTrailingNewlines) {
    _trimPastedTrailingNewlines = trimPastedTrailingNewlines;
}

void TerminalDisplay::setFlowControlWarningEnabled(bool enable) {
    _flowControlWarningEnabled = enable;

    if (!enable)
        outputSuspended(false);
}

void TerminalDisplay::setMotionAfterPasting(MotionAfterPasting action) {
    mMotionAfterPasting = action;
}

int TerminalDisplay::motionAfterPasting() { return mMotionAfterPasting; }

void TerminalDisplay::keyPressEvent(QKeyEvent *event) {
    _actSel = 0;
    if (_hasBlinkingCursor) {
        _blinkCursorTimer->start(std::max(QApplication::cursorFlashTime(), 1000) / 2);
        if (_cursorBlinking)
            blinkCursorEvent();
        else
            _cursorBlinking = false;
    }

    Q_EMIT keyPressedSignal(event, false);

    event->accept();
}

void TerminalDisplay::inputMethodEvent(QInputMethodEvent *event) {
    QKeyEvent keyEvent(QEvent::KeyPress, 0, Qt::NoModifier,
                                         event->commitString());
    Q_EMIT keyPressedSignal(&keyEvent, false);

    _inputMethodData.preeditString = event->preeditString().toStdWString();
    update(preeditRect() | _inputMethodData.previousPreeditRect);

    event->accept();
}

QVariant TerminalDisplay::inputMethodQuery(Qt::InputMethodQuery query) const {
    const QPoint cursorPos =
            _screenWindow ? _screenWindow->cursorPosition() : QPoint(0, 0);
    switch (query) {
    case Qt::ImCursorRectangle:
        return imageToWidget(QRect(cursorPos.x(), cursorPos.y(), 1, 1));
        break;
    case Qt::ImFont:
        return font();
        break;
    case Qt::ImCursorPosition:
        return cursorPos.x();
        break;
    case Qt::ImSurroundingText: {
        QString lineText;
        QTextStream stream(&lineText);
        PlainTextDecoder decoder;
        decoder.begin(&stream);
        decoder.decodeLine(&_image[loc(0, cursorPos.y())], _usedColumns, 0);
        decoder.end();
        return lineText;
    } break;
    case Qt::ImCurrentSelection:
        return QString();
        break;
    case Qt::ImHints:
        return (int)inputMethodHints();
        break;
    default:
        break;
    }

    return QVariant();
}

bool TerminalDisplay::handleShortcutOverrideEvent(QKeyEvent *keyEvent) {
    int modifiers = keyEvent->modifiers();

    if (modifiers != Qt::NoModifier) {
        int modifierCount = 0;
        unsigned int currentModifier = Qt::ShiftModifier;

        while (currentModifier <= Qt::KeypadModifier) {
            if (modifiers & currentModifier)
                modifierCount++;
            currentModifier <<= 1;
        }
        if (modifierCount < 2) {
            bool override = false;
            Q_EMIT overrideShortcutCheck(keyEvent, override);
            if (override) {
                keyEvent->accept();
                return true;
            }
        }
    }

    int keyCode = keyEvent->key() | modifiers;
    switch (keyCode) {
    case Qt::Key_Tab:
    case Qt::Key_Delete:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_Backspace:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Escape:
        keyEvent->accept();
        return true;
    }
    return false;
}

bool TerminalDisplay::event(QEvent *event) {
    bool eventHandled = false;
    switch (event->type()) {
    case QEvent::ShortcutOverride:
        eventHandled = handleShortcutOverrideEvent((QKeyEvent *)event);
        break;
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
        _scrollBar->setPalette(QApplication::palette());
        break;
    default:
        break;
    }
    return eventHandled ? true : QWidget::event(event);
}

void TerminalDisplay::setBellMode(int mode) { _bellMode = mode; }

void TerminalDisplay::enableBell() { _allowBell = true; }

void TerminalDisplay::bell() {
    if (_bellMode == NoBell)
        return;

    if (_allowBell) {
        _allowBell = false;
        QTimer::singleShot(500, this, &TerminalDisplay::enableBell);

        if (_bellMode == SystemBeepBell) {
            QApplication::beep();
        } else if (_bellMode == NotifyBell) {
            Q_EMIT notifyBell();
        } else if (_bellMode == VisualBell) {
            swapColorTable();
            QTimer::singleShot(200, this, &TerminalDisplay::swapColorTable);
        }
    }
}

void TerminalDisplay::selectionChanged() {
    Q_EMIT copyAvailable(_screenWindow->selectedText(false).isEmpty() == false);
}

void TerminalDisplay::swapColorTable() {
    ColorEntry color = _colorTable[1];
    _colorTable[1] = _colorTable[0];
    _colorTable[0] = color;
    _colorsInverted = !_colorsInverted;
    update();
}

void TerminalDisplay::clearImage() {
    for (int i = 0; i <= _imageSize; i++) {
        _image[i].character = ' ';
        _image[i].foregroundColor =
                CharacterColor(COLOR_SPACE_DEFAULT, DEFAULT_FORE_COLOR);
        _image[i].backgroundColor =
                CharacterColor(COLOR_SPACE_DEFAULT, DEFAULT_BACK_COLOR);
        _image[i].rendition = DEFAULT_RENDITION;
    }
}

void TerminalDisplay::calcGeometry() {
    _scrollBar->resize(_scrollBar->sizeHint().width(), contentsRect().height());
    int scrollBarWidth = _scrollBar->style()->styleHint(
                        QStyle::SH_ScrollBar_Transient, nullptr, _scrollBar)
                        ? 0
                        : _scrollBar->width();
    switch (_scrollbarLocation) {
    case QTermWidget::NoScrollBar:
        _leftMargin = _leftBaseMargin;
        _contentWidth = contentsRect().width() - 2 * _leftBaseMargin;
        break;
    case QTermWidget::ScrollBarLeft:
        _leftMargin = _leftBaseMargin + scrollBarWidth;
        _contentWidth =
                contentsRect().width() - 2 * _leftBaseMargin - scrollBarWidth;
        _scrollBar->move(contentsRect().topLeft());
        break;
    case QTermWidget::ScrollBarRight:
        _leftMargin = _leftBaseMargin;
        _contentWidth =
                contentsRect().width() - 2 * _leftBaseMargin - scrollBarWidth;
        _scrollBar->move(contentsRect().topRight() -
                                         QPoint(_scrollBar->width() - 1, 0));
        break;
    }

    _topMargin = _topBaseMargin;
    _contentHeight =
            contentsRect().height() - 2 * _topBaseMargin + /* mysterious */ 1;

    if (!_isFixedSize) {
        _columns = qMax(1, _contentWidth / _fontWidth);
        _usedColumns = qMin(_usedColumns, _columns);

        _lines = qMax(1, _contentHeight / _fontHeight);
        _usedLines = qMin(_usedLines, _lines);
    }
}

void TerminalDisplay::makeImage() {
    calcGeometry();

    Q_ASSERT(_lines > 0 && _columns > 0);
    Q_ASSERT(_usedLines <= _lines && _usedColumns <= _columns);

    _imageSize = _lines * _columns;

    _image = new Character[_imageSize + 1];

    clearImage();
}

void TerminalDisplay::setSize(int columns, int lines) {
    int scrollBarWidth =
            (_scrollBar->isHidden() ||
             _scrollBar->style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, _scrollBar))
                    ? 0
                    : _scrollBar->sizeHint().width();
    int horizontalMargin = 2 * _leftBaseMargin;
    int verticalMargin = 2 * _topBaseMargin;

    QSize newSize =
            QSize(horizontalMargin + scrollBarWidth + (columns * _fontWidth),
                        verticalMargin + (lines * _fontHeight));

    if (newSize != size()) {
        _size = newSize;
        updateGeometry();
    }
}

void TerminalDisplay::setFixedSize(int cols, int lins) {
    _isFixedSize = true;

    _columns = qMax(1, cols);
    _lines = qMax(1, lins);
    _usedColumns = qMin(_usedColumns, _columns);
    _usedLines = qMin(_usedLines, _lines);

    if (_image) {
        delete[] _image;
        makeImage();
    }
    setSize(cols, lins);
    QWidget::setFixedSize(_size);
}

QSize TerminalDisplay::sizeHint() const { return _size; }

void TerminalDisplay::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat(QLatin1String("text/plain")))
        event->acceptProposedAction();
    if (event->mimeData()->urls().count())
        event->acceptProposedAction();
}

void TerminalDisplay::dropEvent(QDropEvent *event) {
    QList<QUrl> urls = event->mimeData()->urls();

    QString dropText;
    if (!urls.isEmpty()) {
        for (int i = 0; i < urls.count(); i++) {
            QUrl url = urls[i];

            QString urlText;

            if (url.isLocalFile())
                urlText = url.path();
            else
                urlText = url.toString();

            QChar q(QLatin1Char('\''));
            dropText += q + QString(urlText).replace(q, QLatin1String("'\\''")) + q;
            dropText += QLatin1Char(' ');
        }
    } else {
        dropText = event->mimeData()->text();

        dropText.replace(QLatin1String("\r\n"), QLatin1String("\n"));
        dropText.replace(QLatin1Char('\n'), QLatin1Char('\r'));
        if (_trimPastedTrailingNewlines) {
            dropText.replace(QRegularExpression(QStringLiteral("\\r+$")), QString());
        }
        if (_confirmMultilinePaste && dropText.contains(QLatin1Char('\r'))) {
            if (!multilineConfirmation(dropText)) {
                return;
            }
        }
    }

    Q_EMIT sendStringToEmu(dropText.toLocal8Bit().constData());
}

void TerminalDisplay::doDrag() {
    dragInfo.state = diDragging;
    dragInfo.dragObject = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    mimeData->setText(QApplication::clipboard()->text(QClipboard::Selection));
    dragInfo.dragObject->setMimeData(mimeData);
    dragInfo.dragObject->exec(Qt::CopyAction);
}

void TerminalDisplay::outputSuspended(bool suspended) {
    if (!_outputSuspendedLabel) {
        _outputSuspendedLabel = new QLabel(
                tr("<qt>Output has been "
                     "<a href=\"http://en.wikipedia.org/wiki/Flow_control\">suspended</a>"
                     " by pressing Ctrl+S."
                     "  Press <b>Ctrl+Q</b> to resume.</qt>"),
                this);

        QPalette palette(_outputSuspendedLabel->palette());
        _outputSuspendedLabel->setPalette(palette);
        _outputSuspendedLabel->setAutoFillBackground(true);
        _outputSuspendedLabel->setBackgroundRole(QPalette::Base);
        _outputSuspendedLabel->setFont(QApplication::font());
        _outputSuspendedLabel->setContentsMargins(5, 5, 5, 5);

        _outputSuspendedLabel->setTextInteractionFlags(
                Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
        _outputSuspendedLabel->setOpenExternalLinks(true);
        _outputSuspendedLabel->setVisible(false);

        _gridLayout->addWidget(_outputSuspendedLabel);
        _gridLayout->addItem(
                new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding),
                1, 0);
    }

    _outputSuspendedLabel->setVisible(suspended);
}

uint TerminalDisplay::lineSpacing() const { return _lineSpacing; }

void TerminalDisplay::setLineSpacing(uint i) {
    _lineSpacing = i;
    setVTFont(font());
}

int TerminalDisplay::margin() const { return _topBaseMargin; }

void TerminalDisplay::setMargin(int i) {
    _topBaseMargin = i;
    _leftBaseMargin = i;
}

int TerminalDisplay::getCursorX() const {
    return _screenWindow.isNull() ? 0 : _screenWindow->getCursorX();
}

int TerminalDisplay::getCursorY() const {
    return _screenWindow.isNull() ? 0 : _screenWindow->getCursorY();
}

void TerminalDisplay::setCursorX(int x) {
    if (!_screenWindow.isNull())
        _screenWindow->setCursorX(x);
}

void TerminalDisplay::setCursorY(int y) {
    if (!_screenWindow.isNull())
        _screenWindow->setCursorY(y);
}

QString TerminalDisplay::screenGet(int row1, int col1, int row2, int col2, int mode) {
    return _screenWindow.isNull()
                         ? QString()
                         : _screenWindow->getScreenText(row1, col1, row2, col2, mode);
}

AutoScrollHandler::AutoScrollHandler(QWidget *parent)
        : QObject(parent), _timerId(0) {
    parent->installEventFilter(this);
}
void AutoScrollHandler::timerEvent(QTimerEvent *event) {
    if (event->timerId() != _timerId)
        return;

    QMouseEvent mouseEvent(
            QEvent::MouseMove, widget()->mapFromGlobal(QCursor::pos()),
            QCursor::pos(), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);

    QApplication::sendEvent(widget(), &mouseEvent);
}
bool AutoScrollHandler::eventFilter(QObject *watched, QEvent *event) {
    Q_ASSERT(watched == parent());
    Q_UNUSED(watched);

    QMouseEvent *mouseEvent = (QMouseEvent *)event;
    switch (event->type()) {
    case QEvent::MouseMove: {
        bool mouseInWidget = widget()->rect().contains(mouseEvent->pos());

        if (mouseInWidget) {
            if (_timerId)
                killTimer(_timerId);
            _timerId = 0;
        } else {
            if (!_timerId && (mouseEvent->buttons() & Qt::LeftButton))
                _timerId = startTimer(100);
        }
        break;
    }
    case QEvent::MouseButtonRelease:
        if (_timerId && (mouseEvent->buttons() & ~Qt::LeftButton)) {
            killTimer(_timerId);
            _timerId = 0;
        }
        break;
    default:
        break;
    };

    return false;
}

ScrollBar::ScrollBar(QWidget* parent) : QScrollBar(parent) {}

void ScrollBar::enterEvent(QEnterEvent* event)
{
  if (gs_deadSpot.x() > -1)
  {
    gs_deadSpot = QPoint(-1,-1);
    QApplication::restoreOverrideCursor();
  }
  QScrollBar::enterEvent(event);
}
