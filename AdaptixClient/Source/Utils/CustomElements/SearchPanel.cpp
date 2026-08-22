#include <Utils/CustomElements/SearchPanel.h>
#include <Utils/FontManager.h>
#include <QKeyEvent>
#include <QToolButton>
#include <QRegularExpression>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>

SearchPanel::SearchPanel(QPlainTextEdit* target, QWidget* parent) : QWidget(parent), target(target)
{
    chrome = new QFrame(this);
    chrome->setObjectName(QStringLiteral("ConsoleSearchChrome"));

    searchLineEdit = new oclero::qlementine::LineEdit(chrome);
    searchLineEdit->setIcon(QIcon(":/icons/search"));
    searchLineEdit->setPlaceholderText(QStringLiteral("Find in view"));
    searchLineEdit->setMinimumWidth(160);
    searchLineEdit->setMaximumWidth(280);
    searchLineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    searchLineEdit->installEventFilter(this);

    searchLabel = new QLabel(QStringLiteral("0/0"), chrome);
    searchLabel->setObjectName(QStringLiteral("ConsoleSearchCount"));
    searchLabel->setMinimumWidth(40);
    searchLabel->setAlignment(Qt::AlignCenter);
    searchLabel->setToolTip(QStringLiteral("Match count in loaded view"));

    prevButton = new QToolButton(chrome);
    prevButton->setText(QStringLiteral("◀"));
    prevButton->setToolTip(QStringLiteral("Previous match (Shift+Enter)"));
    prevButton->setAutoRaise(true);
    prevButton->setCursor(Qt::PointingHandCursor);

    nextButton = new QToolButton(chrome);
    nextButton->setText(QStringLiteral("▶"));
    nextButton->setToolTip(QStringLiteral("Next match (Enter)"));
    nextButton->setAutoRaise(true);
    nextButton->setCursor(Qt::PointingHandCursor);

    historyButton = new QToolButton(chrome);
    historyButton->setIcon(QIcon(":/icons/search"));
    historyButton->setText(QStringLiteral("History"));
    historyButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    historyButton->setToolTip(QStringLiteral("Search full console history on the server"));
    historyButton->setAutoRaise(true);
    historyButton->setCursor(Qt::PointingHandCursor);
    historyButton->setVisible(false);

    hideButton = new QToolButton(chrome);
    hideButton->setText(QStringLiteral("✕"));
    hideButton->setToolTip(QStringLiteral("Close (Esc)"));
    hideButton->setAutoRaise(true);
    hideButton->setCursor(Qt::PointingHandCursor);

    chromeLayout = new QHBoxLayout(chrome);
    chromeLayout->setContentsMargins(8, 2, 6, 2);
    chromeLayout->setSpacing(4);
    chromeLayout->addWidget(searchLineEdit, 1);
    chromeLayout->addWidget(searchLabel, 0);
    chromeLayout->addWidget(prevButton, 0);
    chromeLayout->addWidget(nextButton, 0);
    chromeLayout->addWidget(historyButton, 0);
    chromeLayout->addWidget(hideButton, 0);

    layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(chrome, 0, Qt::AlignLeft | Qt::AlignVCenter);

    connect(searchLineEdit, &QLineEdit::returnPressed, this, &SearchPanel::searchNext);
    connect(searchLineEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (!isVisible() || !this->target)
            return;
        if (text.isEmpty()) {
            clearSelections();
            return;
        }
        findAndHighlightAll(text);
        currentIndex = selections.isEmpty() ? -1 : 0;
        highlightCurrent();
    });
    connect(nextButton,    &QToolButton::clicked, this, &SearchPanel::searchNext);
    connect(prevButton,    &QToolButton::clicked, this, &SearchPanel::searchPrevious);
    connect(hideButton,    &QToolButton::clicked, this, &SearchPanel::toggle);
    connect(historyButton, &QToolButton::clicked, this, [this]() { Q_EMIT historySearchRequested(); });

    connect(&FontManager::instance(), &FontManager::typographyChanged, this, [this]() {
        applyMetrics();
        applyChromeStyle();
    });

    applyMetrics();
    applyChromeStyle();
    setVisible(false);
}

void SearchPanel::applyMetrics()
{
    const AppTypography& ty = FontManager::instance().typography();
    const int innerH = ty.controlInnerH;
    const int barH   = ty.historyBarHeight;
    const int iconPx = qMax(10, qRound(12 * (ty.baseSize / 10.0)));

    searchLineEdit->setFixedHeight(innerH);
    searchLabel->setFixedHeight(innerH);
    prevButton->setFixedSize(innerH, innerH);
    nextButton->setFixedSize(innerH, innerH);
    hideButton->setFixedSize(innerH, innerH);
    historyButton->setFixedHeight(innerH);
    historyButton->setIconSize(QSize(iconPx, iconPx));

    chrome->setFixedHeight(barH);
    setFixedHeight(barH);
}

void SearchPanel::applyChromeStyle()
{
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();
    const AppTypography& ty = FontManager::instance().typography();
    const int fontPx = ty.chromeFontPx;
    const int innerH = ty.controlInnerH;
    const int minH   = qMax(16, innerH - 4);

    chrome->setStyleSheet(QStringLiteral(
        "QFrame#ConsoleSearchChrome {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
        "QLabel#ConsoleSearchCount {"
        "  color: %3;"
        "  font-size: %6px;"
        "  padding: 0 2px;"
        "}"
        "QToolButton {"
        "  border: none;"
        "  border-radius: 4px;"
        "  padding: 0 3px;"
        "  color: %4;"
        "  font-size: %6px;"
        "}"
        "QToolButton:hover {"
        "  background-color: %5;"
        "}"
        "oclero--qlementine--LineEdit, QLineEdit {"
        "  min-height: %7px;"
        "  max-height: %8px;"
        "  padding-top: 0px;"
        "  padding-bottom: 0px;"
        "}"
    ).arg(t.backgroundColorMain3.name(),
          t.borderColor.name(),
          t.secondaryColor.name(),
          t.primaryColor.name(),
          t.backgroundColorMain4.name())
     .arg(fontPx)
     .arg(minH)
     .arg(innerH));
}

void SearchPanel::setTarget(QPlainTextEdit* newTarget)
{
    if (newTarget == target)
        return;
    clearSelections();
    target = newTarget;
}

void SearchPanel::clearSelections()
{
    selections.clear();
    currentIndex = -1;
    QString label = QStringLiteral("0/0");
    if (!scopeHint.isEmpty())
        label += QStringLiteral(" ") + scopeHint;
    searchLabel->setText(label);
    if (target)
        target->setExtraSelections({});
}

void SearchPanel::setScopeHint(const QString& hint)
{
    scopeHint = hint;
}

QString SearchPanel::currentQuery() const
{
    return searchLineEdit ? searchLineEdit->text() : QString();
}

void SearchPanel::setHistorySearchEnabled(bool enabled)
{
    historySearchEnabled = enabled;
    if (historyButton)
        historyButton->setVisible(enabled);
}

void SearchPanel::highlightLocalQuery(const QString& pattern)
{
    if (pattern.isEmpty()) {
        clearSelections();
        return;
    }
    if (searchLineEdit && searchLineEdit->text() != pattern)
        searchLineEdit->setText(pattern);
    findAndHighlightAll(pattern);
    currentIndex = selections.isEmpty() ? -1 : 0;
    highlightCurrent();
}

void SearchPanel::toggle()
{
    if (isVisible()) {
        setVisible(false);
        searchLineEdit->blockSignals(true);
        searchLineEdit->setText(QString());
        searchLineEdit->blockSignals(false);
        clearSelections();
    } else {
        applyMetrics();
        applyChromeStyle();
        setVisible(true);
        searchLineEdit->setFocus();
        searchLineEdit->selectAll();
    }
}

void SearchPanel::searchNext()
{
    if (!target)
        return;
    const QString pattern = searchLineEdit->text();
    if (pattern.isEmpty()) {
        clearSelections();
        return;
    }

    if (currentIndex < 0 || selections.isEmpty()
        || selections[0].cursor.selectedText().compare(pattern, Qt::CaseInsensitive) != 0) {
        findAndHighlightAll(pattern);
        currentIndex = 0;
    } else {
        currentIndex = (currentIndex + 1) % selections.size();
    }

    highlightCurrent();
}

void SearchPanel::searchPrevious()
{
    if (!target)
        return;
    const QString pattern = searchLineEdit->text();
    if (pattern.isEmpty()) {
        clearSelections();
        return;
    }

    if (currentIndex < 0 || selections.isEmpty()
        || selections[0].cursor.selectedText().compare(pattern, Qt::CaseInsensitive) != 0) {
        findAndHighlightAll(pattern);
        currentIndex = selections.isEmpty() ? -1 : selections.size() - 1;
    } else {
        currentIndex = (currentIndex - 1 + selections.size()) % selections.size();
    }

    highlightCurrent();
}

void SearchPanel::findAndHighlightAll(const QString& pattern)
{
    selections.clear();
    if (!target || pattern.isEmpty())
        return;

    QTextCharFormat baseFmt;
    baseFmt.setBackground(QColor(255, 200, 0, 140));
    baseFmt.setForeground(Qt::black);

    const QString haystack = target->document()->toPlainText();
    const int patLen = pattern.size();
    int from = 0;
    while (from < haystack.size()) {
        const int idx = haystack.indexOf(pattern, from, Qt::CaseInsensitive);
        if (idx < 0)
            break;

        QTextCursor c(target->document());
        c.setPosition(idx);
        c.setPosition(idx + patLen, QTextCursor::KeepAnchor);

        QTextEdit::ExtraSelection sel;
        sel.cursor = c;
        sel.format = baseFmt;
        selections.append(sel);

        from = idx + qMax(1, patLen);
    }

    target->setExtraSelections(selections);
}

void SearchPanel::highlightCurrent()
{
    if (!target)
        return;
    if (selections.isEmpty()) {
        QString label = QStringLiteral("0/0");
        if (!scopeHint.isEmpty())
            label += QStringLiteral(" ") + scopeHint;
        searchLabel->setText(label);
        return;
    }

    auto sels = selections;

    QTextCharFormat activeFmt;
    activeFmt.setBackground(QColor(255, 165, 0));
    activeFmt.setForeground(Qt::black);

    sels[currentIndex].format = activeFmt;
    target->setExtraSelections(sels);
    target->setTextCursor(sels[currentIndex].cursor);

    QString label = QStringLiteral("%1/%2").arg(currentIndex + 1).arg(sels.size());
    if (!scopeHint.isEmpty())
        label += QStringLiteral(" ") + scopeHint;
    searchLabel->setText(label);
}

bool SearchPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == searchLineEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            toggle();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier)
                searchPrevious();
            else
                searchNext();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
