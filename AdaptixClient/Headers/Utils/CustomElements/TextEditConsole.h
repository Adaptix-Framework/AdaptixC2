#ifndef ADAPTIXCLIENT_TEXTEDITCONSOLE_H
#define ADAPTIXCLIENT_TEXTEDITCONSOLE_H

#include <main.h>
#include <QSyntaxHighlighter>
#include <QPlainTextEdit>

class QTimer;
class QMutex;

struct FormatRange {
    int start;
    int length;
    QTextCharFormat format;
};

class ConsoleBlockData : public QTextBlockUserData {
public:
    QVector<FormatRange> formats;
};

class ConsoleHighlighter : public QSyntaxHighlighter {
public:
    explicit ConsoleHighlighter(QTextDocument* parent);

protected:
    void highlightBlock(const QString& text) override;
};

class TextEditConsole : public QPlainTextEdit {
Q_OBJECT
    friend class ConsoleHighlighter;

    struct FormattedChunk {
        QString text;
        QTextCharFormat format;
    };

    ConsoleHighlighter* highlighter = nullptr;
    QTextCursor cachedCursor;
    QTextCursor prependCursor;
    bool prependMode = false;
    int  maxLines    = 50000;
    bool autoScroll  = false;
    bool noWrap      = true;
    bool syncMode    = false;
    bool showBgImage = true;
    bool suppressHighlight = false;
    int appendCount  = 0;
    int historyBlockCount = 0;

    QPixmap m_bgPixmap;
    QColor  m_bgColor      = QColor("#151515");
    int     m_bgDimming    = 70;
    bool    m_hasBgImage   = false;

    void updatePaletteBackground();

    QList<FormattedChunk> pendingChunks;
    QTimer* batchTimer = nullptr;
    QMutex   batchMutex;
    static constexpr int BATCH_INTERVAL_MS = 100;
    static constexpr int MAX_BATCH_SIZE = 64 * 1024;
    int pendingSize = 0;

    void trimExcessLines();
    void createContextMenu(const QPoint &pos);
    void setBufferSize(int size);
    void appendChunk(const QString& text, const QTextCharFormat& fmt);
    void insertChunks(const QList<FormattedChunk>& chunks);

private Q_SLOTS:
    void flushPending();

public:
    explicit TextEditConsole(QWidget* parent = nullptr, int maxLines = 50000, bool noWrap = true, bool autoScroll = false);

    void appendPlain(const QString& text);
    void appendFormatted(const QString& text, const std::function<void(QTextCharFormat&)> &styleFn);

    void appendColor(const QString& text, QColor color);
    void appendBold(const QString& text);
    void appendUnderline(const QString& text);
    void appendColorBold(const QString& text, QColor color);
    void appendColorUnderline(const QString& text, QColor color);

    void setMaxLines(int lines);
    void setAutoScrollEnabled(bool enabled);
    bool isAutoScrollEnabled() const;
    bool isNoWrapEnabled() const;

    void setSyncMode(bool enabled);
    void flushAll();
    void resetHistoryCount();

    void setBulkInsertMode(bool enabled);
    bool isBulkInsertMode() const { return suppressHighlight; }

    void beginPrepend();
    void endPrepend();
    bool isPrependMode() const;

    bool isShowBackgroundImage() const;
    void setShowBackgroundImage(bool enabled);

    void setConsoleBackground(const QColor& bgColor, const QString& imagePath = QString(), int dimming = 70);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    bool m_suppressPaletteGuard = false;
    void forceTransparentBase();

Q_SIGNALS:
    void ctx_find();
    void ctx_history();
    void ctx_bgToggled(bool showImage);
};

#endif // ADAPTIXCLIENT_TEXTEDITCONSOLE_H
