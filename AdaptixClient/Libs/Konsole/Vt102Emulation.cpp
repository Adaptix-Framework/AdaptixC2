#include "Vt102Emulation.h"
#include <cstdio>
#include <string>

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QKeyEvent>
#include <QRegularExpression>

#include "util/KeyboardTranslator.h"
#include "Screen.h"

Vt102Emulation::Vt102Emulation()
        : Emulation(), prevCC(0), _titleUpdateTimer(new QTimer(this)),
            _reportFocusEvents(false), _toUtf8(QStringEncoder::Utf8),
            _isTitleChanged(false) {
    _titleUpdateTimer->setSingleShot(true);
    QObject::connect(_titleUpdateTimer, &QTimer::timeout, this, &Vt102Emulation::updateTitle);
    initTokenizer();
    reset();
}

Vt102Emulation::~Vt102Emulation() {}

void Vt102Emulation::clearEntireScreen() {
    _currentScreen->clearEntireScreen();
    bufferedUpdate();
}

void Vt102Emulation::reset() {
    resetTokenizer();
    resetModes();
    resetCharset(0);
    _screen[0]->reset();
    resetCharset(1);
    _screen[1]->reset();

    bufferedUpdate();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                     Processing the incoming byte stream                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/* Incoming Bytes Event pipeline

     This section deals with decoding the incoming character stream.
     Decoding means here, that the stream is first separated into `tokens'
     which are then mapped to a `meaning' provided as operations by the
     `Screen' class or by the emulation class itself.

     The pipeline proceeds as follows:

     - Tokenizing the ESC codes (onReceiveChar)
     - VT100 code page translation of plain characters (applyCharset)
     - Interpretation of ESC codes (processToken)

     The escape codes and their meaning are described in the
     technical reference of this program.
*/

/*
     Since the tokens are the central notion if this section, we've put them
     in front. They provide the syntactical elements used to represent the
     terminals operations as byte sequences.

     They are encodes here into a single machine word, so that we can later
     switch over them easily. Depending on the token itself, additional
     argument variables are filled with parameter values.

     The tokens are defined below:

     - CHR        - Printable characters     (32..255 but DEL (=127))
     - CTL        - Control characters       (0..31 but ESC (= 27), DEL)
     - ESC        - Escape codes of the form <ESC><CHR but `[]()+*#'>
     - ESC_DE     - Escape codes of the form <ESC><any of `()+*#%'> C
     - CSI_PN     - Escape codes of the form <ESC>'['     {Pn} ';' {Pn} C
     - CSI_PS     - Escape codes of the form <ESC>'['     {Pn} ';' ...  C
     - CSI_PS_SP  - Escape codes of the form <ESC>'['     {Pn} ';' ... {Space} C
     - CSI_PR     - Escape codes of the form <ESC>'[' '?' {Pn} ';' ...  C
     - CSI_PE     - Escape codes of the form <ESC>'[' '!' {Pn} ';' ...  C
     - VT52       - VT52 escape codes
                                    - <ESC><Chr>
                                    - <ESC>'Y'{Pc}{Pc}
     - XTE_HA     - Xterm window/terminal attribute commands
                                    of the form <ESC>`]' {Pn} `;' {Text} <BEL>
                                    (Note that these are handled differently to the other formats)

     The last two forms allow list of arguments. Since the elements of
     the lists are treated individually the same way, they are passed
     as individual tokens to the interpretation. Further, because the
     meaning of the parameters are names (although represented as numbers),
     they are includes within the token ('N').

*/

#define TY_CONSTRUCT(T, A, N)                                                  \
    (((((int)N) & 0xffff) << 16) | ((((int)A) & 0xff) << 8) | (((int)T) & 0xff))

#define TY_CHR() TY_CONSTRUCT(0, 0, 0)
#define TY_CTL(A) TY_CONSTRUCT(1, A, 0)
#define TY_ESC(A) TY_CONSTRUCT(2, A, 0)
#define TY_ESC_CS(A, B) TY_CONSTRUCT(3, A, B)
#define TY_ESC_DE(A) TY_CONSTRUCT(4, A, 0)
#define TY_CSI_PS(A, N) TY_CONSTRUCT(5, A, N)
#define TY_CSI_PN(A) TY_CONSTRUCT(6, A, 0)
#define TY_CSI_PR(A, N) TY_CONSTRUCT(7, A, N)
#define TY_CSI_PS_SP(A, N) TY_CONSTRUCT(11, A, N)

#define TY_VT52(A) TY_CONSTRUCT(8, A, 0)
#define TY_CSI_PG(A) TY_CONSTRUCT(9, A, 0)
#define TY_CSI_PE(A) TY_CONSTRUCT(10, A, 0)

#define MAX_ARGUMENT 4096

/* The tokenizer's state

     The state is represented by the buffer (tokenBuffer, tokenBufferPos),
     and accompanied by decoded arguments kept in (argv,argc).
     Note that they are kept internal in the tokenizer.
*/

void Vt102Emulation::resetTokenizer() {
    tokenBufferPos = 0;
    argc = 0;
    argv[0] = 0;
    argv[1] = 0;
    prevCC = 0;
}

void Vt102Emulation::addDigit(int digit) {
    if (argv[argc] < MAX_ARGUMENT)
        argv[argc] = 10 * argv[argc] + digit;
}

void Vt102Emulation::addArgument() {
    argc = qMin(argc + 1, MAXARGS - 1);
    argv[argc] = 0;
}

void Vt102Emulation::addToCurrentToken(wchar_t cc) {
    tokenBuffer[tokenBufferPos] = cc;
    tokenBufferPos = qMin(tokenBufferPos + 1, MAX_TOKEN_LENGTH - 1);
}

#define CTL 1
#define CHR 2
#define CPN 4
#define DIG 8
#define SCS 16
#define GRP 32
#define CPS 64

void Vt102Emulation::initTokenizer() {
    int i;
    quint8 *s;
    for (i = 0; i < 256; ++i)
        charClass[i] = 0;
    for (i = 0; i < 32; ++i)
        charClass[i] |= CTL;
    for (i = 32; i < 256; ++i)
        charClass[i] |= CHR;
    for (s = (quint8 *)"@ABCDEFGHILMPSTXZbcdfry"; *s; ++s)
        charClass[*s] |= CPN;
    for (s = (quint8 *)"t"; *s; ++s)
        charClass[*s] |= CPS;
    for (s = (quint8 *)"0123456789"; *s; ++s)
        charClass[*s] |= DIG;
    for (s = (quint8 *)"()+*%"; *s; ++s)
        charClass[*s] |= SCS;
    for (s = (quint8 *)"()+*#[]%"; *s; ++s)
        charClass[*s] |= GRP;

    resetTokenizer();
}

/* Ok, here comes the nasty part of the decoder.

     Instead of keeping an explicit state, we deduce it from the
     token scanned so far. It is then immediately combined with
     the current character to form a scanning decision.

     This is done by the following defines.

     - P is the length of the token scanned so far.
     - L (often P-1) is the position on which contents we base a decision.
     - C is a character or a group of characters (taken from 'charClass').

     - 'cc' is the current character
     - 's' is a pointer to the start of the token buffer
     - 'p' is the current position within the token buffer

     Note that they need to applied in proper order.
*/

#define lec(P, L, C) (p == (P) && s[(L)] == (C))
#define lun()        (p == 1 && cc >= 32)
#define les(P, L, C) (p == (P) && s[L] < 256 && (charClass[s[(L)]] & (C)) == (C))
#define eec(C)       (p >= 3 && cc == (C))
#define ees(C)       (p >= 3 && cc < 256 && (charClass[cc] & (C)) == (C))
#define eps(C)       (p >= 3 && s[2] != '?' && s[2] != '!' && s[2] != '>' && cc < 256 && (charClass[cc] & (C)) == (C))
#define epp()        (p >= 3 && s[2] == '?')
#define epe()        (p >= 3 && s[2] == '!')
#define egt()        (p >= 3 && s[2] == '>')
#define esp()        (p == 4 && s[3] == ' ')
#define Xpe          (tokenBufferPos >= 2 && tokenBuffer[1] == ']')
#define Xte          (Xpe && (cc == 7 || (prevCC == 27 && cc == 92)))
#define ces(C)       (cc < 256 && (charClass[cc] & (C)) == (C) && !Xte)

#define CNTL(c)      ((c) - '@')
#define ESC 27
#define DEL 127

void Vt102Emulation::receiveChar(wchar_t cc) {
    if ((cc == L'\r') || (cc == L'\n'))
        dupDisplayCharacter(cc);
    if (cc == DEL)
        return;

    if (ces(CTL)) {
        if (Xpe) {
            prevCC = cc;
            return;
        }

        if (cc == CNTL('X') || cc == CNTL('Z') || cc == ESC)
            resetTokenizer();
        if (cc != ESC) {
            processToken(TY_CTL(cc + '@'), 0, 0);
            return;
        }
    }
    addToCurrentToken(cc);

    wchar_t *s = tokenBuffer;
    int p = tokenBufferPos;

    if (getMode(MODE_Ansi)) {
        if (lec(1, 0, ESC)) {
            return;
        }
        if (lec(1, 0, ESC + 128)) {
            s[0] = ESC;
            receiveChar('[');
            return;
        }
        if (les(2, 1, GRP)) {
            return;
        }
        if (Xte) {
            processOSC();
            resetTokenizer();
            return;
        }
        if (Xpe) {
            prevCC = cc;
            return;
        }
        if (lec(3, 2, '?')) {
            return;
        }
        if (lec(3, 2, '>')) {
            return;
        }
        if (lec(3, 2, '!')) {
            return;
        }
        if (lun()) {
            processToken(TY_CHR(), applyCharset(cc), 0);
            resetTokenizer();
            return;
        }
        if (lec(2, 0, ESC)) {
            processToken(TY_ESC(s[1]), 0, 0);
            resetTokenizer();
            return;
        }
        if (les(3, 1, SCS)) {
            processToken(TY_ESC_CS(s[1], s[2]), 0, 0);
            resetTokenizer();
            return;
        }
        if (lec(3, 1, '#')) {
            processToken(TY_ESC_DE(s[2]), 0, 0);
            resetTokenizer();
            return;
        }
        if (eps(CPN)) {
            processToken(TY_CSI_PN(cc), argv[0], argv[1]);
            resetTokenizer();
            return;
        }
        if (esp()) {
            return;
        }
        if (lec(5, 4, 'q') && s[3] == ' ') {
            processToken(TY_CSI_PS_SP(cc, argv[0]), argv[0], 0);
            resetTokenizer();
            return;
        }

        if (eps(CPS)) {
            processToken(TY_CSI_PS(cc, argv[0]), argv[1], argv[2]);
            resetTokenizer();
            return;
        }

        if (epe()) {
            processToken(TY_CSI_PE(cc), 0, 0);
            resetTokenizer();
            return;
        }
        if (ees(DIG)) {
            addDigit(cc - '0');
            return;
        }
        if (eec(';') || eec(':')) {
            addArgument();
            return;
        }
        for (int i = 0; i <= argc; i++) {
            if (epp())
                processToken(TY_CSI_PR(cc, argv[i]), 0, 0);
            else if (egt())
                processToken(TY_CSI_PG(cc), 0, 0);
            else if (cc == 'm' && argc - i >= 4 && (argv[i] == 38 || argv[i] == 48) &&
                             argv[i + 1] == 2) {
                i += 2;
                processToken(TY_CSI_PS(cc, argv[i - 2]), COLOR_SPACE_RGB,
                                         (argv[i] << 16) | (argv[i + 1] << 8) | argv[i + 2]);
                i += 2;
            } else if (cc == 'm' && argc - i >= 2 &&
                                 (argv[i] == 38 || argv[i] == 48) && argv[i + 1] == 5) {
                i += 2;
                processToken(TY_CSI_PS(cc, argv[i - 2]), COLOR_SPACE_256, argv[i]);
            } else
                processToken(TY_CSI_PS(cc, argv[i]), 0, 0);
        }
        resetTokenizer();
    } else {
        if (lec(1, 0, ESC))
            return;
        if (les(1, 0, CHR)) {
            processToken(TY_CHR(), s[0], 0);
            resetTokenizer();
            return;
        }
        if (lec(2, 1, 'Y'))
            return;
        if (lec(3, 1, 'Y'))
            return;
        if (p < 4) {
            processToken(TY_VT52(s[1]), 0, 0);
            resetTokenizer();
            return;
        }
        processToken(TY_VT52(s[1]), s[2], s[3]);
        resetTokenizer();
        return;
    }
}

void Vt102Emulation::processOSC() {
    QString token = QString::fromWCharArray(tokenBuffer, tokenBufferPos);
    int i = 2;
    while (i < tokenBufferPos && tokenBuffer[i] != ';')
        i++;
    if (i == tokenBufferPos) {
        reportDecodingError();
        return;
    }

    int command = -1;
    switch (i - 1) {
    case 2:
        command = tokenBuffer[2] - L'0';
        break;
    case 3:
        command = 10 * (tokenBuffer[2] - L'0') + (tokenBuffer[3] - L'0');
        break;
    default:
        reportDecodingError();
        return;
    }

    switch (command) {
    /*
     * Operating System Controls https://www.xfree86.org/current/ctlseqs.html
     *
     * Ps = 0 → Change Icon Name and Window Title to Pt
     * Ps = 1 → Change Icon Name to Pt
     * Ps = 2 → Change Window Title to Pt
     */
    case 0:
    case 1:
    case 2: {
        QString newValue =
                QString::fromWCharArray(tokenBuffer + 3 + 1, tokenBufferPos - 3 - 2);
        processWindowAttributeChange(command, newValue);
        break;
    }
    case 52: {
        /* The first, Pc , may contain any character from the set c p s 0 1 2 3 4 5
         * 6 7 . It is used to construct a list of selection parameters for
         * clipboard, primary, select, or cut buffers 0 through 8 respectively, in
         * the order given. If the parameter is empty, xterm uses s 0 , to specify
         * the configurable primary/clipboard selection and cut buffer 0. The second
         * parameter, Pd , gives the selection data. Normally this is a string
         * encoded in base64. The data becomes the new selection, which is then
         * available for pasting by other applications. If the second parameter is a
         * ? , xterm replies to the host with the selection data encoded using the
         * same protocol.
         */
        QString arg =
                QString::fromWCharArray(tokenBuffer + 4 + 1, tokenBufferPos - 4 - 2);
        QStringList args = arg.split(";", Qt::SkipEmptyParts);
        auto processOSC52Text = [&](QString base64, QClipboard::Mode mode) {
            QClipboard *clipboard = QApplication::clipboard();
            if (base64 == "!") {
                clipboard->clear(mode);
            } else {
                QByteArray data = QByteArray::fromBase64(base64.toUtf8());
                clipboard->setText(QString::fromUtf8(data), mode);
            }
        };
        if (args.size() == 1 && args.at(0) != "?") {
            processOSC52Text(args.at(0), QClipboard::Clipboard);
        } else if (args.size() == 2) {
            if (args.at(0) == "c" && args.at(1) != "?") {
                processOSC52Text(args.at(1), QClipboard::Clipboard);
            }
            if (QApplication::clipboard()->supportsSelection()) {
                if (args.at(0) == "p" && args.at(1) != "?") {
                    processOSC52Text(args.at(1), QClipboard::Selection);
                }
            }
        }
        break;
    }
    default:
        reportDecodingError();
        break;
    }
}

void Vt102Emulation::processWindowAttributeChange(int attributeToChange, QString newValue) {
    _pendingTitleUpdates[attributeToChange] = newValue;
    _titleUpdateTimer->start(20);
}

void Vt102Emulation::updateTitle() {
    QListIterator<int> iter(_pendingTitleUpdates.keys());
    while (iter.hasNext()) {
        int arg = iter.next();
        doTitleChanged(arg, _pendingTitleUpdates[arg]);
    }
    _pendingTitleUpdates.clear();
}

void Vt102Emulation::doTitleChanged(int what, const QString &caption) {
    bool modified = false;

    if ((what == 0) || (what == 2)) {
        _isTitleChanged = true;
        if (_userTitle != caption) {
            _userTitle = caption;
            modified = true;
        }
    }

    if ((what == 0) || (what == 1)) {
        _isTitleChanged = true;
        if (_iconText != caption) {
            _iconText = caption;
            modified = true;
        }
    }

    if (what == 11) {
        QString colorString = caption.section(QLatin1Char(';'), 0, 0);
        QColor backColor = QColor(colorString);
        if (backColor.isValid()) {
            if (backColor != _modifiedBackground) {
                _modifiedBackground = backColor;
                emit changeBackgroundColorRequest(backColor);
            }
        }
    }

    if (what == 30) {
        _isTitleChanged = true;
        if (_nameTitle != caption) {
            _nameTitle = caption;
            return;
        }
    }

    if (what == 31) {
        QString cwd = caption;
        cwd =
                cwd.replace(QRegularExpression(QLatin1String("^~")), QDir::homePath());
        emit openUrlRequest(cwd);
    }

    if (what == 32) {
        _isTitleChanged = true;
        if (_iconName != caption) {
            _iconName = caption;

            modified = true;
        }
    }

    if (what == 50) {
        emit profileChangeCommandReceived(caption);
        return;
    }

    if (modified) {
        emit titleChanged(what, caption);
    }
}

// Interpreting Codes ---------------------------------------------------------

/*
     Now that the incoming character stream is properly tokenized,
     meaning is assigned to them. These are either operations of
     the current _screen, or of the emulation class itself.

     The token to be interpreteted comes in as a machine word
     possibly accompanied by two parameters.

     Likewise, the operations assigned to, come with up to two
     arguments. One could consider to make up a proper table
     from the function below.

     The technical reference manual provides more information
     about this mapping.
*/

void Vt102Emulation::processToken(int token, wchar_t p, int q) {
    switch (token) {
    case TY_CHR():
        _currentScreen->displayCharacter(p);
        dupDisplayCharacter(p);
        break;

    case TY_CTL('@'): /* NUL: ignored                      */
        break;
    case TY_CTL('A'): /* SOH: ignored                      */
        break;
    case TY_CTL('B'): /* STX: ignored                      */
        break;
    case TY_CTL('C'): /* ETX: ignored                      */
        break;
    case TY_CTL('D'): /* EOT: ignored                      */
        break;
    case TY_CTL('E'):
        reportAnswerBack();
        break;
    case TY_CTL('F'): /* ACK: ignored                      */
        break;
    case TY_CTL('G'):
        emit stateSet(NOTIFYBELL);
        break;
    case TY_CTL('H'):
        _currentScreen->backspace();
        break;
    case TY_CTL('I'):
        _currentScreen->tab();
        break;
    case TY_CTL('J'):
        _currentScreen->newLine();
        break;
    case TY_CTL('K'):
        _currentScreen->newLine();
        break;
    case TY_CTL('L'):
        _currentScreen->newLine();
        break;
    case TY_CTL('M'):
        _currentScreen->toStartOfLine();
        break;

    case TY_CTL('N'):
        useCharset(1);
        break;
    case TY_CTL('O'):
        useCharset(0);
        break;

    case TY_CTL('P'): /* DLE: ignored                      */
        break;
    case TY_CTL('Q'): /* DC1: XON continue                 */
        break;         
    case TY_CTL('R'): /* DC2: ignored                      */
        break;
    case TY_CTL('S'): /* DC3: XOFF halt                    */
        break;         
    case TY_CTL('T'): /* DC4: ignored                      */
        break;
    case TY_CTL('U'): /* NAK: ignored                      */
        break;
    case TY_CTL('V'): /* SYN: ignored                      */
        break;
    case TY_CTL('W'): /* ETB: ignored                      */
        break;
    case TY_CTL('X'):
        _currentScreen->displayCharacter(0x2592);
        dupDisplayCharacter(0x2592);
        break;         
    case TY_CTL('Y'): /* EM : ignored                      */
        break;
    case TY_CTL('Z'):
        _currentScreen->displayCharacter(0x2592);
        dupDisplayCharacter(0x2592);
        break;         
    case TY_CTL('['): /* ESC: cannot be seen here.         */
        break;
    case TY_CTL('\\'): /* FS : ignored                      */
        break;
    case TY_CTL(']'): /* GS : ignored                      */
        break;
    case TY_CTL('^'): /* RS : ignored                      */
        break;
    case TY_CTL('_'): /* US : ignored                      */
        break;

    case TY_ESC('D'):
        _currentScreen->index();
        break;
    case TY_ESC('E'):
        _currentScreen->nextLine();
        break;
    case TY_ESC('H'):
        _currentScreen->changeTabStop(true);
        break;
    case TY_ESC('M'):
        _currentScreen->reverseIndex();
        break;
    case TY_ESC('Z'):
        reportTerminalType();
        break;
    case TY_ESC('c'):
        reset();
        break;

    case TY_ESC('n'):
        useCharset(2);
        break;
    case TY_ESC('o'):
        useCharset(3);
        break;
    case TY_ESC('7'):
        saveCursor();
        break;
    case TY_ESC('8'):
        restoreCursor();
        break;

    case TY_ESC('='):
        setMode(MODE_AppKeyPad);
        break;
    case TY_ESC('>'):
        resetMode(MODE_AppKeyPad);
        break;
    case TY_ESC('<'):
        setMode(MODE_Ansi);
        break;

    case TY_ESC_CS('(', '0'):
        setCharset(0, '0');
        break;
    case TY_ESC_CS('(', 'A'):
        setCharset(0, 'A');
        break;
    case TY_ESC_CS('(', 'B'):
        setCharset(0, 'B');
        break;

    case TY_ESC_CS(')', '0'):
        setCharset(1, '0');
        break;
    case TY_ESC_CS(')', 'A'):
        setCharset(1, 'A');
        break;
    case TY_ESC_CS(')', 'B'):
        setCharset(1, 'B');
        break;

    case TY_ESC_CS('*', '0'):
        setCharset(2, '0');
        break;
    case TY_ESC_CS('*', 'A'):
        setCharset(2, 'A');
        break;
    case TY_ESC_CS('*', 'B'):
        setCharset(2, 'B');
        break;

    case TY_ESC_CS('+', '0'):
        setCharset(3, '0');
        break;
    case TY_ESC_CS('+', 'A'):
        setCharset(3, 'A');
        break;
    case TY_ESC_CS('+', 'B'):
        setCharset(3, 'B');
        break;

    case TY_ESC_CS('%', 'G'): /*No longer updating codec*/
        break;                 
    case TY_ESC_CS('%', '@'): /*No longer updating codec*/
        break;                 

    case TY_ESC_DE('3'): /* Double height line, top half    */
        _currentScreen->setLineProperty(LINE_DOUBLEWIDTH, true);
        _currentScreen->setLineProperty(LINE_DOUBLEHEIGHT, true);
        break;
    case TY_ESC_DE('4'): /* Double height line, bottom half */
        _currentScreen->setLineProperty(LINE_DOUBLEWIDTH, true);
        _currentScreen->setLineProperty(LINE_DOUBLEHEIGHT, true);
        break;
    case TY_ESC_DE('5'): /* Single width, single height line*/
        _currentScreen->setLineProperty(LINE_DOUBLEWIDTH, false);
        _currentScreen->setLineProperty(LINE_DOUBLEHEIGHT, false);
        break;
    case TY_ESC_DE('6'): /* Double width, single height line*/
        _currentScreen->setLineProperty(LINE_DOUBLEWIDTH, true);
        _currentScreen->setLineProperty(LINE_DOUBLEHEIGHT, false);
        break;
    case TY_ESC_DE('8'):
        _currentScreen->helpAlign();
        break;

    case TY_CSI_PS('t', 8):
        setImageSize(p /*lines */, q /* columns */);
        emit imageResizeRequest(QSize(q, p));
        break;

    case TY_CSI_PS('t', 28):
        emit changeTabTextColorRequest(p);
        break;

    case TY_CSI_PS('K', 0):
        _currentScreen->clearToEndOfLine();
        break;
    case TY_CSI_PS('K', 1):
        _currentScreen->clearToBeginOfLine();
        break;
    case TY_CSI_PS('K', 2):
        _currentScreen->clearEntireLine();
        break;
    case TY_CSI_PS('J', 0):
        _currentScreen->clearToEndOfScreen();
        break;
    case TY_CSI_PS('J', 1):
        _currentScreen->clearToBeginOfScreen();
        break;
    case TY_CSI_PS('J', 2):
        _currentScreen->clearEntireScreen();
        break;
    case TY_CSI_PS('J', 3):
        clearHistory();
        break;
    case TY_CSI_PS('g', 0):
        _currentScreen->changeTabStop(false);
        break;
    case TY_CSI_PS('g', 3):
        _currentScreen->clearTabStops();
        break;
    case TY_CSI_PS('h', 4):
        _currentScreen->setMode(MODE_Insert);
        break;
    case TY_CSI_PS('h', 20):
        setMode(MODE_NewLine);
        break;
    case TY_CSI_PS('i', 0): /* IGNORE: attached printer          */
        break;               
    case TY_CSI_PS('l', 4):
        _currentScreen->resetMode(MODE_Insert);
        break;
    case TY_CSI_PS('l', 20):
        resetMode(MODE_NewLine);
        break;
    case TY_CSI_PS('s', 0):
        saveCursor();
        break;
    case TY_CSI_PS('u', 0):
        restoreCursor();
        break;

    case TY_CSI_PS('m', 0):
        _currentScreen->setDefaultRendition();
        break;
    case TY_CSI_PS('m', 1):
        _currentScreen->setRendition(RE_BOLD);
        break;
    case TY_CSI_PS('m', 2):
        _currentScreen->setRendition(RE_FAINT);
        break;
    case TY_CSI_PS('m', 3):
        _currentScreen->setRendition(RE_ITALIC);
        break;
    case TY_CSI_PS('m', 4):
        _currentScreen->setRendition(RE_UNDERLINE);
        break;
    case TY_CSI_PS('m', 5):
        _currentScreen->setRendition(RE_BLINK);
        break;
    case TY_CSI_PS('m', 7):
        _currentScreen->setRendition(RE_REVERSE);
        break;
    case TY_CSI_PS('m', 8):
        _currentScreen->setRendition(RE_CONCEAL);
        break;
    case TY_CSI_PS('m', 9):
        _currentScreen->setRendition(RE_STRIKEOUT);
        break;
    case TY_CSI_PS('m', 53):
        _currentScreen->setRendition(RE_OVERLINE);
        break;
    case TY_CSI_PS('m', 10): /* IGNORED: mapping related          */
        break;                
    case TY_CSI_PS('m', 11): /* IGNORED: mapping related          */
        break;                
    case TY_CSI_PS('m', 12): /* IGNORED: mapping related          */
        break;                
    case TY_CSI_PS('m', 21):
        _currentScreen->resetRendition(RE_BOLD);
        break;
    case TY_CSI_PS('m', 22):
        _currentScreen->resetRendition(RE_BOLD);
        _currentScreen->resetRendition(RE_FAINT);
        break;
    case TY_CSI_PS('m', 23):
        _currentScreen->resetRendition(RE_ITALIC);
        break;
    case TY_CSI_PS('m', 24):
        _currentScreen->resetRendition(RE_UNDERLINE);
        break;
    case TY_CSI_PS('m', 25):
        _currentScreen->resetRendition(RE_BLINK);
        break;
    case TY_CSI_PS('m', 27):
        _currentScreen->resetRendition(RE_REVERSE);
        break;
    case TY_CSI_PS('m', 28):
        _currentScreen->resetRendition(RE_CONCEAL);
        break;
    case TY_CSI_PS('m', 29):
        _currentScreen->resetRendition(RE_STRIKEOUT);
        break;
    case TY_CSI_PS('m', 55):
        _currentScreen->resetRendition(RE_OVERLINE);
        break;

    case TY_CSI_PS('m', 30):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 0);
        break;
    case TY_CSI_PS('m', 31):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 1);
        break;
    case TY_CSI_PS('m', 32):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 2);
        break;
    case TY_CSI_PS('m', 33):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 3);
        break;
    case TY_CSI_PS('m', 34):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 4);
        break;
    case TY_CSI_PS('m', 35):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 5);
        break;
    case TY_CSI_PS('m', 36):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 6);
        break;
    case TY_CSI_PS('m', 37):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 7);
        break;

    case TY_CSI_PS('m', 38):
        _currentScreen->setForeColor(p, q);
        break;

    case TY_CSI_PS('m', 39):
        _currentScreen->setForeColor(COLOR_SPACE_DEFAULT, 0);
        break;

    case TY_CSI_PS('m', 40):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 0);
        break;
    case TY_CSI_PS('m', 41):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 1);
        break;
    case TY_CSI_PS('m', 42):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 2);
        break;
    case TY_CSI_PS('m', 43):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 3);
        break;
    case TY_CSI_PS('m', 44):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 4);
        break;
    case TY_CSI_PS('m', 45):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 5);
        break;
    case TY_CSI_PS('m', 46):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 6);
        break;
    case TY_CSI_PS('m', 47):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 7);
        break;

    case TY_CSI_PS('m', 48):
        _currentScreen->setBackColor(p, q);
        break;

    case TY_CSI_PS('m', 49):
        _currentScreen->setBackColor(COLOR_SPACE_DEFAULT, 1);
        break;

    case TY_CSI_PS('m', 90):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 8);
        break;
    case TY_CSI_PS('m', 91):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 9);
        break;
    case TY_CSI_PS('m', 92):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 10);
        break;
    case TY_CSI_PS('m', 93):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 11);
        break;
    case TY_CSI_PS('m', 94):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 12);
        break;
    case TY_CSI_PS('m', 95):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 13);
        break;
    case TY_CSI_PS('m', 96):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 14);
        break;
    case TY_CSI_PS('m', 97):
        _currentScreen->setForeColor(COLOR_SPACE_SYSTEM, 15);
        break;

    case TY_CSI_PS('m', 100):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 8);
        break;
    case TY_CSI_PS('m', 101):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 9);
        break;
    case TY_CSI_PS('m', 102):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 10);
        break;
    case TY_CSI_PS('m', 103):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 11);
        break;
    case TY_CSI_PS('m', 104):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 12);
        break;
    case TY_CSI_PS('m', 105):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 13);
        break;
    case TY_CSI_PS('m', 106):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 14);
        break;
    case TY_CSI_PS('m', 107):
        _currentScreen->setBackColor(COLOR_SPACE_SYSTEM, 15);
        break;

    case TY_CSI_PS('n', 5):
        reportStatus();
        break;
    case TY_CSI_PS('n', 6):
        reportCursorPosition();
        break;
    case TY_CSI_PS('q', 0): /* IGNORED: LEDs off                 */
        break;               
    case TY_CSI_PS('q', 1): /* IGNORED: LED1 on                  */
        break;               
    case TY_CSI_PS('q', 2): /* IGNORED: LED2 on                  */
        break;               
    case TY_CSI_PS('q', 3): /* IGNORED: LED3 on                  */
        break;               
    case TY_CSI_PS('q', 4): /* IGNORED: LED4 on                  */
        break;               
    case TY_CSI_PS('x', 0):
        reportTerminalParms(2);
        break;
    case TY_CSI_PS('x', 1):
        reportTerminalParms(3);
        break;

    case TY_CSI_PS_SP('q', 0): /* fall through */
    case TY_CSI_PS_SP('q', 1):
        emit cursorChanged(KeyboardCursorShape::BlockCursor, true);
        break;
    case TY_CSI_PS_SP('q', 2):
        emit cursorChanged(KeyboardCursorShape::BlockCursor, false);
        break;
    case TY_CSI_PS_SP('q', 3):
        emit cursorChanged(KeyboardCursorShape::UnderlineCursor, true);
        break;
    case TY_CSI_PS_SP('q', 4):
        emit cursorChanged(KeyboardCursorShape::UnderlineCursor, false);
        break;
    case TY_CSI_PS_SP('q', 5):
        emit cursorChanged(KeyboardCursorShape::IBeamCursor, true);
        break;
    case TY_CSI_PS_SP('q', 6):
        emit cursorChanged(KeyboardCursorShape::IBeamCursor, false);
        break;

    case TY_CSI_PN('@'):
        _currentScreen->insertChars(p);
        break;
    case TY_CSI_PN('A'):
        _currentScreen->cursorUp(p);
        break;
    case TY_CSI_PN('B'):
        _currentScreen->cursorDown(p);
        break;
    case TY_CSI_PN('C'):
        _currentScreen->cursorRight(p);
        break;
    case TY_CSI_PN('D'):
        _currentScreen->cursorLeft(p);
        break;
    case TY_CSI_PN('E'):
        _currentScreen->cursorNextLine(p);
        break;
    case TY_CSI_PN('F'):
        _currentScreen->cursorPreviousLine(p);
        break;
    case TY_CSI_PN('G'):
        _currentScreen->setCursorX(p);
        break;
    case TY_CSI_PN('H'):
        _currentScreen->setCursorYX(p, q);
        break;
    case TY_CSI_PN('I'):
        _currentScreen->tab(p);
        break;
    case TY_CSI_PN('L'):
        _currentScreen->insertLines(p);
        break;
    case TY_CSI_PN('M'):
        _currentScreen->deleteLines(p);
        break;
    case TY_CSI_PN('P'):
        _currentScreen->deleteChars(p);
        break;
    case TY_CSI_PN('S'):
        _currentScreen->scrollUp(p);
        break;
    case TY_CSI_PN('T'):
        _currentScreen->scrollDown(p);
        break;
    case TY_CSI_PN('X'):
        _currentScreen->eraseChars(p);
        break;
    case TY_CSI_PN('Z'):
        _currentScreen->backtab(p);
        break;
    case TY_CSI_PN('b'):
        _currentScreen->repeatChars(p);
        break;
    case TY_CSI_PN('c'):
        reportTerminalType();
        break;
    case TY_CSI_PN('d'):
        _currentScreen->setCursorY(p);
        break;
    case TY_CSI_PN('f'):
        _currentScreen->setCursorYX(p, q);
        break;
    case TY_CSI_PN('r'):
        setMargins(p, q);
        break;            
    case TY_CSI_PN('y'): /* IGNORED: Confidence test          */
        break;            

    case TY_CSI_PR('h', 1):
        setMode(MODE_AppCuKeys);
        break;
    case TY_CSI_PR('l', 1):
        resetMode(MODE_AppCuKeys);
        break;
    case TY_CSI_PR('s', 1):
        saveMode(MODE_AppCuKeys);
        break;
    case TY_CSI_PR('r', 1):
        restoreMode(MODE_AppCuKeys);
        break;

    case TY_CSI_PR('l', 2):
        resetMode(MODE_Ansi);
        break;

    case TY_CSI_PR('h', 3):
        setMode(MODE_132Columns);
        break;
    case TY_CSI_PR('l', 3):
        resetMode(MODE_132Columns);
        break;

    case TY_CSI_PR('h', 4): /* IGNORED: soft scrolling           */
        break;               
    case TY_CSI_PR('l', 4): /* IGNORED: soft scrolling           */
        break;               

    case TY_CSI_PR('h', 5):
        _currentScreen->setMode(MODE_Screen);
        break;
    case TY_CSI_PR('l', 5):
        _currentScreen->resetMode(MODE_Screen);
        break;

    case TY_CSI_PR('h', 6):
        _currentScreen->setMode(MODE_Origin);
        break;
    case TY_CSI_PR('l', 6):
        _currentScreen->resetMode(MODE_Origin);
        break;
    case TY_CSI_PR('s', 6):
        _currentScreen->saveMode(MODE_Origin);
        break;
    case TY_CSI_PR('r', 6):
        _currentScreen->restoreMode(MODE_Origin);
        break;

    case TY_CSI_PR('h', 7):
        _currentScreen->setMode(MODE_Wrap);
        break;
    case TY_CSI_PR('l', 7):
        _currentScreen->resetMode(MODE_Wrap);
        break;
    case TY_CSI_PR('s', 7):
        _currentScreen->saveMode(MODE_Wrap);
        break;
    case TY_CSI_PR('r', 7):
        _currentScreen->restoreMode(MODE_Wrap);
        break;

    case TY_CSI_PR('h', 8): /* IGNORED: autorepeat on            */
        break;               
    case TY_CSI_PR('l', 8): /* IGNORED: autorepeat off           */
        break;               
    case TY_CSI_PR('s', 8): /* IGNORED: autorepeat on            */
        break;               
    case TY_CSI_PR('r', 8): /* IGNORED: autorepeat off           */
        break;               

    case TY_CSI_PR('h', 9): /* IGNORED: interlace                */
        break;               
    case TY_CSI_PR('l', 9): /* IGNORED: interlace                */
        break;               
    case TY_CSI_PR('s', 9): /* IGNORED: interlace                */
        break;               
    case TY_CSI_PR('r', 9): /* IGNORED: interlace                */
        break;               

    case TY_CSI_PR('h', 12): /* IGNORED: Cursor blink             */
        break;                
    case TY_CSI_PR('l', 12): /* IGNORED: Cursor blink             */
        break;                
    case TY_CSI_PR('s', 12): /* IGNORED: Cursor blink             */
        break;                
    case TY_CSI_PR('r', 12): /* IGNORED: Cursor blink             */
        break;                

    case TY_CSI_PR('h', 25):
        setMode(MODE_Cursor);
        break;
    case TY_CSI_PR('l', 25):
        resetMode(MODE_Cursor);
        break;
    case TY_CSI_PR('s', 25):
        saveMode(MODE_Cursor);
        break;
    case TY_CSI_PR('r', 25):
        restoreMode(MODE_Cursor);
        break;

    case TY_CSI_PR('h', 40):
        setMode(MODE_Allow132Columns);
        break;
    case TY_CSI_PR('l', 40):
        resetMode(MODE_Allow132Columns);
        break;

    case TY_CSI_PR('h', 41): /* IGNORED: obsolete more(1) fix     */
        break;                
    case TY_CSI_PR('l', 41): /* IGNORED: obsolete more(1) fix     */
        break;                
    case TY_CSI_PR('s', 41): /* IGNORED: obsolete more(1) fix     */
        break;                
    case TY_CSI_PR('r', 41): /* IGNORED: obsolete more(1) fix     */
        break;                

    case TY_CSI_PR('h', 47):
        setMode(MODE_AppScreen);
        break;
    case TY_CSI_PR('l', 47):
        resetMode(MODE_AppScreen);
        break;
    case TY_CSI_PR('s', 47):
        saveMode(MODE_AppScreen);
        break;
    case TY_CSI_PR('r', 47):
        restoreMode(MODE_AppScreen);
        break;

    case TY_CSI_PR('h', 67): /* IGNORED: DECBKM                   */
        break;                
    case TY_CSI_PR('l', 67): /* IGNORED: DECBKM                   */
        break;                
    case TY_CSI_PR('s', 67): /* IGNORED: DECBKM                   */
        break;                
    case TY_CSI_PR('r', 67): /* IGNORED: DECBKM                   */
        break;                

    case TY_CSI_PR('h', 1000):
        setMode(MODE_Mouse1000);
        break;
    case TY_CSI_PR('l', 1000):
        resetMode(MODE_Mouse1000);
        break;
    case TY_CSI_PR('s', 1000):
        saveMode(MODE_Mouse1000);
        break;
    case TY_CSI_PR('r', 1000):
        restoreMode(MODE_Mouse1000);
        break;

    case TY_CSI_PR('h', 1001): /* IGNORED: hilite mouse tracking    */
        break;                  
    case TY_CSI_PR('l', 1001):
        resetMode(MODE_Mouse1001);
        break;                  
    case TY_CSI_PR('s', 1001): /* IGNORED: hilite mouse tracking    */
        break;                  
    case TY_CSI_PR('r', 1001): /* IGNORED: hilite mouse tracking    */
        break;                  

    case TY_CSI_PR('h', 1002):
        setMode(MODE_Mouse1002);
        break;
    case TY_CSI_PR('l', 1002):
        resetMode(MODE_Mouse1002);
        break;
    case TY_CSI_PR('s', 1002):
        saveMode(MODE_Mouse1002);
        break;
    case TY_CSI_PR('r', 1002):
        restoreMode(MODE_Mouse1002);
        break;

    case TY_CSI_PR('h', 1003):
        setMode(MODE_Mouse1003);
        break;
    case TY_CSI_PR('l', 1003):
        resetMode(MODE_Mouse1003);
        break;
    case TY_CSI_PR('s', 1003):
        saveMode(MODE_Mouse1003);
        break;
    case TY_CSI_PR('r', 1003):
        restoreMode(MODE_Mouse1003);
        break;

    case TY_CSI_PR('h', 1004):
        _reportFocusEvents = true;
        break;
    case TY_CSI_PR('l', 1004):
        _reportFocusEvents = false;
        break;

    case TY_CSI_PR('h', 1005):
        setMode(MODE_Mouse1005);
        break;
    case TY_CSI_PR('l', 1005):
        resetMode(MODE_Mouse1005);
        break;
    case TY_CSI_PR('s', 1005):
        saveMode(MODE_Mouse1005);
        break;
    case TY_CSI_PR('r', 1005):
        restoreMode(MODE_Mouse1005);
        break;

    case TY_CSI_PR('h', 1006):
        setMode(MODE_Mouse1006);
        break;
    case TY_CSI_PR('l', 1006):
        resetMode(MODE_Mouse1006);
        break;
    case TY_CSI_PR('s', 1006):
        saveMode(MODE_Mouse1006);
        break;
    case TY_CSI_PR('r', 1006):
        restoreMode(MODE_Mouse1006);
        break;

    case TY_CSI_PR('h', 1015):
        setMode(MODE_Mouse1015);
        break;
    case TY_CSI_PR('l', 1015):
        resetMode(MODE_Mouse1015);
        break;
    case TY_CSI_PR('s', 1015):
        saveMode(MODE_Mouse1015);
        break;
    case TY_CSI_PR('r', 1015):
        restoreMode(MODE_Mouse1015);
        break;

    case TY_CSI_PR('h', 1034): /* IGNORED: 8bitinput activation     */
        break;                  

    case TY_CSI_PR('h', 1047):
        setMode(MODE_AppScreen);
        break;
    case TY_CSI_PR('l', 1047):
        _screen[1]->clearEntireScreen();
        resetMode(MODE_AppScreen);
        break;
    case TY_CSI_PR('s', 1047):
        saveMode(MODE_AppScreen);
        break;
    case TY_CSI_PR('r', 1047):
        restoreMode(MODE_AppScreen);
        break;

    case TY_CSI_PR('h', 1048):
        saveCursor();
        break;
    case TY_CSI_PR('l', 1048):
        restoreCursor();
        break;
    case TY_CSI_PR('s', 1048):
        saveCursor();
        break;
    case TY_CSI_PR('r', 1048):
        restoreCursor();
        break;

    case TY_CSI_PR('h', 1049):
        saveCursor();
        _screen[1]->clearEntireScreen();
        setMode(MODE_AppScreen);
        break;
    case TY_CSI_PR('l', 1049):
        resetMode(MODE_AppScreen);
        restoreCursor();
        break;

    case TY_CSI_PR('h', 2004):
        setMode(MODE_BracketedPaste);
        break;
    case TY_CSI_PR('l', 2004):
        resetMode(MODE_BracketedPaste);
        break;
    case TY_CSI_PR('s', 2004):
        saveMode(MODE_BracketedPaste);
        break;
    case TY_CSI_PR('r', 2004):
        restoreMode(MODE_BracketedPaste);
        break;

    case TY_CSI_PE('p'): /* IGNORED: reset         (        ) */
        break;

    case TY_VT52('A'):
        _currentScreen->cursorUp(1);
        break;
    case TY_VT52('B'):
        _currentScreen->cursorDown(1);
        break;
    case TY_VT52('C'):
        _currentScreen->cursorRight(1);
        break;
    case TY_VT52('D'):
        _currentScreen->cursorLeft(1);
        break;

    case TY_VT52('F'):
        setAndUseCharset(0, '0');
        break;
    case TY_VT52('G'):
        setAndUseCharset(0, 'B');
        break;

    case TY_VT52('H'):
        _currentScreen->setCursorYX(1, 1);
        break;
    case TY_VT52('I'):
        _currentScreen->reverseIndex();
        break;
    case TY_VT52('J'):
        _currentScreen->clearToEndOfScreen();
        break;
    case TY_VT52('K'):
        _currentScreen->clearToEndOfLine();
        break;
    case TY_VT52('Y'):
        _currentScreen->setCursorYX(p - 31, q - 31);
        break;
    case TY_VT52('Z'):
        reportTerminalType();
        break;
    case TY_VT52('<'):
        setMode(MODE_Ansi);
        break;
    case TY_VT52('='):
        setMode(MODE_AppKeyPad);
        break;
    case TY_VT52('>'):
        resetMode(MODE_AppKeyPad);
        break;

    case TY_CSI_PG('c'):
        reportSecondaryAttributes();
        break;

    default:
        reportDecodingError();
        break;
    };
}

void Vt102Emulation::clearScreenAndSetColumns(int columnCount) {
    setImageSize(_currentScreen->getLines(), columnCount);
    clearEntireScreen();
    setDefaultMargins();
    _currentScreen->setCursorYX(0, 0);
}

void Vt102Emulation::sendString(const char *s, int length) {
    if (length >= 0)
        emit sendData(s, length);
    else
        emit sendData(s, static_cast<int>(strlen(s)));
}

void Vt102Emulation::reportCursorPosition() {
    const size_t sz = 20;
    char tmp[sz];
    const size_t r =
            snprintf(tmp, sz, "\033[%d;%dR", _currentScreen->getCursorY() + 1,
                             _currentScreen->getCursorX() + 1);
    if (sz <= r) {
        qWarning("Vt102Emulation::reportCursorPosition: Buffer too small\n");
    }
    sendString(tmp);
}

void Vt102Emulation::reportTerminalType() {
    if (getMode(MODE_Ansi))
        sendString("\033[?1;2c");
    else
        sendString("\033/Z");
}

void Vt102Emulation::reportSecondaryAttributes() {
    if (getMode(MODE_Ansi))
        sendString("\033[>0;115;0c"); 
    else
        sendString("\033/Z"); 
}

void Vt102Emulation::reportTerminalParms(int p)
// DECREPTPARM
{
    const size_t sz = 100;
    char tmp[sz];
    const size_t r =
            snprintf(tmp, sz, "\033[%d;1;1;112;112;1;0x", p);
    if (sz <= r) {
        qWarning("Vt102Emulation::reportTerminalParms: Buffer too small\n");
    }
    sendString(tmp);
}

void Vt102Emulation::reportStatus() {
    sendString("\033[0n");
}

void Vt102Emulation::reportAnswerBack() {
    const char *ANSWER_BACK = "";
    sendString(ANSWER_BACK);
}

void Vt102Emulation::sendMouseEvent(int cb, int cx, int cy, int eventType) {
    if (cx < 1 || cy < 1)
        return;

    if (eventType == 2 && !getMode(MODE_Mouse1006))
        cb = 3;

    if (cb >= 4)
        cb += 0x3c;

    if ((getMode(MODE_Mouse1002) || getMode(MODE_Mouse1003)) && eventType == 1)
        cb += 0x20; 

    char command[64];
    command[0] = '\0';
    if (getMode(MODE_Mouse1006)) {
        snprintf(command, sizeof(command), "\033[<%d;%d;%d%c", cb, cx, cy,
                         eventType == 2 ? 'm' : 'M');
    } else if (getMode(MODE_Mouse1015)) {
        snprintf(command, sizeof(command), "\033[%d;%d;%dM", cb + 0x20, cx, cy);
    } else if (getMode(MODE_Mouse1005)) {
        if (cx <= 2015 && cy <= 2015) {
            QChar coords[2];
            coords[0] = static_cast<char16_t>(cx + 0x20);
            coords[1] = static_cast<char16_t>(cy + 0x20);
            QString coordsStr = QString(coords, 2);
            QByteArray utf8 = coordsStr.toUtf8();
            snprintf(command, sizeof(command), "\033[M%c%s", cb + 0x20,
                             utf8.constData());
        }
    } else if (cx <= 223 && cy <= 223) {
        snprintf(command, sizeof(command), "\033[M%c%c%c", cb + 0x20, cx + 0x20,
                         cy + 0x20);
    }

    sendString(command);
}

/**
 * The focus lost event can be used by Vim (or other terminal applications)
 * to recognize that the konsole window has lost focus.
 * The escape sequence is also used by iTerm2.
 * Vim needs the following plugin to be installed to convert the escape
 * sequence into the FocusLost autocmd: https://github.com/sjl/vitality.vim
 */
void Vt102Emulation::focusLost(void) {
    if (_reportFocusEvents)
        sendString("\033[O");
}

/**
 * The focus gained event can be used by Vim (or other terminal applications)
 * to recognize that the konsole window has gained focus again.
 * The escape sequence is also used by iTerm2.
 * Vim needs the following plugin to be installed to convert the escape
 * sequence into the FocusGained autocmd: https://github.com/sjl/vitality.vim
 */
void Vt102Emulation::focusGained(void) {
    if (_reportFocusEvents)
        sendString("\033[I");
}

void Vt102Emulation::sendText(const QString &text) {
    if (!text.isEmpty()) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, text);
        sendKeyEvent(&event, false);
    }
}

void Vt102Emulation::sendKeyEvent(QKeyEvent *event, bool fromPaste) {
    Qt::KeyboardModifiers modifiers = event->modifiers();
    KeyboardTranslator::States states = KeyboardTranslator::NoState;

    if (getMode(MODE_NewLine))
        states |= KeyboardTranslator::NewLineState;
    if (getMode(MODE_Ansi))
        states |= KeyboardTranslator::AnsiState;
    if (getMode(MODE_AppCuKeys))
        states |= KeyboardTranslator::CursorKeysState;
    if (getMode(MODE_AppScreen))
        states |= KeyboardTranslator::AlternateScreenState;
    if (getMode(MODE_AppKeyPad) && (modifiers & Qt::KeypadModifier))
        states |= KeyboardTranslator::ApplicationKeypadState;

    if (modifiers & KeyboardTranslator::CTRL_MOD) {
        switch (event->key()) {
        case Qt::Key_S:
            emit flowControlKeyPressed(true);
            break;
        case Qt::Key_Q:
        case Qt::Key_C:
            emit flowControlKeyPressed(false);
            break;
        }
    }

    if (_keyTranslator) {
        KeyboardTranslator::Entry entry =
                _keyTranslator->findEntry(event->key(), modifiers, states);

        if ((modifiers & Qt::AltModifier) && (event->key() == Qt::Key_Left)) {
            entry = _keyTranslator->findEntry(Qt::Key_Home, Qt::NoModifier, states);
            modifiers = Qt::NoModifier;
        } else if ((modifiers & Qt::AltModifier) &&
                             (event->key() == Qt::Key_Right)) {
            entry = _keyTranslator->findEntry(Qt::Key_End, Qt::NoModifier, states);
            modifiers = Qt::NoModifier;
        }
#if defined(Q_OS_MACOS)
        if ((modifiers & Qt::ControlModifier) &&
                (event->key() == Qt::Key_Backspace)) {
            entry = _keyTranslator->findEntry(Qt::Key_Delete, Qt::NoModifier, states);
            modifiers = Qt::NoModifier;
        }
#endif
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
        if (_enableHandleCtrlC && (modifiers & Qt::ControlModifier) &&
                (event->key() == Qt::Key_C)) {
            bool isSelection = !_currentScreen->isClearSelection();
            if (isSelection) {
                emit handleCtrlC();
                return;
            }
        }
#endif

        QByteArray textToSend;

        bool wantsAltModifier =
                entry.modifiers() & entry.modifierMask() & Qt::AltModifier;
        bool wantsMetaModifier =
                entry.modifiers() & entry.modifierMask() & Qt::MetaModifier;
        bool wantsAnyModifier = entry.state() & entry.stateMask() &
                                                        KeyboardTranslator::AnyModifierState;

        if (modifiers & Qt::AltModifier &&
                !(wantsAltModifier || wantsAnyModifier) && !event->text().isEmpty()) {
            textToSend.prepend("\033");
        }
        if (modifiers & Qt::MetaModifier &&
                !(wantsMetaModifier || wantsAnyModifier) && !event->text().isEmpty()) {
            textToSend.prepend("\030@s");
        }

        if (entry.command() != KeyboardTranslator::NoCommand) {
            if (entry.command() & KeyboardTranslator::EraseCommand) {
                textToSend += eraseChar();
            } else {
                emit handleCommandFromKeyboard(entry.command());
            }

        } else if (!entry.text().isEmpty()) {
            textToSend += entry.text(true, modifiers);
        } else if ((modifiers & KeyboardTranslator::CTRL_MOD) &&
                             event->key() >= 0x40 && event->key() < 0x5f) {
            textToSend += (event->key() & 0x1f);
        } else if (event->key() == Qt::Key_Tab) {
            textToSend += 0x09;
        } else if (event->key() == Qt::Key_PageUp) {
            textToSend += "\033[5~";
        } else if (event->key() == Qt::Key_PageDown) {
            textToSend += "\033[6~";
        } else {
            textToSend += _fromUtf16(event->text());
        }

        if (!fromPaste && textToSend.length()) {
            emit outputFromKeypressEvent();
        }
        emit sendData(textToSend.constData(), textToSend.length());
    } else {
        QString translatorError =
                tr("No keyboard translator available.  "
                     "The information needed to convert key presses "
                     "into characters to send to the terminal "
                     "is missing.");
        reset();
        receiveData(translatorError.toUtf8().constData(), translatorError.size());
    }
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                VT100 Charsets                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

// Character Set Conversion ------------------------------------------------ --

/*
     The processing contains a VT100 specific code translation layer.
     It's still in use and mainly responsible for the line drawing graphics.

     These and some other glyphs are assigned to codes (0x5f-0xfe)
     normally occupied by the latin letters. Since this codes also
     appear within control sequences, the extra code conversion
     does not permute with the tokenizer and is placed behind it
     in the pipeline. It only applies to tokens, which represent
     plain characters.

     This conversion it eventually continued in TerminalDisplay.C, since
     it might involve VT100 enhanced fonts, which have these
     particular glyphs allocated in (0x00-0x1f) in their code page.
*/

#define CHARSET _charset[_currentScreen == _screen[1]]


wchar_t Vt102Emulation::applyCharset(wchar_t c) {
    const unsigned short vt100_graphics[32] = {
            0x0020, 0x25C6, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0x00b0,
            0x00b1, 0x2424, 0x240b, 0x2518, 0x2510, 0x250c, 0x2514, 0x253c,
            0xF800, 0xF801, 0x2500, 0xF803, 0xF804, 0x251c, 0x2524, 0x2534,
            0x252c, 0x2502, 0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x00b7};
    if (CHARSET.graphic && 0x5f <= c && c <= 0x7e)
        return vt100_graphics[c - 0x5f];
    if (CHARSET.pound && c == '#')
        return 0xa3;
    return c;
}

/*
     "Charset" related part of the emulation state.
     This configures the VT100 charset filter.

     While most operation work on the current _screen,
     the following two are different.
*/

void Vt102Emulation::resetCharset(int scrno) {
    _charset[scrno].cu_cs = 0;
    qstrncpy(_charset[scrno].charset, "BBBB", 4);
    _charset[scrno].sa_graphic = false;
    _charset[scrno].sa_pound = false;
    _charset[scrno].graphic = false;
    _charset[scrno].pound = false;
}

void Vt102Emulation::setCharset(int n, int cs)
{
    _charset[0].charset[n & 3] = cs;
    useCharset(_charset[0].cu_cs);
    _charset[1].charset[n & 3] = cs;
    useCharset(_charset[1].cu_cs);
}

void Vt102Emulation::setAndUseCharset(int n, int cs) {
    CHARSET.charset[n & 3] = cs;
    useCharset(n & 3);
}

void Vt102Emulation::useCharset(int n) {
    CHARSET.cu_cs = n & 3;
    CHARSET.graphic = (CHARSET.charset[n & 3] == '0');
    CHARSET.pound = (CHARSET.charset[n & 3] == 'A');
}

void Vt102Emulation::setDefaultMargins() {
    _screen[0]->setDefaultMargins();
    _screen[1]->setDefaultMargins();
}

void Vt102Emulation::setMargins(int t, int b) {
    _screen[0]->setMargins(t, b);
    _screen[1]->setMargins(t, b);
}

void Vt102Emulation::saveCursor() {
    CHARSET.sa_graphic = CHARSET.graphic;
    CHARSET.sa_pound = CHARSET.pound;
    _currentScreen->saveCursor();
}

void Vt102Emulation::restoreCursor() {
    CHARSET.graphic = CHARSET.sa_graphic;
    CHARSET.pound = CHARSET.sa_pound;
    _currentScreen->restoreCursor();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Mode Operations                            */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*
     Some of the emulations state is either added to the state of the screens.

     This causes some scoping problems, since different emulations choose to
     located the mode either to the current _screen or to both.

     For strange reasons, the extend of the rendition attributes ranges over
     all screens and not over the actual _screen.

     We decided on the precise precise extend, somehow.
*/

// "Mode" related part of the state. These are all booleans.

void Vt102Emulation::resetModes() {

    resetMode(MODE_132Columns);
    saveMode(MODE_132Columns);
    resetMode(MODE_Mouse1000);
    saveMode(MODE_Mouse1000);
    resetMode(MODE_Mouse1001);
    saveMode(MODE_Mouse1001);
    resetMode(MODE_Mouse1002);
    saveMode(MODE_Mouse1002);
    resetMode(MODE_Mouse1003);
    saveMode(MODE_Mouse1003);
    resetMode(MODE_Mouse1005);
    saveMode(MODE_Mouse1005);
    resetMode(MODE_Mouse1006);
    saveMode(MODE_Mouse1006);
    resetMode(MODE_Mouse1015);
    saveMode(MODE_Mouse1015);
    resetMode(MODE_BracketedPaste);
    saveMode(MODE_BracketedPaste);

    resetMode(MODE_AppScreen);
    saveMode(MODE_AppScreen);
    resetMode(MODE_AppCuKeys);
    saveMode(MODE_AppCuKeys);
    resetMode(MODE_AppKeyPad);
    saveMode(MODE_AppKeyPad);
    resetMode(MODE_NewLine);
    setMode(MODE_Ansi);
}

void Vt102Emulation::setMode(int m) {
    _currentModes.mode[m] = true;
    switch (m) {
    case MODE_132Columns:
        if (getMode(MODE_Allow132Columns))
            clearScreenAndSetColumns(132);
        else
            _currentModes.mode[m] = false;
        break;
    case MODE_Mouse1000:
    case MODE_Mouse1001:
    case MODE_Mouse1002:
    case MODE_Mouse1003:
        emit programUsesMouseChanged(false);
        break;

    case MODE_BracketedPaste:
        emit programBracketedPasteModeChanged(true);
        break;

    case MODE_AppScreen:
        _screen[1]->clearSelection();
        setScreen(1);
        break;
    }
    if (m < MODES_SCREEN || m == MODE_NewLine) {
        _screen[0]->setMode(m);
        _screen[1]->setMode(m);
    }
}

void Vt102Emulation::resetMode(int m) {
    _currentModes.mode[m] = false;
    switch (m) {
    case MODE_132Columns:
        if (getMode(MODE_Allow132Columns))
            clearScreenAndSetColumns(80);
        break;
    case MODE_Mouse1000:
    case MODE_Mouse1001:
    case MODE_Mouse1002:
    case MODE_Mouse1003:
        emit programUsesMouseChanged(true);
        break;

    case MODE_BracketedPaste:
        emit programBracketedPasteModeChanged(false);
        break;

    case MODE_AppScreen:
        _screen[0]->clearSelection();
        setScreen(0);
        break;
    }
    if (m < MODES_SCREEN || m == MODE_NewLine) {
        _screen[0]->resetMode(m);
        _screen[1]->resetMode(m);
    }
}

void Vt102Emulation::saveMode(int m) {
    _savedModes.mode[m] = _currentModes.mode[m];
}

void Vt102Emulation::restoreMode(int m) {
    if (_savedModes.mode[m])
        setMode(m);
    else
        resetMode(m);
}

bool Vt102Emulation::getMode(int m) { return _currentModes.mode[m]; }

char Vt102Emulation::eraseChar() const {
    KeyboardTranslator::Entry entry = _keyTranslator->findEntry(
            Qt::Key_Backspace, Qt::NoModifier, KeyboardTranslator::NoState);
    if (entry.text().size() > 0)
        return entry.text().at(0);
    else
        return '\b';
}

void Vt102Emulation::reportDecodingError() {
    if (tokenBufferPos == 0 || (tokenBufferPos == 1 && (tokenBuffer[0] & 0xff) >= 32))
        return;
    //qDebug()<< "Undecodable sequence:" << QString::fromWCharArray(tokenBuffer, tokenBufferPos);
}
