#include <Utils/CustomElements/LogView.h>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>

#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QScrollBar>
#include <QShowEvent>
#include <QColor>
#include <QUrl>
#include <QRegularExpression>

namespace {

const oclero::qlementine::Theme* logViewTheme(const QWidget* w)
{
    if (w) {
        if (auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(w->style()))
            return &qs->theme();
    }
    if (qApp) {
        if (auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style()))
            return &qs->theme();
    }
    return nullptr;
}

struct LogViewPalette {
    QString pageBg;
    QString body;
    QString muted;
    QString userBg;
    QString asstBg;
    QString toolBg;
    QString errBg;
    QString accent;
    QString errAccent;
};

LogViewPalette makePalette(const QWidget* w)
{
    LogViewPalette p;
    p.pageBg    = QStringLiteral("#1e2126");
    p.body      = QStringLiteral("#e8eaed");
    p.muted     = QStringLiteral("#9aa0a6");
    p.userBg    = QStringLiteral("#2b3340");
    p.asstBg    = QStringLiteral("#2a2d33");
    p.toolBg    = QStringLiteral("#26292e");
    p.errBg     = QStringLiteral("#3a2426");
    p.accent    = QStringLiteral("#5ec2b7");
    p.errAccent = QStringLiteral("#e06c75");

    const auto* theme = logViewTheme(w);
    if (!theme)
        return p;

    const bool dark = theme->backgroundColorMain1.lightnessF() < 0.5;
    const int shift = dark ? 128 : 88;
    p.pageBg    = theme->backgroundColorMain3.name();
    p.body      = theme->secondaryColor.name();
    p.muted     = theme->secondaryAlternativeColor.name();
    p.accent    = theme->primaryColor.name();
    p.errAccent = theme->statusColorError.name();
    p.asstBg    = theme->backgroundColorMain1.lighter(shift).name();
    p.userBg    = theme->backgroundColorMain1.lighter(shift + 18).name();
    p.toolBg    = theme->backgroundColorMain1.lighter(qMax(100, shift - 8)).name();

    QColor err = theme->statusColorError;
    if (dark)
        err = err.darker(220);
    else
        err = err.lighter(170);
    err.setAlpha(255);
    p.errBg = err.name();
    return p;
}

} // namespace

LogView::LogView(QWidget* parent) : QTextBrowser(parent)
{
    setObjectName(QStringLiteral("AxLogView"));
    setReadOnly(true);
    setOpenExternalLinks(false);
    setOpenLinks(false);
    setUndoRedoEnabled(false);
    setAcceptRichText(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    document()->setDocumentMargin(10);
    connect(this, &QTextBrowser::anchorClicked, this, &LogView::onAnchorClicked);
    applyTheme();
    connectThemeSignals();
}

void LogView::connectThemeSignals()
{
    if (m_themeConn)
        disconnect(m_themeConn);

    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(style());
    if (!qs)
        qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    if (qs)
        m_themeConn = connect(qs, &oclero::qlementine::QlementineStyle::themeChanged, this, &LogView::applyTheme, Qt::UniqueConnection);
}

void LogView::changeEvent(QEvent* event)
{
    QTextBrowser::changeEvent(event);
    if (!event || m_applyingTheme)
        return;
    if (event->type() == QEvent::StyleChange) {
        connectThemeSignals();
        applyTheme();
    }
}

void LogView::showEvent(QShowEvent* event)
{
    QTextBrowser::showEvent(event);
    if (m_applyingTheme)
        return;
    connectThemeSignals();
    applyTheme();
}

void LogView::resizeEvent(QResizeEvent* event)
{
    QTextBrowser::resizeEvent(event);
    if (m_applyingTheme || m_rebuilding || !event)
        return;
    if (event->oldSize().width() != event->size().width())
        rebuild();
}

void LogView::applyTheme()
{
    m_applyingTheme = true;
    const LogViewPalette pal = makePalette(this);
    setStyleSheet(QStringLiteral("QTextBrowser#AxLogView { background-color: %1; color: %2; border: none; padding: 6px; }").arg(pal.pageBg, pal.body));
    rebuild();
    m_applyingTheme = false;
}

int LogView::bubbleWidthFor(const QString& text) const
{
    const int viewW = qMax(viewport()->width(), 280);
    const int maxW = qBound(200, viewW * 70 / 100, 680);
    const int minW = qMin(220, maxW);

    QFont f = font();
    if (f.pointSize() < 12)
        f.setPointSize(13);
    const QFontMetrics fm(f);
    int longest = fm.horizontalAdvance(QStringLiteral("You"));
    const QStringList lines = text.split(QLatin1Char('\n'));
    if (lines.isEmpty())
        longest = qMax(longest, fm.horizontalAdvance(QStringLiteral("mmmmmmmm")));
    for (const QString& line : lines)
        longest = qMax(longest, fm.horizontalAdvance(line.left(120)));

    return qBound(minW, longest + 36, maxW);
}

QString LogView::escaped(const QString& text)
{
    QString out = text.toHtmlEscaped();
    out.replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
    return out;
}

namespace {

QString mdInline(QString s)
{
    static const QRegularExpression reCode(QStringLiteral("`([^`]+)`"));
    static const QRegularExpression reBold(QStringLiteral(R"(\*\*(.+?)\*\*)"));
    static const QRegularExpression reItalic(QStringLiteral(R"((?<!\*)\*(?!\*)(.+?)(?<!\*)\*(?!\*))"));
    s.replace(reCode, QStringLiteral("<span style=\"font-family:monospace; background-color:rgba(0,0,0,40); padding:0 3px;\">\\1</span>"));
    s.replace(reBold, QStringLiteral("<b>\\1</b>"));
    s.replace(reItalic, QStringLiteral("<i>\\1</i>"));
    return s;
}

bool mdIsTableSep(const QString& line)
{
    const QString t = line.trimmed();
    if (!t.contains(QLatin1Char('|')))
        return false;
    static const QRegularExpression re(QStringLiteral(R"(^\|?\s*:?-{2,}:?\s*(\|\s*:?-{2,}:?\s*)+\|?\s*$)"));
    return re.match(t).hasMatch();
}

bool mdLooksLikeTableRow(const QString& line)
{
    const QString t = line.trimmed();
    return t.contains(QLatin1Char('|')) && t.count(QLatin1Char('|')) >= 1 && (t.startsWith(QLatin1Char('|')) || t.contains(QStringLiteral(" | ")));
}

QStringList mdTableCells(const QString& line)
{
    QString t = line.trimmed();
    if (t.startsWith(QLatin1Char('|')))
        t = t.mid(1);
    if (t.endsWith(QLatin1Char('|')))
        t.chop(1);
    const QStringList raw = t.split(QLatin1Char('|'));
    QStringList cells;
    cells.reserve(raw.size());
    for (const QString& c : raw)
        cells.append(mdInline(c.trimmed().toHtmlEscaped()));
    return cells;
}

QString mdTableHtml(const QStringList& rows)
{
    if (rows.isEmpty())
        return {};
    QString html = QStringLiteral(
        "<table cellspacing=\"0\" cellpadding=\"5\" style=\"margin:6px 0;\">");
    bool header = true;
    for (const QString& row : rows) {
        if (mdIsTableSep(row))
            continue;
        const QStringList cells = mdTableCells(row);
        html += QStringLiteral("<tr>");
        for (const QString& c : cells) {
            if (header)
                html += QStringLiteral("<td style=\"font-weight:600; border-bottom:1px solid #666;\">%1</td>").arg(c);
            else
                html += QStringLiteral("<td style=\"padding-top:4px;\">%1</td>").arg(c);
        }
        html += QStringLiteral("</tr>");
        header = false;
    }
    html += QStringLiteral("</table>");
    return html;
}

} // namespace

QString LogView::markdownLite(const QString& text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    QString html;
    int i = 0;
    bool inCode = false;
    QString codeBuf;
    while (i < lines.size()) {
        const QString line = lines.at(i);
        const QString trim = line.trimmed();

        if (trim.startsWith(QStringLiteral("```"))) {
            if (inCode) {
                html += QStringLiteral(
                            "<pre style=\"font-family:monospace; font-size:12px; "
                            "background-color:rgba(0,0,0,50); padding:8px; margin:6px 0;\">%1</pre>")
                            .arg(codeBuf.toHtmlEscaped());
                codeBuf.clear();
                inCode = false;
            } else {
                inCode = true;
            }
            ++i;
            continue;
        }
        if (inCode) {
            if (!codeBuf.isEmpty())
                codeBuf += QLatin1Char('\n');
            codeBuf += line;
            ++i;
            continue;
        }

        if (mdLooksLikeTableRow(trim) && i + 1 < lines.size() && mdIsTableSep(lines.at(i + 1))) {
            QStringList tableRows;
            tableRows.append(line);
            ++i;
            while (i < lines.size() && (mdIsTableSep(lines.at(i)) || mdLooksLikeTableRow(lines.at(i).trimmed()))) {
                tableRows.append(lines.at(i));
                ++i;
            }
            html += mdTableHtml(tableRows);
            continue;
        }

        if (trim.startsWith(QStringLiteral("### "))) {
            html += QStringLiteral("<div style=\"font-weight:600; font-size:14px; margin:8px 0 4px 0;\">%1</div>").arg(mdInline(trim.mid(4).toHtmlEscaped()));
        } else if (trim.startsWith(QStringLiteral("## "))) {
            html += QStringLiteral("<div style=\"font-weight:600; font-size:15px; margin:10px 0 4px 0;\">%1</div>").arg(mdInline(trim.mid(3).toHtmlEscaped()));
        } else if (trim.startsWith(QStringLiteral("# "))) {
            html += QStringLiteral("<div style=\"font-weight:600; font-size:16px; margin:10px 0 4px 0;\">%1</div>").arg(mdInline(trim.mid(2).toHtmlEscaped()));
        } else if (trim.startsWith(QStringLiteral("- ")) || trim.startsWith(QStringLiteral("* "))) {
            html += QStringLiteral("<div style=\"margin:1px 0 1px 12px;\">• %1</div>").arg(mdInline(trim.mid(2).toHtmlEscaped()));
        } else {
            int digits = 0;
            while (digits < trim.size() && trim.at(digits).isDigit())
                ++digits;
            const bool numbered = digits > 0 && digits + 1 < trim.size() && trim.at(digits) == QLatin1Char('.') && trim.at(digits + 1).isSpace();
            if (numbered) {
                html += QStringLiteral("<div style=\"margin:1px 0 1px 12px;\">%1. %2</div>").arg(trim.left(digits), mdInline(trim.mid(digits + 1).trimmed().toHtmlEscaped()));
            } else if (trim.isEmpty()) {
                html += QStringLiteral("<div style=\"height:8px;\"></div>");
            } else {
                html += QStringLiteral("<div>%1</div>").arg(mdInline(line.toHtmlEscaped()));
            }
        }
        ++i;
    }
    if (inCode && !codeBuf.isEmpty()) {
        html += QStringLiteral(
                    "<pre style=\"font-family:monospace; font-size:12px; "
                    "background-color:rgba(0,0,0,50); padding:8px;\">%1</pre>")
                    .arg(codeBuf.toHtmlEscaped());
    }
    return html;
}

QString LogView::toolTitle(const QString& text)
{
    QString t = text.trimmed();
    const int colon = t.indexOf(QLatin1Char(':'));
    const int nl = t.indexOf(QLatin1Char('\n'));
    int cut = t.size();
    if (colon >= 0)
        cut = qMin(cut, colon);
    if (nl >= 0)
        cut = qMin(cut, nl);
    t = t.left(cut).trimmed();
    while (t.endsWith(QLatin1Char('.')) || t.endsWith(QChar(0x2026)))
        t.chop(1);
    t = t.trimmed();
    if (t.size() > 56)
        t = t.left(56) + QChar(0x2026);
    return t.isEmpty() ? QStringLiteral("Tool") : t;
}

void LogView::onAnchorClicked(const QUrl& url)
{
    if (url.scheme() != QLatin1String("toggle"))
        return;
    QString id = url.host();
    if (id.isEmpty()) {
        id = url.path();
        if (id.startsWith(QLatin1Char('/')))
            id = id.mid(1);
    }
    if (id.isEmpty())
        return;
    if (m_tape.toggleExpanded(id.toStdString()))
        rebuild();
}

void LogView::rebuild()
{
    if (m_rebuilding)
        return;
    m_rebuilding = true;

    const QString family = font().family();
    const LogViewPalette pal = makePalette(this);

    QString html = QStringLiteral(
                       "<html><body style=\"font-family:'%1'; font-size:13px; color:%2;\">")
                       .arg(family, pal.body);
    if (m_tape.blocks().empty()) {
        html += QStringLiteral(
                    "<div style=\"text-align:center; padding:72px 24px 24px 24px; color:%1;\">"
                    "<div style=\"font-size:15px; font-weight:600;\">Start a conversation</div>"
                    "<div style=\"font-size:12px; margin-top:8px;\">"
                    "Create a thread, then send a message to the operator.</div></div>")
                    .arg(pal.muted);
    }
    bool first = true;
    for (const auto& block : m_tape.blocks()) {
        const QString role = QString::fromStdString(block.role);
        const QString rawText = QString::fromStdString(block.text);
        if (role != QLatin1String("tool") && rawText.trimmed().isEmpty())
            continue;
        const QString body = escaped(rawText);
        const int width = bubbleWidthFor(rawText);

        if (role == QLatin1String("user") && !first) {
            html += QStringLiteral(
                        "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">"
                        "<tr><td style=\"padding:10px 18px 8px 18px;\">"
                        "<div style=\"border-top:1px solid %1; height:1px;\"></div>"
                        "</td></tr></table>")
                        .arg(pal.toolBg);
        }
        first = false;

        QString bg = pal.asstBg;
        QString fg = pal.body;
        QString align = QStringLiteral("left");
        QString label = QStringLiteral("Operator");
        QString labelColor = pal.muted;
        if (role == QLatin1String("user")) {
            bg = pal.userBg;
            align = QStringLiteral("right");
            label = QStringLiteral("You");
            labelColor = pal.accent;
        } else if (role == QLatin1String("assistant")) {
            label = QStringLiteral("Operator");
        } else if (role == QLatin1String("tool")) {
            const QString title = toolTitle(QString::fromStdString(block.text));
            const QString mark = block.expanded ? QStringLiteral("▾") : QStringLiteral("▸");
            const QString href = QStringLiteral("toggle://%1").arg(QString::fromStdString(block.id));
            QString inner;
            if (block.expanded)
                inner = escaped(QString::fromStdString(block.text));
            else
                inner = escaped(title);
            const int width = block.expanded ? bubbleWidthFor(QString::fromStdString(block.text)) : qMin(bubbleWidthFor(title), 360);
            html += QStringLiteral(
                        "<div style=\"padding:1px 16px 2px 28px; font-size:11px;\">"
                        "<a href=\"%1\" style=\"color:%2; text-decoration:none;\">%3 %4</a></div>")
                        .arg(href, pal.muted, mark, escaped(title));
            if (block.expanded) {
                html += QStringLiteral(
                            "<div style=\"margin:2px 16px 8px 28px; padding:8px 10px; "
                            "background-color:%1; font-size:12px; color:%2;\">%3</div>")
                            .arg(pal.toolBg, pal.muted, inner);
            }
            continue;
        } else if (role == QLatin1String("error")) {
            bg = pal.errBg;
            label = QStringLiteral("Error");
            labelColor = pal.errAccent;
        } else {
            label = QStringLiteral("System");
        }

        const QString rendered = (role == QLatin1String("assistant")) ? markdownLite(QString::fromStdString(block.text)) : body;
        html += QStringLiteral(
                    "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\">"
                    "<tr><td align=\"%1\" style=\"padding:6px 16px 10px 16px;\">"
                    "<div style=\"font-size:11px; font-weight:600; color:%2; padding:0 2px 4px 2px;\">%3</div>"
                    "<table width=\"%4\" cellspacing=\"0\" cellpadding=\"0\" bgcolor=\"%5\">"
                    "<tr><td width=\"4\" bgcolor=\"%2\"></td>"
                    "<td style=\"padding:10px 12px;\">"
                    "<div style=\"font-size:13px; color:%6;\">%7</div>"
                    "</td></tr></table>"
                    "</td></tr></table>")
                    .arg(align, labelColor, label, QString::number(width), bg, fg, rendered);
    }
    html += QStringLiteral("</body></html>");
    setHtml(html);
    scrollToEndIfNeeded();
    m_rebuilding = false;
}

void LogView::scrollToEndIfNeeded()
{
    if (!m_tape.autoScroll())
        return;
    if (auto* bar = verticalScrollBar())
        bar->setValue(bar->maximum());
}

QString LogView::append(const QString& role, const QString& text)
{
    const QString id = QString::fromStdString(m_tape.append(role.toStdString(), text.toStdString()));
    rebuild();
    return id;
}

bool LogView::appendDelta(const QString& blockId, const QString& text)
{
    if (!m_tape.appendDelta(blockId.toStdString(), text.toStdString()))
        return false;
    rebuild();
    return true;
}

bool LogView::endBlock(const QString& blockId)
{
    if (!m_tape.endBlock(blockId.toStdString()))
        return false;
    return true;
}

void LogView::clearTape()
{
    m_tape.clear();
    rebuild();
}

void LogView::setAutoScroll(bool enabled)
{
    m_tape.setAutoScroll(enabled);
    scrollToEndIfNeeded();
}

bool LogView::autoScroll() const
{
    return m_tape.autoScroll();
}
