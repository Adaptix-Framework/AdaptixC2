#include <UI/Dialogs/DialogConsoleSearch.h>
#include <Client/Requestor.h>
#include <Client/AuthProfile.h>
#include <Utils/CustomElements/TextEditConsole.h>
#include <Utils/FontManager.h>

#include <QPointer>
#include <QRegularExpression>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>

namespace {

QString extractMatchLine(const QJsonObject& hitObj, const QString& query)
{
    const QString q = query.trimmed();
    if (q.isEmpty())
        return QString();

    auto splitLines = [](QString s) -> QStringList {
        if (s.isEmpty())
            return {};
        s.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        s.replace(QChar('\r'), QChar('\n'));
        s.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        s.replace(QStringLiteral("\\r"), QStringLiteral("\n"));
        const QStringList raw = s.split(QChar('\n'), Qt::SkipEmptyParts);
        QStringList out;
        out.reserve(raw.size());
        for (QString line : raw) {
            line = line.trimmed();
            if (!line.isEmpty())
                out.append(line);
        }
        return out;
    };

    QStringList lines;

    auto takePacket = [&](const QJsonObject& pkt) {
        for (const char* key : {"a_cmdline", "a_message", "a_text", "a_client"}) {
            lines.append(splitLines(pkt.value(QLatin1String(key)).toString()));
        }
    };

    if (hitObj["packet"].isObject()) {
        takePacket(hitObj["packet"].toObject());
    } else if (hitObj["packet"].isString()) {
        const QJsonDocument doc = QJsonDocument::fromJson(hitObj["packet"].toString().toUtf8());
        if (doc.isObject())
            takePacket(doc.object());
        else
            lines.append(splitLines(hitObj["packet"].toString()));
    }

    if (lines.isEmpty()) {
        for (const char* key : {"a_cmdline", "a_message", "a_text", "a_client"})
            lines.append(splitLines(hitObj.value(QLatin1String(key)).toString()));
    }

    if (lines.isEmpty())
        lines.append(splitLines(hitObj["snippet"].toString()));

    for (const QString& line : lines) {
        if (line.contains(q, Qt::CaseInsensitive))
            return line;
    }

    const QString snip = hitObj["snippet"].toString().trimmed();
    if (!snip.isEmpty()) {
        const QStringList snipLines = splitLines(snip);
        if (!snipLines.isEmpty())
            return snipLines.first();
        return snip.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts).value(0, snip);
    }
    return QString();
}

} // namespace

DialogConsoleSearch::DialogConsoleSearch(qint64 agentId, AuthProfile* profile, QWidget* parent) : QDialog(parent), agentId(agentId), profile(profile)
{
    setWindowTitle(tr("Console history search"));
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumSize(760, 480);
    resize(920, 560);
    createUI();
}

DialogConsoleSearch::~DialogConsoleSearch() = default;

void DialogConsoleSearch::createUI()
{
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style());
    const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();

    queryEdit = new QLineEdit(this);
    queryEdit->setPlaceholderText(tr("Search ..."));
    queryEdit->setClearButtonEnabled(true);
    queryEdit->setFixedHeight(30);

    searchButton = new QPushButton(tr("Search"), this);
    searchButton->setDefault(true);
    searchButton->setFixedHeight(30);
    searchButton->setMinimumWidth(88);
    searchButton->setCursor(Qt::PointingHandCursor);

    statusLabel = new QLabel(tr("Enter a query to search server history"), this);
    statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: %2px;")
        .arg(t.secondaryColor.name())
        .arg(FontManager::instance().typography().chromeFontPx));

    resultsList = new QListWidget(this);
    resultsList->setAlternatingRowColors(true);
    resultsList->setUniformItemSizes(true);
    resultsList->setWordWrap(false);
    resultsList->setTextElideMode(Qt::ElideRight);
    resultsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    resultsList->setMinimumWidth(260);

    contextView = new TextEditConsole(this, 20000, true, false);
    contextView->setReadOnly(true);
    contextView->setFont(FontManager::instance().appMonoFont());
    contextView->setPlaceholderText(tr("Select a result to preview nearby history"));

    contextSpin = new QSpinBox(this);
    contextSpin->setRange(1, 500);
    contextSpin->setSingleStep(1);
    contextSpin->setValue(contextLimit);
    contextSpin->setToolTip(tr("Number of history items around the match"));
    contextSpin->setFixedWidth(64);

    auto* contextLabel = new QLabel(tr("Context:"), this);
    expandButton = new QPushButton(tr("Expand"), this);
    expandButton->setToolTip(tr("Load a larger window around the selected match"));
    expandButton->setCursor(Qt::PointingHandCursor);
    expandButton->setEnabled(false);

    openButton = new QPushButton(tr("Open in console"), this);
    openButton->setToolTip(tr("Load this history window into the agent console"));
    openButton->setCursor(Qt::PointingHandCursor);
    openButton->setEnabled(false);
    openButton->setDefault(false);

    closeButton = new QPushButton(tr("Close"), this);
    closeButton->setCursor(Qt::PointingHandCursor);

    auto* searchRow = new QHBoxLayout();
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(8);
    searchRow->addWidget(queryEdit, 1);
    searchRow->addWidget(searchButton, 0);

    auto* leftCol = new QVBoxLayout();
    leftCol->setContentsMargins(0, 0, 0, 0);
    leftCol->setSpacing(6);
    auto* resultsHeader = new QLabel(tr("Results"), this);
    resultsHeader->setStyleSheet(QStringLiteral("font-weight: 600; color: %1;").arg(t.primaryColor.name()));
    leftCol->addWidget(resultsHeader);
    leftCol->addWidget(resultsList, 1);

    auto* rightCol = new QVBoxLayout();
    rightCol->setContentsMargins(0, 0, 0, 0);
    rightCol->setSpacing(6);
    auto* previewHeader = new QLabel(tr("Context preview"), this);
    previewHeader->setStyleSheet(QStringLiteral("font-weight: 600; color: %1;").arg(t.primaryColor.name()));

    auto* ctxToolbar = new QHBoxLayout();
    ctxToolbar->setContentsMargins(0, 0, 0, 0);
    ctxToolbar->setSpacing(6);
    ctxToolbar->addWidget(previewHeader, 0);
    ctxToolbar->addStretch(1);
    ctxToolbar->addWidget(contextLabel, 0);
    ctxToolbar->addWidget(contextSpin, 0);
    ctxToolbar->addWidget(expandButton, 0);

    rightCol->addLayout(ctxToolbar);
    rightCol->addWidget(contextView, 1);

    auto* leftWrap = new QWidget(this);
    leftWrap->setLayout(leftCol);
    auto* rightWrap = new QWidget(this);
    rightWrap->setLayout(rightCol);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftWrap);
    splitter->addWidget(rightWrap);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    splitter->setChildrenCollapsible(false);

    auto* bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);
    bottomRow->setSpacing(8);
    bottomRow->addWidget(statusLabel, 1);
    bottomRow->addWidget(openButton, 0);
    bottomRow->addWidget(closeButton, 0);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 12);
    root->setSpacing(10);
    root->addLayout(searchRow);
    root->addWidget(splitter, 1);
    root->addLayout(bottomRow);

    connect(searchButton, &QPushButton::clicked, this, &DialogConsoleSearch::runSearch);
    connect(queryEdit, &QLineEdit::returnPressed, this, &DialogConsoleSearch::runSearch);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    connect(contextSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        contextLimit = v;
    });
    connect(expandButton, &QPushButton::clicked, this, [this]() {
        if (currentCenterId <= 0)
            return;
        contextLimit = qMin(500, contextLimit + 4);
        contextSpin->setValue(contextLimit);
        loadContext(currentCenterId);
    });
    connect(openButton, &QPushButton::clicked, this, [this]() {
        if (currentCenterId <= 0)
            return;
        Q_EMIT openInConsole(currentCenterId, contextLimit);
        close();
    });
    connect(resultsList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= hitIds.size()) {
            currentCenterId = 0;
            openButton->setEnabled(false);
            expandButton->setEnabled(false);
            return;
        }
        loadContext(hitIds[row]);
    });
}

void DialogConsoleSearch::setInitialQuery(const QString& query)
{
    queryEdit->setText(query);
    if (!query.trimmed().isEmpty())
        runSearch();
}

void DialogConsoleSearch::setBusy(bool busy)
{
    searching = busy;
    searchButton->setEnabled(!busy);
    queryEdit->setEnabled(!busy);
}

void DialogConsoleSearch::runSearch()
{
    if (!profile || agentId == 0 || searching)
        return;

    const QString q = queryEdit->text().trimmed();
    if (q.isEmpty()) {
        statusLabel->setText(tr("Enter a query to search server history"));
        return;
    }

    setBusy(true);
    statusLabel->setText(tr("Searching…"));
    resultsList->clear();
    hitIds.clear();
    hitTotal = 0;
    currentCenterId = 0;
    contextView->clear();
    openButton->setEnabled(false);
    expandButton->setEnabled(false);

    QPointer<DialogConsoleSearch> self = this;
    HttpReqConsoleSearchAsync(agentId, q, 100, 0, *profile,
        [self, q](bool success, const QString& message, const QJsonObject& response) {
            if (!self)
                return;
            self->setBusy(false);
            if (!success) {
                self->statusLabel->setText(tr("Search failed: %1").arg(message.isEmpty() ? tr("unknown error") : message));
                return;
            }

            const QJsonArray items = response["items"].toArray();
            self->hitTotal = response["total"].toInt();
            self->hitIds.clear();

            for (const QJsonValue& v : items) {
                if (!v.isObject())
                    continue;
                const QJsonObject obj = v.toObject();
                qint64 id = parseI64(obj, "id");
                if (id <= 0)
                    id = parseI64(obj, "Id");
                if (id <= 0)
                    continue;
                self->hitIds.append(id);

                QString line = extractMatchLine(obj, q);
                if (line.isEmpty()) {
                    line = obj["snippet"].toString().trimmed();
                    if (line.contains(QChar('\n')) || line.contains(QChar('\r'))) {
                        line = line.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts).value(0);
                    }
                }
                if (line.isEmpty())
                    line = tr("(empty match)");
                line.replace(QChar('\n'), QChar(' '));
                line.replace(QChar('\r'), QChar(' '));
                line = line.simplified();
                if (line.size() > 200)
                    line = line.left(197) + QStringLiteral("…");

                auto* item = new QListWidgetItem(line);
                item->setData(Qt::UserRole, QVariant::fromValue(id));
                item->setToolTip(line);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                self->resultsList->addItem(item);
            }

            const int shown = self->hitIds.size();
            if (shown == 0) {
                if (self->hitTotal > 0)
                    self->statusLabel->setText(tr("Server reported %1 matches, but none could be displayed").arg(self->hitTotal));
                else
                    self->statusLabel->setText(tr("No matches for “%1”").arg(q));
                return;
            }

            self->statusLabel->setText(tr("Showing %1 of %2 matches for “%3”").arg(shown).arg(self->hitTotal > 0 ? self->hitTotal : shown).arg(q));
            self->resultsList->setCurrentRow(0);
        });
}

void DialogConsoleSearch::loadContext(qint64 centerId)
{
    if (!profile || agentId == 0 || centerId <= 0 || loadingContext)
        return;

    loadingContext = true;
    currentCenterId = centerId;
    openButton->setEnabled(false);
    expandButton->setEnabled(false);
    statusLabel->setText(tr("Loading context…"));

    QPointer<DialogConsoleSearch> self = this;
    HttpReqConsoleGetAroundAsync(agentId, centerId, contextLimit, *profile,
        [self, centerId](bool success, const QString& message, const QJsonObject& response) {
            if (!self)
                return;
            self->loadingContext = false;
            if (!success) {
                self->statusLabel->setText(tr("Context load failed: %1")
                    .arg(message.isEmpty() ? tr("unknown error") : message));
                return;
            }
            if (self->currentCenterId != centerId)
                return;

            const QJsonArray items = response["items"].toArray();
            self->renderPackets(items);
            self->openButton->setEnabled(true);
            self->expandButton->setEnabled(true);

            const int total = response["total"].toInt();
            self->statusLabel->setText(tr("Context: %1 items around match · history total %2").arg(items.size()).arg(total));
        });
}

void DialogConsoleSearch::renderPackets(const QJsonArray& items)
{
    contextView->clear();
    contextView->resetHistoryCount();
    contextView->setSyncMode(true);
    contextView->setBulkInsertMode(true);

    const QString q = queryEdit->text().trimmed();

    for (const QJsonValue& v : items) {
        if (!v.isObject())
            continue;
        const QJsonObject obj = v.toObject();

        QStringList parts;
        const QString client = obj["a_client"].toString();
        const QString cmdline = obj["a_cmdline"].toString();
        const QString message = obj["a_message"].toString();
        const QString text = obj["a_text"].toString();
        const QString taskId = obj.contains("a_task_id")
            ? QString::number(parseI64(obj, "a_task_id"))
            : QString();

        if (!client.isEmpty() || !cmdline.isEmpty()) {
            QString head;
            if (!client.isEmpty())
                head += client + QStringLiteral(" ");
            if (!taskId.isEmpty() && taskId != QStringLiteral("0"))
                head += QStringLiteral("[%1] ").arg(taskId);
            if (!cmdline.isEmpty())
                head += QStringLiteral("> ") + cmdline;
            contextView->appendColor(head + QStringLiteral("\n"), QColor("#7FA3C0"));
        }
        if (!message.isEmpty())
            contextView->appendPlain(message + QStringLiteral("\n"));
        if (!text.isEmpty())
            contextView->appendPlain(text + QStringLiteral("\n"));
        if (client.isEmpty() && cmdline.isEmpty() && message.isEmpty() && text.isEmpty()) {
            contextView->appendPlain(QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)) + QStringLiteral("\n"));
        }
        contextView->appendPlain(QStringLiteral("\n"));
    }

    contextView->setBulkInsertMode(false);
    contextView->setSyncMode(false);

    if (!q.isEmpty()) {
        QTextDocument* doc = contextView->document();
        QList<QTextEdit::ExtraSelection> sels;
        QTextCharFormat fmt;
        fmt.setBackground(QColor(255, 200, 0, 140));
        fmt.setForeground(Qt::black);

        const QString haystack = doc->toPlainText();
        const int patLen = q.size();
        int from = 0;
        while (from < haystack.size()) {
            const int idx = haystack.indexOf(q, from, Qt::CaseInsensitive);
            if (idx < 0)
                break;
            QTextCursor c(doc);
            c.setPosition(idx);
            c.setPosition(idx + patLen, QTextCursor::KeepAnchor);
            QTextEdit::ExtraSelection sel;
            sel.cursor = c;
            sel.format = fmt;
            sels.append(sel);
            from = idx + qMax(1, patLen);
        }
        contextView->setExtraSelections(sels);
        if (!sels.isEmpty())
            contextView->setTextCursor(sels.first().cursor);
    }
}
