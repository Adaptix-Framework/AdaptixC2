#ifndef ADAPTIXCLIENT_LOGVIEW_H
#define ADAPTIXCLIENT_LOGVIEW_H

#include <Utils/CustomElements/LogViewTape.h>

#include <QTextBrowser>
#include <QMetaObject>
#include <QResizeEvent>
#include <QUrl>

/// Read-only chat
/// Roles: system | user | assistant | tool | error.
class LogView : public QTextBrowser
{
    Q_OBJECT
        LogViewTape m_tape;
    bool        m_applyingTheme = false;
    QMetaObject::Connection m_themeConn;
    bool        m_rebuilding = false;

    void connectThemeSignals();
    void applyTheme();
    void rebuild();
    void scrollToEndIfNeeded();
    int  bubbleWidthFor(const QString& text) const;
    void onAnchorClicked(const QUrl& url);
    static QString escaped(const QString& text);
    static QString toolTitle(const QString& text);
    static QString markdownLite(const QString& text);

protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

public:
    explicit LogView(QWidget* parent = nullptr);
    ~LogView() override = default;

    QString append(const QString& role, const QString& text);
    bool    appendDelta(const QString& blockId, const QString& text);
    bool    endBlock(const QString& blockId);
    void    clearTape();
    void    setAutoScroll(bool enabled);
    bool    autoScroll() const;

    const LogViewTape& tape() const { return m_tape; }
};

#endif
