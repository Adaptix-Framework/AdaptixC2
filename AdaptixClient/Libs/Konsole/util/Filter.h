#ifndef FILTER_H
#define FILTER_H

#include <QAction>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QHash>
#include <QRegularExpression>
#include <QColor>

#include "Character.h"

class Filter : public QObject
{
    Q_OBJECT
public:
    class HotSpot {
    public:
        HotSpot(int startLine , int startColumn , int endLine , int endColumn);
        virtual ~HotSpot();

        enum Type {
            NotSpecified,
            Link,
            Marker
        };

        int startLine() const;
        int endLine() const;
        int startColumn() const;
        int endColumn() const;

        Type type() const;
        QColor color() const;
        void setColor(const QColor& color);

        virtual void clickAction(void) = 0;
        virtual QString clickActionToolTip(void) = 0;
        virtual bool hasClickAction(void) = 0;

        virtual QList<QAction*> actions();

    protected:
        void setType(Type type);

    private:
        int    _startLine;
        int    _startColumn;
        int    _endLine;
        int    _endColumn;
        Type   _type;
        QColor _color;
    };

    Filter();
    ~Filter() override;

    virtual void process() = 0;

    void reset();

    HotSpot* hotSpotAt(int line , int column) const;

    QList<HotSpot*> hotSpots() const;

    QList<HotSpot*> hotSpotsAtLine(int line) const;

    void setBuffer(const QString* buffer , const QList<int>* linePositions);

protected:
    void addHotSpot(HotSpot*);
    const QString* buffer();
    void getLineColumn(int position , int& startLine , int& startColumn);

private:
    QMultiHash<int,HotSpot*> _hotspots;
    QList<HotSpot*> _hotspotList;

    const QList<int>* _linePositions = nullptr;
    const QString* _buffer = nullptr;
};

class RegExpFilter : public Filter
{
    Q_OBJECT
public:

    class HotSpot : public Filter::HotSpot {
    public:
        HotSpot(int startLine, int startColumn, int endLine , int endColumn);
        void clickAction(void) override;
        QString clickActionToolTip(void) override;
        bool hasClickAction(void) override;

        void setCapturedTexts(const QStringList& texts);
        QStringList capturedTexts() const;
    private:
        QStringList _capturedTexts;
    };

    RegExpFilter();

    void setRegExp(const QRegularExpression& text);
    QRegularExpression regExp() const;

    void setColor(const QColor& color) { _color = color;}
    QColor color() const { return _color;}

    void process() override;

protected:

    virtual RegExpFilter::HotSpot* newHotSpot(int startLine,int startColumn, int endLine,int endColumn);

private:
    QRegularExpression _searchText;
    QColor _color;
};

class FilterObject;

class UrlFilter : public RegExpFilter
{
    Q_OBJECT
public:

    class HotSpot : public RegExpFilter::HotSpot {
    public:
        HotSpot(int startLine,int startColumn,int endLine,int endColumn);
        ~HotSpot() override;

        FilterObject* getUrlObject() const;

        QList<QAction*> actions() override;

        void clickAction(void) override;
        QString clickActionToolTip(void) override;
        bool hasClickAction(void) override;

    private:
        enum UrlType {
            StandardUrl,
            Email,
            FilePath,
            Unknown
        };
        UrlType urlType() const;

        FilterObject* _urlObject;
        
        HotSpot( const HotSpot& ) = delete;
        HotSpot& operator= ( const HotSpot& ) = delete;
    };

    UrlFilter();

protected:
    RegExpFilter::HotSpot* newHotSpot(int,int,int,int) override;

private:
    static const QRegularExpression FullUrlRegExp;
    static const QRegularExpression EmailAddressRegExp;
    static const QRegularExpression WindowsFilePathRegExp;
    static const QRegularExpression UnixFilePathRegExp;
    static const QRegularExpression FilePathRegExp;

    static const QRegularExpression CompleteUrlRegExp;

signals:
    void activated(const QUrl& url, uint32_t opcode);
};

class FilterObject : public QObject
{
    Q_OBJECT
public:
    FilterObject(Filter::HotSpot* filter) : _filter(filter) {}

    void emitActivated(const QUrl& url, uint32_t opcode);

private:
    Filter::HotSpot* _filter;
signals:
    void activated(const QUrl& url, uint32_t opcode);
};

class FilterChain : protected QList<Filter*>
{
public:
    virtual ~FilterChain();

    void addFilter(Filter* filter);
    void removeFilter(Filter* filter);
    bool containsFilter(Filter* filter);
    void clear();

    void reset();
    void process();

    void setBuffer(const QString* buffer , const QList<int>* linePositions);

    Filter::HotSpot* hotSpotAt(int line , int column) const;
    QList<Filter::HotSpot*> hotSpots() const;
    QList<Filter::HotSpot> hotSpotsAtLine(int line) const;
};

class TerminalImageFilterChain : public FilterChain
{
public:
    TerminalImageFilterChain();
    ~TerminalImageFilterChain() override;

    void setImage(const Character* const image , int lines , int columns, const QVector<LineProperty>& lineProperties);

private:
    QString* _buffer;
    QList<int>* _linePositions;
};

#endif //FILTER_H
