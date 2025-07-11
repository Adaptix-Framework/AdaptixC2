#include "Emulation.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include <QApplication>
#include <QClipboard>
#include <QHash>
#include <QKeyEvent>
#include <QTextStream>
#include <QThread>
#include <QTime>

#include "util/KeyboardTranslator.h"
#include "Screen.h"
#include "ScreenWindow.h"
#include "util/TerminalCharacterDecoder.h"
#include "TerminalDisplay.h"

Emulation::Emulation()
    : _currentScreen(nullptr)
    , _keyTranslator(nullptr)
    , _enableHandleCtrlC(false)
    , _usesMouse(false)
    , _bracketedPasteMode(false)
    , _fromUtf8(QStringEncoder::Utf16) {
    _screen[0] = new Screen(40, 80);
    _screen[1] = new Screen(40, 80);
    _currentScreen = _screen[0];

    connect(&_bulkTimer1, &QTimer::timeout, this, &Emulation::showBulk);
    connect(&_bulkTimer2, &QTimer::timeout, this, &Emulation::showBulk);

    connect(this, &Emulation::programUsesMouseChanged, this, &Emulation::usesMouseChanged);
    connect(this, &Emulation::programBracketedPasteModeChanged, this, &Emulation::bracketedPasteModeChanged);

    connect(this, &Emulation::cursorChanged, this, [this](KeyboardCursorShape cursorShape, bool blinkingCursorEnabled) {
            emit profileChangeCommandReceived( QString(QLatin1String("CursorShape=%1;BlinkingCursorEnabled=%2")).arg(static_cast<int>(cursorShape)).arg(blinkingCursorEnabled));
    });
}

bool Emulation::programUsesMouse() const { 
    return _usesMouse; 
}

void Emulation::usesMouseChanged(bool usesMouse) { 
    _usesMouse = usesMouse; 
}

bool Emulation::programBracketedPasteMode() const {
    return _bracketedPasteMode;
}

void Emulation::bracketedPasteModeChanged(bool bracketedPasteMode) {
    _bracketedPasteMode = bracketedPasteMode;
}

ScreenWindow *Emulation::createWindow() {
    ScreenWindow *window = new ScreenWindow();
    window->setScreen(_currentScreen);
    _windows << window;
    ExtendedCharTable::instance.windows << window;

    connect(window, &ScreenWindow::selectionChanged, this, &Emulation::bufferedUpdate);
    connect(this, &Emulation::outputChanged, window, &ScreenWindow::notifyOutputChanged);
    connect(this, &Emulation::handleCommandFromKeyboard, window, &ScreenWindow::handleCommandFromKeyboard);
    connect(this, &Emulation::handleCtrlC, window, &ScreenWindow::handleCtrlC);
    connect(this, &Emulation::outputFromKeypressEvent, window, &ScreenWindow::scrollToEnd);

    return window;
}

void Emulation::checkScreenInUse() {
    emit primaryScreenInUse(_currentScreen == _screen[0]);
}

Emulation::~Emulation() {
    QListIterator<ScreenWindow *> windowIter(_windows);

    while (windowIter.hasNext()) {
        auto win = windowIter.next();
        ExtendedCharTable::instance.windows.remove(win);
        delete win;
    }

    delete _screen[0];
    delete _screen[1];
}

void Emulation::setScreen(int n) {
    Screen *old = _currentScreen;
    _currentScreen = _screen[n & 1];
    if (_currentScreen != old) {
        for (ScreenWindow *window : std::as_const(_windows))
            window->setScreen(_currentScreen);
        checkScreenInUse();
    }
}

void Emulation::clearHistory() {
    _screen[0]->setScroll(_screen[0]->getScroll(), false);
}

void Emulation::setHistory(const HistoryType &t) {
    _screen[0]->setScroll(t);

    showBulk();
}

const HistoryType &Emulation::history() const {
    return _screen[0]->getScroll();
}

void Emulation::setCodec(QStringEncoder qtc) {
    if (qtc.isValid())
        _fromUtf16 = std::move(qtc);
    else
        setCodec(LocaleCodec);

    _toUtf16 = QStringDecoder{utf8() ? QStringConverter::Encoding::Utf8
                                    : QStringConverter::Encoding::System};
    emit useUtf8Request(utf8());
}

bool Emulation::utf8() const {
    const auto enc = QStringConverter::encodingForName(_fromUtf16.name());
    return enc && enc.value() == QStringConverter::Encoding::Utf8;
}

void Emulation::setCodec(EmulationCodec codec) {
    setCodec(QStringEncoder{codec == Utf8Codec
                              ? QStringConverter::Encoding::Utf8
                              : QStringConverter::Encoding::System});
}

void Emulation::setKeyBindings(const QString &name) {
    _keyTranslator = KeyboardTranslatorManager::instance()->findTranslator(name);
    if (!_keyTranslator) {
        _keyTranslator = KeyboardTranslatorManager::instance()->defaultTranslator();
    }
}

QString Emulation::keyBindings() const { 
    return _keyTranslator->name(); 
}

void Emulation::receiveChar(wchar_t c) {
    c &= 0xff;
    switch (c) {
        case '\b':
            _currentScreen->backspace();
            break;
        case '\t':
            _currentScreen->tab();
            break;
        case '\n':
            _currentScreen->newLine();
            break;
        case '\r':
            _currentScreen->toStartOfLine();
            break;
        case 0x07:
            emit stateSet(NOTIFYBELL);
            break;
        default:
            _currentScreen->displayCharacter(c);
            break;
    };
}

void Emulation::sendKeyEvent(QKeyEvent *ev, bool) {
    emit stateSet(NOTIFYNORMAL);

    if (!ev->text().isEmpty()) {
        emit sendData(ev->text().toUtf8().constData(), ev->text().length());
    }
}

void Emulation::sendString(const char *, int) {}

void Emulation::sendMouseEvent(int /*buttons*/, int /*column*/, int /*row*/, int /*eventType*/) {}

void Emulation::receiveData(const char *text, int length) {
    emit stateSet(NOTIFYACTIVITY);

    bufferedUpdate();

    QString utf16Text = _toUtf16(QByteArray::fromRawData(text, length));
    std::wstring unicodeText = utf16Text.toStdWString();

    for (wchar_t i : unicodeText)
        receiveChar(i);

    for (int i = 0; i < length; i++) {
        if (text[i] == '\030') {
            if ((length - i - 1 > 3) && (strncmp(text + i + 1, "B00", 3) == 0)) {
                emit zmodemSendDetected();
            }
            if ((length - i - 1 > 5) && (strncmp(text + i + 1, "B0100", 5) == 0)) {
                emit zmodemRecvDetected();
            }
        }
    }
}

void Emulation::dupDisplayCharacter(wchar_t cc) {
    if (cc == L'\n') {
        dupCache.append(L'\n');
        PlainTextDecoder decoder;
        QString lineText;
        QTextStream stream(&lineText);
        decoder.begin(&stream);
        Character *data = new Character[dupCache.size()];
        for (int j = 0; j < dupCache.size(); j++) {
            data[j] = Character(dupCache.at(j));
        }
        decoder.decodeLine(data, dupCache.size(), 0);
        decoder.end();
        delete[] data;
        emit dupDisplayOutput(lineText.toUtf8().constData(),
                            lineText.toUtf8().length());
        dupCache.clear();
    } else {
        dupCache.append(cc);
    }
}

void Emulation::writeToStream(TerminalCharacterDecoder *_decoder, int startLine,
                              int endLine) {
    _currentScreen->writeLinesToStream(_decoder, startLine, endLine);
}

int Emulation::lineCount() const {
    return _currentScreen->getLines() + _currentScreen->getHistLines();
}

void Emulation::showBulk() {
    _bulkTimer1.stop();
    _bulkTimer2.stop();

    emit outputChanged();

    _currentScreen->resetScrolledLines();
    _currentScreen->resetDroppedLines();
}

void Emulation::bufferedUpdate() {
    static const int BULK_TIMEOUT1 = 10;
    static const int BULK_TIMEOUT2 = 40;

    _bulkTimer1.setSingleShot(true);
    _bulkTimer1.start(BULK_TIMEOUT1);
    if (!_bulkTimer2.isActive()) {
        _bulkTimer2.setSingleShot(true);
        _bulkTimer2.start(BULK_TIMEOUT2);
    }
}

char Emulation::eraseChar() const { 
    return '\b'; 
}

void Emulation::setImageSize(int lines, int columns) {
    if ((lines < 1) || (columns < 1))
        return;

    QSize screenSize[2] = {
        QSize(_screen[0]->getColumns(), _screen[0]->getLines()),
        QSize(_screen[1]->getColumns(), _screen[1]->getLines())};
    QSize newSize(columns, lines);

    if (newSize == screenSize[0] && newSize == screenSize[1])
        return;

    _screen[0]->resizeImage(lines, columns);
    _screen[1]->resizeImage(lines, columns);

    emit imageSizeChanged(lines, columns);

    bufferedUpdate();
}

QSize Emulation::imageSize() const {
    return {_currentScreen->getColumns(), _currentScreen->getLines()};
}

uint ExtendedCharTable::extendedCharHash(uint* unicodePoints , ushort length) const {
    uint hash = 0;
    for (ushort i = 0; i < length; i++) {
        hash = 31 * hash + unicodePoints[i];
    }
    return hash;
}

bool ExtendedCharTable::extendedCharMatch(uint hash , uint* unicodePoints , ushort length) const {
    uint* entry = extendedCharTable[hash];

    if (entry == nullptr || entry[0] != length)
        return false;
    for (int i = 0; i < length; i++) {
        if (entry[i + 1] != unicodePoints[i])
        return false;
    }
    return true;
}

uint ExtendedCharTable::createExtendedChar(uint* unicodePoints , ushort length) {    
    uint hash = extendedCharHash(unicodePoints,length);
    const uint initialHash = hash;
    bool triedCleaningSolution = false;

    while (extendedCharTable.contains(hash) && hash != 0) {
        if (extendedCharMatch(hash, unicodePoints, length)) {
            return hash;
        } else {
            hash++;

            if (hash == initialHash) {
                if (!triedCleaningSolution) {
                    triedCleaningSolution = true;
                    QSet<uint> usedExtendedChars;
                    for (const auto &w : std::as_const(windows)) {
                        if (w->screen()) {
                            usedExtendedChars += w->screen()->usedExtendedChars();
                        }
                    }

                    QHash<uint,uint*>::iterator it = extendedCharTable.begin();
                    QHash<uint,uint*>::iterator itEnd = extendedCharTable.end();
                    while (it != itEnd) {
                        if (usedExtendedChars.contains(it.key())) {
                            ++it;
                        } else {
                            it = extendedCharTable.erase(it);
                        }
                    }
                } else {
                    return 0;
                }
            }
        }
    }

    uint* buffer = new uint[length+1];
    buffer[0] = length;
    for (int i = 0; i < length; i++)
        buffer[i + 1] = unicodePoints[i];

    extendedCharTable.insert(hash, buffer);

    return hash;
}

uint* ExtendedCharTable::lookupExtendedChar(uint hash , ushort& length) const {
    uint* buffer = extendedCharTable[hash];
    if (buffer) {
        length = buffer[0];
        return buffer + 1;
    } else {
        length = 0;
        return nullptr;
    }
}

ExtendedCharTable::ExtendedCharTable() {
}

ExtendedCharTable::~ExtendedCharTable() {
    QHashIterator<uint,uint*> iter(extendedCharTable);
    while (iter.hasNext()) {
        iter.next();
        delete[] iter.value();
    }
}

ExtendedCharTable ExtendedCharTable::instance;
