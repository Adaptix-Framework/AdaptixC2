#include "Filter.h"

#include <iostream>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QSharedData>
#include <QString>
#include <QTextStream>
#include <QUrl>
#include <QtAlgorithms>

#include "CharWidth.h"
#include "TerminalCharacterDecoder.h"
#include "../konsole.h"

FilterChain::~FilterChain() {
}

void FilterChain::addFilter(Filter *filter) {
    if (!containsFilter(filter))
        append(filter);
}

void FilterChain::removeFilter(Filter *filter) { 
    removeAll(filter); 
}

bool FilterChain::containsFilter(Filter *filter) { 
    return contains(filter); 
}

void FilterChain::reset() {
    QListIterator<Filter *> iter(*this);
    while (iter.hasNext())
        iter.next()->reset();
}

void FilterChain::setBuffer(const QString *buffer,
                            const QList<int> *linePositions) {
    QListIterator<Filter *> iter(*this);
    while (iter.hasNext())
        iter.next()->setBuffer(buffer, linePositions);
}

void FilterChain::process() {
    QListIterator<Filter *> iter(*this);
    while (iter.hasNext())
        iter.next()->process();
}

void FilterChain::clear() { 
    QList<Filter *>::clear(); 
}

Filter::HotSpot *FilterChain::hotSpotAt(int line, int column) const {
    QListIterator<Filter *> iter(*this);
    while (iter.hasNext()) {
        Filter *filter = iter.next();
        Filter::HotSpot *spot = filter->hotSpotAt(line, column);
        if (spot != nullptr) {
        return spot;
        }
    }

    return nullptr;
}

QList<Filter::HotSpot *> FilterChain::hotSpots() const {
    QList<Filter::HotSpot *> list;
    QListIterator<Filter *> iter(*this);
    while (iter.hasNext()) {
        Filter *filter = iter.next();
        list << filter->hotSpots();
    }
    return list;
}

TerminalImageFilterChain::TerminalImageFilterChain()
    : _buffer(nullptr), _linePositions(nullptr) {
}

TerminalImageFilterChain::~TerminalImageFilterChain() {
    delete _buffer;
    delete _linePositions;
}

void TerminalImageFilterChain::setImage(
    const Character *const image, int lines, int columns,
    const QVector<LineProperty> &lineProperties) {
    if (empty())
        return;

    reset();

    PlainTextDecoder decoder;
    decoder.setTrailingWhitespace(true);

    QString *newBuffer = new QString();
    QList<int> *newLinePositions = new QList<int>();
    setBuffer(newBuffer, newLinePositions);

    delete _buffer;
    delete _linePositions;

    _buffer = newBuffer;
    _linePositions = newLinePositions;

    QTextStream lineStream(_buffer);
    decoder.begin(&lineStream);

    for (int i = 0; i < lines; i++) {
        _linePositions->append(_buffer->length());
        decoder.decodeLine(image + i * columns, columns, LINE_DEFAULT);

        if (!(lineProperties.value(i, LINE_DEFAULT) & LINE_WRAPPED))
        lineStream << QLatin1Char('\n');
    }
    decoder.end();
}

Filter::Filter() : QObject(nullptr) {
}

Filter::~Filter() {
    qDeleteAll(_hotspotList);
    _hotspotList.clear();
}

void Filter::reset() {
    qDeleteAll(_hotspotList);
    _hotspots.clear();
    _hotspotList.clear();
}

void Filter::setBuffer(const QString *buffer, const QList<int> *linePositions) {
    _buffer = buffer;
    _linePositions = linePositions;
}

void Filter::getLineColumn(int position, int &startLine, int &startColumn) {
    Q_ASSERT(_linePositions);
    Q_ASSERT(_buffer);

    for (int i = 0; i < _linePositions->count(); i++) {
        int nextLine = 0;

        if (i == _linePositions->count() - 1)
        nextLine = _buffer->length() + 1;
        else
        nextLine = _linePositions->value(i + 1);

        if (_linePositions->value(i) <= position && position < nextLine) {
            startLine = i;
            startColumn = CharWidth::string_unicode_width(buffer()->mid(
                _linePositions->value(i), position - _linePositions->value(i)));
            return;
        }
    }
}

const QString *Filter::buffer() { 
    return _buffer; 
}

Filter::HotSpot::~HotSpot() {
}

void Filter::addHotSpot(HotSpot *spot) {
    _hotspotList << spot;

    for (int line = spot->startLine(); line <= spot->endLine(); line++) {
        _hotspots.insert(line, spot);
    }
}

QList<Filter::HotSpot *> Filter::hotSpots() const { 
    return _hotspotList; 
}

QList<Filter::HotSpot *> Filter::hotSpotsAtLine(int line) const {
    return _hotspots.values(line);
}

Filter::HotSpot *Filter::hotSpotAt(int line, int column) const {
    QListIterator<HotSpot *> spotIter(_hotspots.values(line));

    while (spotIter.hasNext()) {
        HotSpot *spot = spotIter.next();

        if (spot->startLine() == line && spot->startColumn() > column)
            continue;
        if (spot->endLine() == line && spot->endColumn() < column)
            continue;

        return spot;
    }

    return nullptr;
}

Filter::HotSpot::HotSpot(int startLine, int startColumn, int endLine, int endColumn)
    : _startLine(startLine), _startColumn(startColumn), _endLine(endLine),
      _endColumn(endColumn), _type(NotSpecified), _color(Qt::red) {}
QList<QAction *> Filter::HotSpot::actions() { return QList<QAction *>(); }
int Filter::HotSpot::startLine() const { return _startLine; }
int Filter::HotSpot::endLine() const { return _endLine; }
int Filter::HotSpot::startColumn() const { return _startColumn; }
int Filter::HotSpot::endColumn() const { return _endColumn; }
Filter::HotSpot::Type Filter::HotSpot::type() const { return _type; }
QColor Filter::HotSpot::color() const { return _color; }
void Filter::HotSpot::setType(Type type) { _type = type; }
void Filter::HotSpot::setColor(const QColor &color) { _color = color; }

RegExpFilter::RegExpFilter() : Filter() {
}

RegExpFilter::HotSpot::HotSpot(int startLine, int startColumn, int endLine,
                               int endColumn)
    : Filter::HotSpot(startLine, startColumn, endLine, endColumn) {
    setType(Marker);
}

void RegExpFilter::HotSpot::clickAction(void) {}
QString RegExpFilter::HotSpot::clickActionToolTip(void) { return QString(); }
bool RegExpFilter::HotSpot::hasClickAction(void) { return false; }
void RegExpFilter::HotSpot::setCapturedTexts(const QStringList &texts) {
  _capturedTexts = texts;
}
QStringList RegExpFilter::HotSpot::capturedTexts() const {
  return _capturedTexts;
}
void RegExpFilter::setRegExp(const QRegularExpression &regExp) {
  _searchText = regExp;
}
QRegularExpression RegExpFilter::regExp() const { return _searchText; }

void RegExpFilter::process() {
    const QString *text = buffer();

    Q_ASSERT(text);

    static const QString emptyString;
    auto match = _searchText.match(emptyString, 0, QRegularExpression::NormalMatch, QRegularExpression::AnchorAtOffsetMatchOption);
    if (match.hasMatch())
        return;

    match = _searchText.match(*text);

    while (match.hasMatch()) {
        int startLine = 0;
        int endLine = 0;
        int startColumn = 0;
        int endColumn = 0;

        QStringList captureList;
        for (int i = 0; i <= match.lastCapturedIndex(); i++) {
            QString text = match.captured(i);
            captureList.append(text);
        }

        getLineColumn(match.capturedStart(), startLine, startColumn);
        getLineColumn(match.capturedEnd(), endLine, endColumn);

        RegExpFilter::HotSpot *spot = newHotSpot(startLine, startColumn, endLine, endColumn);
        spot->setCapturedTexts(captureList);

        addHotSpot(spot);

        if (match.capturedLength() == 0) {
            break;
        }

        match = _searchText.match(*text, match.capturedEnd());
    }
}

RegExpFilter::HotSpot *RegExpFilter::newHotSpot(int startLine, int startColumn, int endLine, int endColumn) {
    HotSpot *spot = new RegExpFilter::HotSpot(startLine, startColumn, endLine, endColumn);
    spot->setColor(color());
    return spot;
}

RegExpFilter::HotSpot *UrlFilter::newHotSpot(int startLine, int startColumn, int endLine, int endColumn) {
    HotSpot *spot = new UrlFilter::HotSpot(startLine, startColumn, endLine, endColumn);
    connect(spot->getUrlObject(), &FilterObject::activated, this, &UrlFilter::activated);
    return spot;
}

UrlFilter::HotSpot::HotSpot(int startLine, int startColumn, int endLine, int endColumn)
    : RegExpFilter::HotSpot(startLine, startColumn, endLine, endColumn),
      _urlObject(new FilterObject(this)) {
    setType(Link);
}

UrlFilter::HotSpot::UrlType UrlFilter::HotSpot::urlType() const {
    QString url = capturedTexts().constFirst();

    if (FullUrlRegExp.match(url).hasMatch())
        return StandardUrl;
    else if (EmailAddressRegExp.match(url).hasMatch())
        return Email;
    else if (FilePathRegExp.match(url).hasMatch())
        return FilePath;
    else
        return Unknown;
}

bool UrlFilter::HotSpot::hasClickAction(void) {
    const UrlType kind = urlType();
    if (kind == StandardUrl) {
        return true;
    } else if (kind == FilePath) {
        return true;
    }
    return false;
}

QString UrlFilter::HotSpot::clickActionToolTip(void) {
    const UrlType kind = urlType();
    if (kind == StandardUrl) {
        return tr("Follow link (ctrl + click)");
    } else if (kind == FilePath) {
        return tr("Follow path (ctrl + click)");
    }
    return QString();
}

void UrlFilter::HotSpot::clickAction(void) {
    QString url = capturedTexts().constFirst();
    const UrlType kind = urlType();

    if (kind == StandardUrl) {
        if (!url.contains(QLatin1String("://"))) {
            url.prepend(QLatin1String("http://"));
        }
    } else if (kind == FilePath) {
        url.replace(QLatin1Char('\\'), QLatin1Char('/'));
        url.replace(QLatin1Char('~'), QDir::homePath());
        if (url.startsWith(QLatin1String("/"))) {
            url.prepend(QLatin1String("file://"));
        } else if (url.startsWith(QLatin1String("."))) {
            url.prepend(QLatin1String("relative:"));
        } else if (url.startsWith(QLatin1String(".."))) {
            url.prepend(QLatin1String("relative:"));
        } else {
            url.prepend(QLatin1String("file:///"));
        }
    } else {
        return;
    }

    _urlObject->emitActivated(QUrl(url, QUrl::StrictMode), QTermWidget::OpenFromClick);
}

const QRegularExpression UrlFilter::FullUrlRegExp(QLatin1String("(www\\.(?!\\.)|[a-z][a-z0-9+.-]*://)[^\\s<>'\"]+[^!,\\.\\s<>'\"\\]]"));
const QRegularExpression UrlFilter::EmailAddressRegExp(QLatin1String("\\b(\\w|\\.|-)+@(\\w|\\.|-)+\\.\\w+\\b"));
const QRegularExpression UrlFilter::WindowsFilePathRegExp( QLatin1String("([a-zA-Z]:\\\\|\\\\\\\\|\\.\\\\|\\.\\.\\\\)[^\\s<>'\"]+[^!,\\.\\s<>'\"\\]]"));
const QRegularExpression UrlFilter::UnixFilePathRegExp(QLatin1String("((\\./|~/|\\.\\./)[^\\s<>'\"]+|/[^\\s<>'\"]+)[^!,\\.\\s<>'\"\\]]"));
const QRegularExpression UrlFilter::FilePathRegExp( QLatin1String("(") + WindowsFilePathRegExp.pattern() + QLatin1Char('|') + UnixFilePathRegExp.pattern() + QLatin1Char(')'));
const QRegularExpression UrlFilter::CompleteUrlRegExp(
    QLatin1Char('(') + FullUrlRegExp.pattern() + QLatin1Char('|') +
    EmailAddressRegExp.pattern() + QLatin1Char('|') + FilePathRegExp.pattern() +
    QLatin1Char(')'));

UrlFilter::UrlFilter() : RegExpFilter() { 
    setRegExp(CompleteUrlRegExp); 
}

UrlFilter::HotSpot::~HotSpot() { 
    delete _urlObject; 
}

void FilterObject::emitActivated(const QUrl &url, uint32_t opcode) {
    Q_EMIT activated(url, opcode);
}

FilterObject *UrlFilter::HotSpot::getUrlObject() const { return _urlObject; }

QList<QAction *> UrlFilter::HotSpot::actions() {
    QList<QAction *> list;

    const UrlType kind = urlType();

    Q_ASSERT(kind == StandardUrl || kind == Email || kind == FilePath);

    if (kind == StandardUrl) {
        QAction *openLinkAction = new QAction(_urlObject);
        QAction *copyLinkAction = new QAction(_urlObject);
        openLinkAction->setText(QObject::tr("Open Link"));
        copyLinkAction->setText(QObject::tr("Copy Link Address"));
        QObject::connect(openLinkAction, &QAction::triggered, _urlObject, [&](void) {
            QString url = capturedTexts().constFirst();
            if (!url.contains(QLatin1String("://"))) {
                url.prepend(QLatin1String("http://"));
            }
            _urlObject->emitActivated(QUrl(url, QUrl::StrictMode),
                                        QTermWidget::OpenFromContextMenu);
        });
        QObject::connect(copyLinkAction, &QAction::triggered, _urlObject, [&](void) {
            QString url = capturedTexts().constFirst();
            QApplication::clipboard()->setText(url);
        });
        list << openLinkAction;
        list << copyLinkAction;
    } else if (kind == Email) {
        QAction *sendEmailAction = new QAction(_urlObject);
        QAction *copyEmailAction = new QAction(_urlObject);
        sendEmailAction->setText(QObject::tr("Send Email To..."));
        copyEmailAction->setText(QObject::tr("Copy Email Address"));
        QObject::connect(sendEmailAction, &QAction::triggered, _urlObject, [&](void) {
            QString url = capturedTexts().constFirst();
            url.prepend(QLatin1String("mailto:"));
            _urlObject->emitActivated(QUrl(url, QUrl::StrictMode),
                                        QTermWidget::OpenFromContextMenu);
        });
        QObject::connect(copyEmailAction, &QAction::triggered, _urlObject, [&](void) {
            QString url = capturedTexts().constFirst();
            QApplication::clipboard()->setText(url);
        });
        list << sendEmailAction;
        list << copyEmailAction;
    } else if (kind == FilePath) {
        QAction *openPathAction = new QAction(_urlObject);
        QAction *openContainingPathAction = new QAction(_urlObject);
        QAction *copyPathAction = new QAction(_urlObject);
        openPathAction->setText(QObject::tr("Open Path"));
        openContainingPathAction->setText(QObject::tr("Open Containing Folder"));
        copyPathAction->setText(QObject::tr("Copy Path"));
        QObject::connect(openPathAction, &QAction::triggered, _urlObject, [&](void) {
            QString url = capturedTexts().constFirst();
            url.replace(QLatin1Char('\\'), QLatin1Char('/'));
            url.replace(QLatin1Char('~'), QDir::homePath());
            if (url.startsWith(QLatin1String("/"))) {
                url.prepend(QLatin1String("file://"));
            } else if (url.startsWith(QLatin1String("."))) {
                url.prepend(QLatin1String("relative:"));
            } else if (url.startsWith(QLatin1String(".."))) {
                url.prepend(QLatin1String("relative:"));
            } else {
                url.prepend(QLatin1String("file:///"));
            }
            _urlObject->emitActivated(QUrl(url, QUrl::StrictMode),
                                        QTermWidget::OpenFromContextMenu);
        });
        QObject::connect(openContainingPathAction, &QAction::triggered, _urlObject, [&](void) {
            QString url = capturedTexts().constFirst();
            url.replace(QLatin1Char('\\'), QLatin1Char('/'));
            url.replace(QLatin1Char('~'), QDir::homePath());
            if (url.startsWith(QLatin1String("/"))) {
                url.prepend(QLatin1String("file://"));
            } else if (url.startsWith(QLatin1String("."))) {
                url.prepend(QLatin1String("relative:"));
            } else if (url.startsWith(QLatin1String(".."))) {
                url.prepend(QLatin1String("relative:"));
            } else {
                url.prepend(QLatin1String("file:///"));
            }
            _urlObject->emitActivated(QUrl(url, QUrl::StrictMode),
                                        QTermWidget::OpenContainingFromContextMenu);
        });
        QObject::connect(copyPathAction, &QAction::triggered, _urlObject, [&](void) {
            QString url = capturedTexts().constFirst();
            QApplication::clipboard()->setText(url);
        });
        list << openPathAction;
        list << openContainingPathAction;
        list << copyPathAction;
    }

    return list;
}
