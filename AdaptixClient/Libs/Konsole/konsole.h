#ifndef _Q_TERM_WIDGET
#define _Q_TERM_WIDGET

#include <QTranslator>
#include <QLocale>
#include <QWidget>
#include <QClipboard>
#include <QTimer>
#include "Emulation.h"
#include "util/Filter.h"

class QVBoxLayout;
class SearchBar;
class Session;
class TerminalDisplay;
class Emulation;
class QUrl;

class QTermWidget : public QWidget {
    Q_OBJECT

public:
    enum ScrollBarPosition {
        NoScrollBar = 0,
        ScrollBarLeft = 1,
        ScrollBarRight = 2
    };
    enum UrlActivatedType {
        OpenFromContextMenu = 0,
        OpenContainingFromContextMenu = 1,
        OpenFromClick = 2
    };

    using KeyboardCursorShape = Emulation::KeyboardCursorShape;

    QTermWidget(QWidget *messageParentWidget = nullptr, QWidget *parent = nullptr);
    ~QTermWidget() override;

    QSize sizeHint() const override;

    void setTerminalSizeHint(bool enabled);
    bool terminalSizeHint();

    void setTerminalFont(const QFont & font);
    QFont getTerminalFont();
    void setTerminalOpacity(qreal level);
    void setTerminalBackgroundImage(const QString& backgroundImage);
    void setTerminalBackgroundMovie(const QString& backgroundMovie);
    void setTerminalBackgroundVideo(const QString& backgroundVideo);
    void setTerminalBackgroundMode(int mode);

    //Text codec, default is UTF-8
    void setTextCodec(QStringEncoder codec);

    void setColorScheme(const QString & name);

    QStringList getAvailableColorSchemes();
    static QStringList availableColorSchemes();

    void setBackgroundColor(const QColor &color);
    void setForegroundColor(const QColor &color);
    void setANSIColor(const int ansiColorId, const QColor &color);

    void setPreeditColorIndex(int index);

    void setHistorySize(int lines);

    int historySize() const;

    void setScrollBarPosition(ScrollBarPosition);

    void scrollToEnd();

    void sendText(const QString & text);

    void sendKeyEvent(QKeyEvent* e);

    void setFlowControlEnabled(bool enabled);

    bool flowControlEnabled(void);

    void setFlowControlWarningEnabled(bool enabled);

    static QStringList availableKeyBindings();

    QString keyBindings();

    void setMotionAfterPasting(int);

    int historyLinesCount();

    int screenColumnsCount();
    int screenLinesCount();

    void setSelectionStart(int row, int column);
    void setSelectionEnd(int row, int column);
    void getSelectionStart(int& row, int& column);
    void getSelectionEnd(int& row, int& column);

    QString selectedText(bool preserveLineBreaks = true);

    void setMonitorActivity(bool);
    void setMonitorSilence(bool);
    void setSilenceTimeout(int seconds);

    Filter::HotSpot* getHotSpotAt(const QPoint& pos) const;

    Filter::HotSpot* getHotSpotAt(int row, int column) const;

    QList<QAction*> filterActions(const QPoint& position);

    int recvData(const char *buff, int len) const;

    void setKeyboardCursorShape(KeyboardCursorShape shape);
    void setKeyboardCursorShape(uint32_t shape);

    void setBlinkingCursor(bool blink);

    /** Enables or disables bidi text in the terminal. */
    void setBidiEnabled(bool enabled);
    bool isBidiEnabled();

    /** change and wrap text corresponding to paste mode **/
    void bracketText(QString& text);

    /** forcefully disable bracketed paste mode **/
    void disableBracketedPasteMode(bool disable);
    bool bracketedPasteModeIsDisabled() const;

    /** Set the empty space outside the terminal */
    void setMargin(int);

    /** Get the empty space outside the terminal */
    int getMargin() const;

    void setDrawLineChars(bool drawLineChars);

    void setBoldIntense(bool boldIntense);

    void setConfirmMultilinePaste(bool confirmMultilinePaste);
    void setTrimPastedTrailingNewlines(bool trimPastedTrailingNewlines);
    void setEcho(bool echo);
    void setKeyboardCursorColor(bool useForegroundColor, const QColor& color);
    void proxySendData(QByteArray data) {
        emit sendData(data.data(), data.size());
    }

    void setLocked(bool enabled);

    void setSelectionOpacity(qreal opacity);

    void addHighLightText(const QString &text, const QColor &color);
    bool isContainHighLightText(const QString &text);
    void removeHighLightText(const QString &text);
    void clearHighLightTexts(void);
    QMap<QString, QColor> getHighLightTexts(void);

    void setWordCharacters(const QString &wordCharacters);
    QString wordCharacters(void);

    void autoHideMouseAfter(int delay);

    void setShowResizeNotificationEnabled(bool enabled);

    void setEnableHandleCtrlC(bool enable);

    int lines();
    int columns();
    int getCursorX();
    int getCursorY();
    void setCursorX(int x);
    void setCursorY(int y);

    QString screenGet(int row1, int col1, int row2, int col2, int mode);

    void setUrlFilterEnabled(bool enable);

    void setMessageParentWidget(QWidget *parent);
    void reTranslateUi(void);
    void set_fix_quardCRT_issue33(bool fix);

signals:
    void finished();
    void copyAvailable(bool);
    void termGetFocus();
    void termLostFocus();
    void termKeyPressed(QKeyEvent *);
    void urlActivated(const QUrl&, uint32_t opcode);
    void notifyBell();
    void activity();
    void silence();

    void stateChanged(int state);

    void flowControlEnabledChanged(bool enabled);

    void sendData(const char *,int);
    void dupDisplayOutput(const char* data,int len);
    void profileChanged(const QString & profile);
    void titleChanged(int title,const QString& newTitle);
    void changeTabTextColorRequest(int);
    void termSizeChange(int lines, int columns);
    void mousePressEventForwarded(QMouseEvent* event);
    void zmodemSendDetected();
    void zmodemRecvDetected();
    void handleCtrlC(void);

public slots:
    void copyClipboard();
    void copySelection();
    void pasteClipboard();
    void pasteSelection();
    void selectAll();
    int zoomIn();
    int zoomOut();
    void setSize(const QSize &);
    void setKeyBindings(const QString & kb);

    void clearScrollback();
    void clearScreen();
    void clear();
    void toggleShowSearchBar();
    void saveHistory(QIODevice *device, int format = 0, int start = -1, int end = -1);
    void saveHistory(QTextStream *stream, int format = 0, int start = -1, int end = -1);
    void screenShot(QPixmap *pixmap);
    void screenShot(const QString &fileName);
    void repaintDisplay(void);

protected:
    void resizeEvent(QResizeEvent *) override;

protected slots:
    void sessionFinished();
    void updateTerminalSize();
    void selectionChanged(bool textSelected);
    void monitorTimerDone();
    void activityStateSet(int);

private slots:
    void cursorChanged(Emulation::KeyboardCursorShape cursorShape, bool blinkingCursorEnabled);

private:
    class HighLightText {
    public:
        HighLightText(const QString& text, const QColor& color) : text(text), color(color) {
            regExpFilter = new RegExpFilter();
            regExpFilter->setRegExp(QRegularExpression(text));
            regExpFilter->setColor(color);
        }
        ~HighLightText() {
            delete regExpFilter;
        }
        QString text;
        QColor color;
        RegExpFilter *regExpFilter;
    };
    void search(bool forwards, bool next);
    int setZoom(int step);
    QWidget *messageParentWidget = nullptr;
    TerminalDisplay *m_terminalDisplay = nullptr;
    Emulation  *m_emulation = nullptr;
    SearchBar* m_searchBar = nullptr;
    QVBoxLayout *m_layout = nullptr;
    QList<HighLightText*> m_highLightTexts;
    bool m_echo = false;
    UrlFilter *m_urlFilter = nullptr;
    bool m_UrlFilterEnable = true;
    bool m_flowControl = true;
    bool m_hasDarkBackground = true;
    bool m_monitorActivity = false;
    bool m_monitorSilence = false;
    bool m_notifiedActivity = false;
    QTimer* m_monitorTimer = nullptr;
    int m_silenceSeconds = 10;

    const static int STEP_ZOOM = 3;
};

#endif
