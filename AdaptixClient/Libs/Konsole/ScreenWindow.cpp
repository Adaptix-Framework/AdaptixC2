#include "ScreenWindow.h"
#include "Screen.h"
#include <QtDebug>

ScreenWindow::ScreenWindow(QObject *parent)
    : QObject(parent), _screen(nullptr), _windowBuffer(nullptr),
      _windowBufferSize(0), _bufferNeedsUpdate(true), _windowLines(1),
      _currentLine(0), _trackOutput(true), _scrollCount(0) {
}

ScreenWindow::~ScreenWindow() { 
    delete[] _windowBuffer; 
}

void ScreenWindow::setScreen(Screen *screen) {
  Q_ASSERT(screen);
  _screen = screen;
}

Screen *ScreenWindow::screen() const { 
    return _screen; 
}

Character *ScreenWindow::getImage() {
    int size = windowLines() * windowColumns();
    if (_windowBuffer == nullptr || _windowBufferSize != size) {
        delete[] _windowBuffer;
        _windowBufferSize = size;
        _windowBuffer = new Character[size];
        _bufferNeedsUpdate = true;
    }

    if (!_bufferNeedsUpdate)
        return _windowBuffer;

    _screen->getImage(_windowBuffer, size, currentLine(), endWindowLine());

    fillUnusedArea();

    _bufferNeedsUpdate = false;
    return _windowBuffer;
}

void ScreenWindow::fillUnusedArea() {
    int screenEndLine = _screen->getHistLines() + _screen->getLines() - 1;
    int windowEndLine = currentLine() + windowLines() - 1;

    int unusedLines = windowEndLine - screenEndLine;
    int charsToFill = unusedLines * windowColumns();

    Screen::fillWithDefaultChar(_windowBuffer + _windowBufferSize - charsToFill,
                                charsToFill);
}

int ScreenWindow::endWindowLine() const {
    return qMin(currentLine() + windowLines() - 1, lineCount() - 1);
}

QVector<LineProperty> ScreenWindow::getLineProperties() {
    QVector<LineProperty> result = _screen->getLineProperties(currentLine(), endWindowLine());

    if (result.count() != windowLines())
        result.resize(windowLines());

    return result;
}

QString ScreenWindow::selectedText(bool preserveLineBreaks) const {
    return _screen->selectedText(preserveLineBreaks);
}

void ScreenWindow::getSelectionStart(int &column, int &line) {
    _screen->getSelectionStart(column, line);
    line -= currentLine();
}

void ScreenWindow::getSelectionEnd(int &column, int &line) {
    _screen->getSelectionEnd(column, line);
    line -= currentLine();
}

void ScreenWindow::setSelectionStart(int column, int line, bool columnMode) {
    _screen->setSelectionStart(column, qMin(line + currentLine(), endWindowLine()), columnMode);

    _bufferNeedsUpdate = true;
    Q_EMIT selectionChanged();
}

void ScreenWindow::setSelectionEnd(int column, int line) {
    _screen->setSelectionEnd(column, qMin(line + currentLine(), endWindowLine()));

    _bufferNeedsUpdate = true;
    Q_EMIT selectionChanged();
}

bool ScreenWindow::isSelected(int column, int line) {
    return _screen->isSelected(column, qMin(line + currentLine(), endWindowLine()));
}

void ScreenWindow::clearSelection() {
    _screen->clearSelection();

    Q_EMIT selectionChanged();
}

bool ScreenWindow::isClearSelection() { 
    return _screen->isClearSelection(); 
}

void ScreenWindow::setWindowLines(int lines) {
    Q_ASSERT(lines > 0);
    _windowLines = lines;
}

int ScreenWindow::windowLines() const { 
    return _windowLines; 
}

int ScreenWindow::windowColumns() const { 
    return _screen->getColumns(); 
}

int ScreenWindow::lineCount() const {
    return _screen->getHistLines() + _screen->getLines();
}

int ScreenWindow::columnCount() const { 
    return _screen->getColumns(); 
}

QPoint ScreenWindow::cursorPosition() const {
    QPoint position;

    position.setX(_screen->getCursorX());
    position.setY(_screen->getCursorY());

    return position;
}

int ScreenWindow::getCursorX() const { return _screen->getCursorX(); }
int ScreenWindow::getCursorY() const { return _screen->getCursorY(); }
void ScreenWindow::setCursorX(int x) { _screen->setCursorX(x); }
void ScreenWindow::setCursorY(int y) { _screen->setCursorY(y); }

QString ScreenWindow::getScreenText(int row1, int col1, int row2, int col2, int mode) {
    return _screen->getScreenText(row1, col1, row2, col2, mode);
}

int ScreenWindow::currentLine() const {
    if (lineCount() >= windowLines()) {
        return qBound(0, _currentLine, lineCount() - windowLines());
    }
    return 0;
}

void ScreenWindow::scrollBy(RelativeScrollMode mode, int amount) {
    if (mode == ScrollLines) {
        scrollTo(currentLine() + amount);
    } else if (mode == ScrollPages) {
        scrollTo(currentLine() + amount * (windowLines() / 2));
    }
}

bool ScreenWindow::atEndOfOutput() const {
    return currentLine() == (lineCount() - windowLines());
}

void ScreenWindow::scrollTo(int line) {
    int maxCurrentLineNumber = lineCount() - windowLines();
    line = qBound(0, line, maxCurrentLineNumber);

    const int delta = line - _currentLine;
    _currentLine = line;

    _scrollCount += delta;

    _bufferNeedsUpdate = true;

    Q_EMIT scrolled(_currentLine);
}

void ScreenWindow::setTrackOutput(bool trackOutput) {
    _trackOutput = trackOutput;
}

bool ScreenWindow::trackOutput() const { return _trackOutput; }
int ScreenWindow::scrollCount() const { return _scrollCount; }
void ScreenWindow::resetScrollCount() { _scrollCount = 0; }

QRect ScreenWindow::scrollRegion() const {
    bool equalToScreenSize = windowLines() == _screen->getLines();

    if (atEndOfOutput() && equalToScreenSize)
        return _screen->lastScrolledRegion();
    else
        return {0, 0, windowColumns(), windowLines()};
}

void ScreenWindow::notifyOutputChanged() {
    if (_trackOutput) {
        _scrollCount -= _screen->scrolledLines();
        _currentLine = qMax(0, _screen->getHistLines() -
                                (windowLines() - _screen->getLines()));
    } else {
        _currentLine = qMax(0, _currentLine - _screen->droppedLines());

        _currentLine = qMin(_currentLine, _screen->getHistLines());
    }

    _bufferNeedsUpdate = true;

    Q_EMIT outputChanged();
}

void ScreenWindow::handleCommandFromKeyboard(
    KeyboardTranslator::Command command) {
    bool update = false;

    if (command & KeyboardTranslator::ScrollPageUpCommand) {
        scrollBy(ScreenWindow::ScrollPages, -1);
        update = true;
    }
    if (command & KeyboardTranslator::ScrollPageDownCommand) {
        scrollBy(ScreenWindow::ScrollPages, 1);
        update = true;
    }
    if (command & KeyboardTranslator::ScrollLineUpCommand) {
        scrollBy(ScreenWindow::ScrollLines, -1);
        update = true;
    }
    if (command & KeyboardTranslator::ScrollLineDownCommand) {
        scrollBy(ScreenWindow::ScrollLines, 1);
        update = true;
    }
    if (command & KeyboardTranslator::ScrollDownToBottomCommand) {
        Q_EMIT scrollToEnd();
        update = true;
    }
    if (command & KeyboardTranslator::ScrollUpToTopCommand) {
        scrollTo(0);
        update = true;
    }

    if (update) {
        setTrackOutput(atEndOfOutput());

        Q_EMIT outputChanged();
    }
}
