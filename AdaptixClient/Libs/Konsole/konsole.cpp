#include <QLayout>
#include <QBoxLayout>
#include <QDir>
#include <QMessageBox>

#include "util/CharacterColor.h"
#include "Screen.h"
#include "ScreenWindow.h"
#include "Emulation.h"
#include "TerminalDisplay.h"
#include "Vt102Emulation.h"
#include "util/KeyboardTranslator.h"
#include "util/ColorScheme.h"
#include "util/SearchBar.h"
#include "konsole.h"


QTermWidget::QTermWidget(QWidget *messageParentWidget, QWidget *parent)
    : QWidget(parent) {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);

    m_terminalDisplay = new TerminalDisplay(this);
    m_emulation = new Vt102Emulation();
    m_terminalDisplay->setBellMode(TerminalDisplay::SystemBeepBell);
    m_terminalDisplay->setTerminalSizeHint(true);
    m_terminalDisplay->setTripleClickMode(TerminalDisplay::SelectWholeLine);
    m_terminalDisplay->setTerminalSizeStartup(true);

    connect(m_terminalDisplay, &TerminalDisplay::keyPressedSignal, m_emulation, &Emulation::sendKeyEvent);
    connect(m_terminalDisplay, &TerminalDisplay::mouseSignal, m_emulation, &Emulation::sendMouseEvent);
    connect(m_terminalDisplay, &TerminalDisplay::sendStringToEmu, this, [this](const char* s){
        m_emulation->sendString(s);
    });
    connect(m_emulation, &Emulation::stateSet, this, &QTermWidget::activityStateSet);
    m_monitorTimer = new QTimer(this);
    m_monitorTimer->setSingleShot(true);
    connect(m_monitorTimer, &QTimer::timeout, this, &QTermWidget::monitorTimerDone);
    connect(m_emulation, &Emulation::programUsesMouseChanged, m_terminalDisplay, &TerminalDisplay::setUsesMouse);
    m_terminalDisplay->setUsesMouse(m_emulation->programUsesMouse());
    connect(m_emulation, &Emulation::programBracketedPasteModeChanged, m_terminalDisplay, &TerminalDisplay::setBracketedPasteMode);
    m_terminalDisplay->setBracketedPasteMode(m_emulation->programBracketedPasteMode());
    m_terminalDisplay->setScreenWindow(m_emulation->createWindow());
    connect(m_emulation, &Emulation::primaryScreenInUse, m_terminalDisplay, &TerminalDisplay::usingPrimaryScreen);
    connect(m_emulation, &Emulation::imageSizeChanged, this, [this](int /*height*/, int /*width*/){
        updateTerminalSize();
    });
    connect(m_terminalDisplay, &TerminalDisplay::changedContentSizeSignal, this, [this](int /*height*/, int /*width*/){
        updateTerminalSize();
    });

    setFlowControlEnabled(true);
    m_emulation->setCodec(QStringEncoder{QStringConverter::Encoding::Utf8});
    m_emulation->setHistory(HistoryTypeBuffer(1000));
    m_emulation->setKeyBindings(QString());

    m_layout->addWidget(m_terminalDisplay);
    m_terminalDisplay->setObjectName("terminalDisplay");
    setMessageParentWidget(messageParentWidget?messageParentWidget:this);

    connect(m_terminalDisplay, &TerminalDisplay::notifyBell, this, &QTermWidget::notifyBell);
    connect(m_terminalDisplay, &TerminalDisplay::handleCtrlC, this, &QTermWidget::handleCtrlC);
    connect(m_terminalDisplay, &TerminalDisplay::changedContentCountSignal, this, &QTermWidget::termSizeChange);
    connect(m_terminalDisplay, &TerminalDisplay::mousePressEventForwarded, this, &QTermWidget::mousePressEventForwarded);
    connect(m_emulation, &Emulation::profileChangeCommandReceived, this, &QTermWidget::profileChanged);
    connect(m_emulation, &Emulation::zmodemRecvDetected, this, &QTermWidget::zmodemRecvDetected);
    connect(m_emulation, &Emulation::zmodemSendDetected, this, &QTermWidget::zmodemSendDetected);
    connect(m_emulation, &Emulation::titleChanged, this, &QTermWidget::titleChanged);
    connect(m_emulation, &Emulation::sendData, this, [this](const char *buff, int len) {
        if (m_echo) {
            recvData(buff, len);
        }
        emit sendData(buff, len);
    });
    connect( m_emulation, &Emulation::dupDisplayOutput, this, &QTermWidget::dupDisplayOutput);
    connect( m_emulation, &Emulation::changeTabTextColorRequest, this, &QTermWidget::changeTabTextColorRequest);
    connect( m_emulation, &Emulation::cursorChanged, this, &QTermWidget::cursorChanged);

    m_urlFilter = new UrlFilter();
    connect(m_urlFilter, &UrlFilter::activated, this, &QTermWidget::urlActivated);
    m_terminalDisplay->filterChain()->addFilter(m_urlFilter);
    m_UrlFilterEnable = true;

    m_searchBar = new SearchBar(this);
    m_searchBar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    connect(m_searchBar, &SearchBar::searchCriteriaChanged, this, [this](){
        search(true, false);
    });
    connect(m_searchBar, &SearchBar::findNext, this, [this](){
        search(true, true);
    });
    connect(m_searchBar, &SearchBar::findPrevious, this, [this](){
        search(false, false);
    });
    m_layout->addWidget(m_searchBar);
    m_searchBar->hide();
    QString style_sheet = qApp->styleSheet();
    m_searchBar->setStyleSheet(style_sheet);
    
    this->setFocus( Qt::OtherFocusReason );
    this->setFocusPolicy( Qt::WheelFocus );
    m_terminalDisplay->resize(this->size());

    this->setFocusProxy(m_terminalDisplay);
    connect(m_terminalDisplay, &TerminalDisplay::copyAvailable,
            this, &QTermWidget::copyAvailable);
    connect(m_terminalDisplay, &TerminalDisplay::termGetFocus,
            this, &QTermWidget::termGetFocus);
    connect(m_terminalDisplay, &TerminalDisplay::termLostFocus,
            this, &QTermWidget::termLostFocus);
    connect(m_terminalDisplay, &TerminalDisplay::keyPressedSignal, this, [this] (QKeyEvent* e, bool) { 
        emit termKeyPressed(e); 
    });

    setScrollBarPosition(NoScrollBar);
    setKeyboardCursorShape(Emulation::KeyboardCursorShape::BlockCursor);

    connect(m_emulation, &Emulation::imageResizeRequest, this, [this](const QSize& size){
        if ((size.width() <= 1) || (size.height() <= 1)) {
            return;
        }
        setSize(size);
    });
}

QTermWidget::~QTermWidget() {
    setUrlFilterEnabled(false);
    clearHighLightTexts();
    delete m_urlFilter;
    delete m_searchBar;
    emit destroyed();
    delete m_emulation;
}

void QTermWidget::selectionChanged(bool textSelected) {
    emit copyAvailable(textSelected);
}

void QTermWidget::search(bool forwards, bool next) {
    int startColumn, startLine;

    if (next) {
        m_terminalDisplay->screenWindow()->screen()->getSelectionEnd(startColumn, startLine);
        startColumn++;
    } else {
        m_terminalDisplay->screenWindow()->screen()->getSelectionStart(startColumn, startLine);
    }

    QRegularExpression regExp;
    if (m_searchBar->useRegularExpression()) {
        regExp.setPattern(m_searchBar->searchText());
    } else {
        regExp.setPattern(QRegularExpression::escape(m_searchBar->searchText()));
    }
    regExp.setPatternOptions(m_searchBar->matchCase() ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);

    HistorySearch *historySearch =
            new HistorySearch(m_emulation, regExp, forwards, startColumn, startLine, this);
    connect(historySearch, &HistorySearch::matchFound, this, [this](int startColumn, int startLine, int endColumn, int endLine){
        ScreenWindow* sw = m_terminalDisplay->screenWindow();
        sw->scrollTo(startLine);
        sw->setTrackOutput(false);
        sw->notifyOutputChanged();
        sw->setSelectionStart(startColumn, startLine - sw->currentLine(), false);
        sw->setSelectionEnd(endColumn, endLine - sw->currentLine());
    });
    connect(historySearch, &HistorySearch::noMatchFound, this, [this](){
        m_terminalDisplay->screenWindow()->clearSelection();
    });
    connect(historySearch, &HistorySearch::noMatchFound, m_searchBar, &SearchBar::noMatchFound);
    historySearch->search();
}

QSize QTermWidget::sizeHint() const {
    QSize size = m_terminalDisplay->sizeHint();
    size.rheight() = 150;
    return size;
}

void QTermWidget::setTerminalSizeHint(bool enabled) {
    m_terminalDisplay->setTerminalSizeHint(enabled);
}

bool QTermWidget::terminalSizeHint() {
    return m_terminalDisplay->terminalSizeHint();
}

void QTermWidget::setTerminalFont(const QFont &font) {
    m_terminalDisplay->setVTFont(font);
}

QFont QTermWidget::getTerminalFont() {
    return m_terminalDisplay->getVTFont();
}

void QTermWidget::setTerminalOpacity(qreal level) {
    m_terminalDisplay->setOpacity(level);
}

void QTermWidget::setTerminalBackgroundImage(const QString& backgroundImage) {
    m_terminalDisplay->setBackgroundImage(backgroundImage);
}

void QTermWidget::setTerminalBackgroundMovie(const QString& backgroundMovie) {
    m_terminalDisplay->setBackgroundMovie(backgroundMovie);
}

void QTermWidget::setTerminalBackgroundVideo(const QString& backgroundVideo) {
    m_terminalDisplay->setBackgroundVideo(backgroundVideo);
}

void QTermWidget::setTerminalBackgroundMode(int mode) {
    m_terminalDisplay->setBackgroundMode((BackgroundMode)mode);
}

void QTermWidget::setTextCodec(QStringEncoder codec) {
    m_emulation->setCodec(std::move(codec));
}

void QTermWidget::setColorScheme(const QString& origName) {
    const ColorScheme *cs = nullptr;

    const bool isFile = QFile::exists(origName);
    const QString& name = isFile ? QFileInfo(origName).baseName() : origName;

    if (!availableColorSchemes().contains(name)) {
        if (isFile) {
            if (ColorSchemeManager::instance()->loadCustomColorScheme(origName))
                cs = ColorSchemeManager::instance()->findColorScheme(name);
        }

        if (!cs)
            cs = ColorSchemeManager::instance()->defaultColorScheme();
    } else {
        cs = ColorSchemeManager::instance()->findColorScheme(name);
    }

    if (! cs) {
        QMessageBox::information(messageParentWidget,
                                 tr("Color Scheme Error"),
                                 tr("Cannot load color scheme: %1").arg(name));
        return;
    }
    ColorEntry table[TABLE_COLORS];
    cs->getColorTable(table);
    m_terminalDisplay->setColorTable(table);
    m_hasDarkBackground = cs->hasDarkBackground();
}

QStringList QTermWidget::getAvailableColorSchemes() {
   return QTermWidget::availableColorSchemes();
}

QStringList QTermWidget::availableColorSchemes() {
    QStringList ret;
    const auto allColorSchemes = ColorSchemeManager::instance()->allColorSchemes();
    for (const ColorScheme* cs : allColorSchemes)
        ret.append(cs->name());
    return ret;
}

void QTermWidget::setBackgroundColor(const QColor &color) {
    m_terminalDisplay->setBackgroundColor(color);
}

void QTermWidget::setForegroundColor(const QColor &color) {
    m_terminalDisplay->setForegroundColor(color);
}

void QTermWidget::setANSIColor(const int ansiColorId, const QColor &color) {
    m_terminalDisplay->setColorTableColor(ansiColorId, color);
}

void QTermWidget::setPreeditColorIndex(int index) {
    m_terminalDisplay->setPreeditColorIndex(index);
}

void QTermWidget::setSize(const QSize &size) {
    m_terminalDisplay->setSize(size.width(), size.height());
}

void QTermWidget::setHistorySize(int lines) {
    if (lines <= 0)
        m_emulation->setHistory(HistoryTypeNone());
    else
        m_emulation->setHistory(HistoryTypeBuffer(lines));
}

int QTermWidget::historySize() const {
    const HistoryType& currentHistory = m_emulation->history();

     if (currentHistory.isEnabled()) {
         if (currentHistory.isUnlimited()) {
             return -1;
         } else {
             return currentHistory.maximumLineCount();
         }
     } else {
         return 0;
     }
}

void QTermWidget::setScrollBarPosition(ScrollBarPosition pos) {
    m_terminalDisplay->setScrollBarPosition(pos);
}

void QTermWidget::scrollToEnd() {
    m_terminalDisplay->scrollToEnd();
}

void QTermWidget::sendText(const QString &text) {
    m_emulation->sendText(text);
}

void QTermWidget::sendKeyEvent(QKeyEvent *e) {
    m_emulation->sendKeyEvent(e, false);
}

void QTermWidget::resizeEvent(QResizeEvent*) {
    m_terminalDisplay->resize(this->size());
}

void QTermWidget::sessionFinished() {
    emit finished();
}

void QTermWidget::updateTerminalSize() {
    int minLines = -1;
    int minColumns = -1;

    const int VIEW_LINES_THRESHOLD = 2;
    const int VIEW_COLUMNS_THRESHOLD = 2;

    if ( m_terminalDisplay->isHidden() == false &&
            m_terminalDisplay->lines() >= VIEW_LINES_THRESHOLD &&
            m_terminalDisplay->columns() >= VIEW_COLUMNS_THRESHOLD ) {
        minLines = (minLines == -1) ? m_terminalDisplay->lines() : qMin( minLines , m_terminalDisplay->lines() );
        minColumns = (minColumns == -1) ? m_terminalDisplay->columns() : qMin( minColumns , m_terminalDisplay->columns() );
    }

    if ( minLines > 0 && minColumns > 0 ) {
        m_emulation->setImageSize( minLines , minColumns );
    }
}

void QTermWidget::monitorTimerDone() {
    if (m_monitorSilence) {
        emit silence();
        emit stateChanged(NOTIFYSILENCE);
    } else {
        emit stateChanged(NOTIFYNORMAL);
    }

    m_notifiedActivity=false;
}

void QTermWidget::activityStateSet(int state) {
    if (state==NOTIFYBELL) {
        m_terminalDisplay->bell();
    } else if (state==NOTIFYACTIVITY) {
        if (m_monitorSilence) {
            m_monitorTimer->start(m_silenceSeconds*1000);
        }

        if ( m_monitorActivity ) {
            if (!m_notifiedActivity) {
                m_notifiedActivity=true;
                emit activity();
            }
        }
    }

    if ( state==NOTIFYACTIVITY && !m_monitorActivity ) {
        state = NOTIFYNORMAL;
    }
    if ( state==NOTIFYSILENCE && !m_monitorSilence ) {
        state = NOTIFYNORMAL;
    }

    emit stateChanged(state);
}

void QTermWidget::setMonitorActivity(bool enabled) {
    m_monitorActivity=enabled;
    m_notifiedActivity=false;

    activityStateSet(NOTIFYNORMAL);
}

void QTermWidget::setMonitorSilence(bool enabled) {
    if (m_monitorSilence==enabled) {
        return;
    }

    m_monitorSilence=enabled;
    if (m_monitorSilence) {
        m_monitorTimer->start(m_silenceSeconds*1000);
    } else {
        m_monitorTimer->stop();
    }

    activityStateSet(NOTIFYNORMAL);
}

void QTermWidget::setSilenceTimeout(int seconds) {
    m_silenceSeconds=seconds;
    if (m_monitorSilence) {
        m_monitorTimer->start(m_silenceSeconds*1000);
    }
}

void QTermWidget::bracketText(QString& text) {
    m_terminalDisplay->bracketText(text);
}

void QTermWidget::disableBracketedPasteMode(bool disable) {
    m_terminalDisplay->disableBracketedPasteMode(disable);
}

bool QTermWidget::bracketedPasteModeIsDisabled() const {
    return m_terminalDisplay->bracketedPasteModeIsDisabled();
}

void QTermWidget::copyClipboard() {
    m_terminalDisplay->copyClipboard(QClipboard::Clipboard);
}

void QTermWidget::copySelection() {
    m_terminalDisplay->copyClipboard(QClipboard::Selection);
}

void QTermWidget::pasteClipboard() {
    m_terminalDisplay->pasteClipboard();
}

void QTermWidget::pasteSelection() {
    m_terminalDisplay->pasteSelection();
}

void QTermWidget::selectAll() {
    m_terminalDisplay->selectAll();
}

int QTermWidget::setZoom(int step) {
    QFont font = m_terminalDisplay->getVTFont();

    font.setPointSize(font.pointSize() + step);
    setTerminalFont(font);
    return font.pointSize();
}

int QTermWidget::zoomIn() {
    return setZoom(STEP_ZOOM);
}

int QTermWidget::zoomOut() {
    return setZoom(-STEP_ZOOM);
}

void QTermWidget::setKeyBindings(const QString & kb) {
    m_emulation->setKeyBindings(kb);
}

void QTermWidget::clear() {
    clearScreen();
    clearScrollback();
}

void QTermWidget::clearScrollback() {
    m_emulation->clearHistory();
}

void QTermWidget::clearScreen() {
    m_emulation->reset();
}

void QTermWidget::setFlowControlEnabled(bool enabled) {
    if (m_flowControl == enabled)
        return;

    m_flowControl = enabled;

    emit flowControlEnabledChanged(enabled);
}

bool QTermWidget::flowControlEnabled(void) {
    return m_flowControl;
}

void QTermWidget::setFlowControlWarningEnabled(bool enabled) {
    if (flowControlEnabled())
        m_terminalDisplay->setFlowControlWarningEnabled(enabled);
}

QStringList QTermWidget::availableKeyBindings() {
    return KeyboardTranslatorManager::instance()->allTranslators();
}

QString QTermWidget::keyBindings() {
    return m_emulation->keyBindings();
}

void QTermWidget::toggleShowSearchBar() {
    if(m_searchBar->isHidden()) {
        m_searchBar->setText(selectedText(true));
        m_searchBar->show();
    } else {
        m_searchBar->hide();
    }
}

void QTermWidget::setMotionAfterPasting(int action) {
    m_terminalDisplay->setMotionAfterPasting((MotionAfterPasting) action);
}

int QTermWidget::historyLinesCount() {
    return m_terminalDisplay->screenWindow()->screen()->getHistLines();
}

int QTermWidget::screenColumnsCount() {
    return m_terminalDisplay->screenWindow()->screen()->getColumns();
}

int QTermWidget::screenLinesCount() {
    return m_terminalDisplay->screenWindow()->screen()->getLines();
}

void QTermWidget::setSelectionStart(int row, int column) {
    m_terminalDisplay->screenWindow()->screen()->setSelectionStart(column, row, true);
}

void QTermWidget::setSelectionEnd(int row, int column) {
    m_terminalDisplay->screenWindow()->screen()->setSelectionEnd(column, row);
}

void QTermWidget::getSelectionStart(int& row, int& column) {
    m_terminalDisplay->screenWindow()->screen()->getSelectionStart(column, row);
}

void QTermWidget::getSelectionEnd(int& row, int& column) {
    m_terminalDisplay->screenWindow()->screen()->getSelectionEnd(column, row);
}

QString QTermWidget::selectedText(bool preserveLineBreaks) {
    return m_terminalDisplay->screenWindow()->screen()->selectedText(preserveLineBreaks);
}

Filter::HotSpot* QTermWidget::getHotSpotAt(const QPoint &pos) const {
    int row = 0, column = 0;
    m_terminalDisplay->getCharacterPosition(pos, row, column);
    return getHotSpotAt(row, column);
}

Filter::HotSpot* QTermWidget::getHotSpotAt(int row, int column) const {
    return m_terminalDisplay->filterChain()->hotSpotAt(row, column);
}

QList<QAction*> QTermWidget::filterActions(const QPoint& position) {
    return m_terminalDisplay->filterActions(position);
}

int QTermWidget::recvData(const char *buff, int len) const {
    m_emulation->receiveData( buff, len );
    return len;
}

void QTermWidget::setKeyboardCursorShape(KeyboardCursorShape shape) {
    m_terminalDisplay->setKeyboardCursorShape(shape);
}

void QTermWidget::setKeyboardCursorShape(uint32_t shape) {
    m_terminalDisplay->setKeyboardCursorShape((KeyboardCursorShape)shape);
}

void QTermWidget::setBlinkingCursor(bool blink) {
    m_terminalDisplay->setBlinkingCursor(blink);
}

void QTermWidget::setBidiEnabled(bool enabled) {
    m_terminalDisplay->setBidiEnabled(enabled);
}

bool QTermWidget::isBidiEnabled() {
    return m_terminalDisplay->isBidiEnabled();
}

void QTermWidget::cursorChanged(Emulation::KeyboardCursorShape cursorShape, bool blinkingCursorEnabled) {
    setKeyboardCursorShape(cursorShape);
    setBlinkingCursor(blinkingCursorEnabled);
}

void QTermWidget::setMargin(int margin) {
    m_terminalDisplay->setMargin(margin);
}

int QTermWidget::getMargin() const {
    return m_terminalDisplay->margin();
}

void QTermWidget::saveHistory(QTextStream *stream, int format, int start, int end) {
    TerminalCharacterDecoder *decoder;
    if(format == 0) {
        decoder = new PlainTextDecoder;
    } else {
        decoder = new HTMLDecoder;
    }
    decoder->begin(stream);
    if(start < 0) {
        start = 0;
    }
    if(end < 0) {
        end = m_emulation->lineCount();
    }
    m_emulation->writeToStream(decoder, start, end);
    delete decoder;
}

void QTermWidget::saveHistory(QIODevice *device, int format, int start, int end) {
    QTextStream stream(device);
    saveHistory(&stream, format, start, end);
}

void QTermWidget::screenShot(QPixmap *pixmap) {
    QPixmap currPixmap(m_terminalDisplay->size());
    m_terminalDisplay->render(&currPixmap);
    *pixmap = currPixmap.scaled(pixmap->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void QTermWidget::repaintDisplay(void) {
    m_terminalDisplay->repaintDisplay();
}

void QTermWidget::screenShot(const QString &fileName) {
    qreal deviceratio = m_terminalDisplay->devicePixelRatio();
    deviceratio = deviceratio*2;
    QPixmap pixmap(m_terminalDisplay->size() * deviceratio);
    pixmap.setDevicePixelRatio(deviceratio);
    m_terminalDisplay->render(&pixmap);
    pixmap.save(fileName);
}

void QTermWidget::setLocked(bool enabled) {
    this->setEnabled(!enabled);
    m_terminalDisplay->setLocked(enabled);
}

void QTermWidget::setDrawLineChars(bool drawLineChars) {
    m_terminalDisplay->setDrawLineChars(drawLineChars);
}

void QTermWidget::setBoldIntense(bool boldIntense) {
    m_terminalDisplay->setBoldIntense(boldIntense);
}

void QTermWidget::setConfirmMultilinePaste(bool confirmMultilinePaste) {
    m_terminalDisplay->setConfirmMultilinePaste(confirmMultilinePaste);
}

void QTermWidget::setTrimPastedTrailingNewlines(bool trimPastedTrailingNewlines) {
    m_terminalDisplay->setTrimPastedTrailingNewlines(trimPastedTrailingNewlines);
}

void QTermWidget::setEcho(bool echo) {
    m_echo = echo;
}

void QTermWidget::setKeyboardCursorColor(bool useForegroundColor, const QColor& color) {
    m_terminalDisplay->setKeyboardCursorColor(useForegroundColor, color);
}

void QTermWidget::addHighLightText(const QString &text, const QColor &color) {
    for (int i = 0; i < m_highLightTexts.size(); i++) {
        if (m_highLightTexts.at(i)->text == text) {
            return;
        }
    }
    HighLightText *highLightText = new HighLightText(text,color);
    m_highLightTexts.append(highLightText);
    m_terminalDisplay->filterChain()->addFilter(highLightText->regExpFilter);
    m_terminalDisplay->updateFilters();
    m_terminalDisplay->repaint();
}

QMap<QString, QColor> QTermWidget::getHighLightTexts(void) {
    QMap<QString, QColor> highLightTexts;
    for (int i = 0; i < m_highLightTexts.size(); i++) {
        highLightTexts.insert(m_highLightTexts.at(i)->text, m_highLightTexts.at(i)->color);
    }
    return highLightTexts;
}

bool QTermWidget::isContainHighLightText(const QString &text) {
    for (int i = 0; i < m_highLightTexts.size(); i++) {
        if (m_highLightTexts.at(i)->text == text) {
            return true;
        }
    }
    return false;
}

void QTermWidget::removeHighLightText(const QString &text) {
    for (int i = 0; i < m_highLightTexts.size(); i++) {
        if (m_highLightTexts.at(i)->text == text) {
            m_terminalDisplay->filterChain()->removeFilter(m_highLightTexts.at(i)->regExpFilter);
            delete m_highLightTexts.at(i);
            m_highLightTexts.removeAt(i);
            m_terminalDisplay->updateFilters();
            break;
        }
    }
    m_terminalDisplay->repaint();
}

void QTermWidget::clearHighLightTexts(void) {
    for (int i = 0; i < m_highLightTexts.size(); i++) {
        m_terminalDisplay->filterChain()->removeFilter(m_highLightTexts.at(i)->regExpFilter);
        delete m_highLightTexts.at(i);
    }
    m_terminalDisplay->updateFilters();
    m_highLightTexts.clear();
    m_terminalDisplay->repaint();
}

void QTermWidget::setWordCharacters(const QString &wordCharacters) {
    m_terminalDisplay->setWordCharacters(wordCharacters);
}

QString QTermWidget::wordCharacters(void) {
    return m_terminalDisplay->wordCharacters();
}

void QTermWidget::autoHideMouseAfter(int delay)
{
    m_terminalDisplay->autoHideMouseAfter(delay);
}

void QTermWidget::setShowResizeNotificationEnabled(bool enabled) {
    m_terminalDisplay->setShowResizeNotificationEnabled(enabled);
}

void QTermWidget::setEnableHandleCtrlC(bool enable) {
    m_emulation->setEnableHandleCtrlC(enable);
}

int QTermWidget::lines() {
    return m_terminalDisplay->lines();
}

int QTermWidget::columns() {
    return m_terminalDisplay->columns();
}

int QTermWidget::getCursorX() {
    return m_terminalDisplay->getCursorX();
}

int QTermWidget::getCursorY() {
    return m_terminalDisplay->getCursorY();
}

void QTermWidget::setCursorX(int x) {
    m_terminalDisplay->setCursorX(x);
}

void QTermWidget::setCursorY(int y) {
    m_terminalDisplay->setCursorY(y);
}

QString QTermWidget::screenGet(int row1, int col1, int row2, int col2, int mode) {
    return m_terminalDisplay->screenGet(row1, col1, row2, col2, mode);
}

void QTermWidget::setSelectionOpacity(qreal opacity) {
    m_terminalDisplay->setSelectionOpacity(opacity);
}

void QTermWidget::setUrlFilterEnabled(bool enable) {
    if(m_UrlFilterEnable == enable) {
        return;
    }
    if(enable) {
        m_terminalDisplay->filterChain()->addFilter(m_urlFilter);
    } else {
        m_terminalDisplay->filterChain()->removeFilter(m_urlFilter);
    }
}

void QTermWidget::setMessageParentWidget(QWidget *parent) {
    messageParentWidget = parent;
    m_terminalDisplay->setMessageParentWidget(messageParentWidget);
}

void QTermWidget::reTranslateUi(void) {
    m_searchBar->retranslateUi();
}

void QTermWidget::set_fix_quardCRT_issue33(bool fix) {
    m_terminalDisplay->set_fix_quardCRT_issue33(fix);
}
