#include <UI/Widgets/AdvisorWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Utils/Convert.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>

REGISTER_DOCK_WIDGET(AdvisorWidget, "AI Advisor", true)

AdvisorWidget::AdvisorWidget(AdaptixWidget* w) : DockTab("AI Advisor", w->GetProfile()->GetProject(), ":/icons/extension"), adaptixWidget(w)
{
    this->createUI();

    connect(searchLineEdit, &QLineEdit::returnPressed, this, &AdvisorWidget::handleSearch);
    connect(nextButton,     &ClickableLabel::clicked,  this, &AdvisorWidget::handleSearch);
    connect(prevButton,     &ClickableLabel::clicked,  this, &AdvisorWidget::handleSearchBackward);
    connect(hideButton,     &ClickableLabel::clicked,  this, &AdvisorWidget::toggleSearchPanel);
    connect(advisorOutput,  &TextEditConsole::ctx_find, this, &AdvisorWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), advisorOutput);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &AdvisorWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+L"), advisorOutput);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, advisorOutput, &QPlainTextEdit::clear);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+A"), advisorOutput);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, advisorOutput, &QPlainTextEdit::selectAll);

    this->dockWidget->setWidget(this);
}

AdvisorWidget::~AdvisorWidget() = default;

void AdvisorWidget::SetUpdatesEnabled(const bool enabled)
{
    advisorOutput->setUpdatesEnabled(enabled);
}

void AdvisorWidget::createUI()
{
    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    prevButton = new ClickableLabel("<");
    prevButton->setCursor(Qt::PointingHandCursor);

    nextButton = new ClickableLabel(">");
    nextButton->setCursor(Qt::PointingHandCursor);

    searchLabel    = new QLabel("0 of 0");
    searchLineEdit = new QLineEdit();
    searchLineEdit->setPlaceholderText("Find");
    searchLineEdit->setMaximumWidth(300);

    hideButton = new ClickableLabel("X");
    hideButton->setCursor(Qt::PointingHandCursor);

    spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 3, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(prevButton);
    searchLayout->addWidget(nextButton);
    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(searchLineEdit);
    searchLayout->addWidget(hideButton);
    searchLayout->addSpacerItem(spacer);

    advisorOutput = new TextEditConsole(this);
    advisorOutput->setReadOnly(true);
    advisorOutput->setStyleSheet("background-color: #151515; color: #BEBEBE; border: 1px solid #2A2A2A; border-radius: 4px;");

    gridLayout = new QGridLayout(this);
    gridLayout->setContentsMargins(0, 1, 0, 4);
    gridLayout->setVerticalSpacing(4);
    gridLayout->addWidget(searchWidget,  0, 0, 1, 1);
    gridLayout->addWidget(advisorOutput, 1, 0, 1, 1);
}

void AdvisorWidget::AddRecommendation(const qint64 date, const QString &agentId, const QString &command,
                                      const QString &rationale, const QString &lookFor,
                                      const QString &priority, const QString &category,
                                      const QStringList &alternatives, const QString &opsecNote)
{
    QColor priorityColor(COLOR_ConsoleWhite);
    if (priority == "high")
        priorityColor = QColor(COLOR_ChiliPepper);
    else if (priority == "medium")
        priorityColor = QColor(COLOR_BrightOrange);
    else if (priority == "low")
        priorityColor = QColor(COLOR_KellyGreen);

    advisorOutput->appendColor(UnixTimestampGlobalToStringLocal(date), QColor(COLOR_Gray));
    advisorOutput->appendPlain(" ");
    advisorOutput->appendColorBold("[" + category.toUpper() + "]", priorityColor);
    advisorOutput->appendPlain(" agent:");
    advisorOutput->appendColor(agentId.left(8), QColor(COLOR_NeonGreen));
    advisorOutput->appendPlain("\n");

    advisorOutput->appendPlain("  ");
    advisorOutput->appendColorBold(">>", QColor(COLOR_NeonGreen));
    advisorOutput->appendPlain(" ");
    advisorOutput->appendColorBold(command, QColor(COLOR_ConsoleWhite));
    advisorOutput->appendPlain("\n");

    advisorOutput->appendPlain("  ");
    advisorOutput->appendColor("Why: ", QColor(COLOR_Gray));
    advisorOutput->appendColor(rationale, QColor(COLOR_ConsoleWhite));
    advisorOutput->appendPlain("\n");

    if (!lookFor.isEmpty()) {
        advisorOutput->appendPlain("  ");
        advisorOutput->appendColor("Look for: ", QColor(COLOR_Gray));
        advisorOutput->appendColor(lookFor, QColor(COLOR_ConsoleWhite));
        advisorOutput->appendPlain("\n");
    }

    if (!opsecNote.isEmpty()) {
        advisorOutput->appendPlain("  ");
        advisorOutput->appendColor("OPSEC: ", QColor(COLOR_BrightOrange));
        advisorOutput->appendColor(opsecNote, QColor(COLOR_ConsoleWhite));
        advisorOutput->appendPlain("\n");
    }

    if (!alternatives.isEmpty()) {
        advisorOutput->appendPlain("  ");
        advisorOutput->appendColor("Alt: ", QColor(COLOR_Gray));
        advisorOutput->appendColor(alternatives.join(" | "), QColor(COLOR_PastelYellow));
        advisorOutput->appendPlain("\n");
    }

    advisorOutput->appendPlain("\n");
}

void AdvisorWidget::findAndHighlightAll(const QString &pattern)
{
    allSelections.clear();

    QTextCursor cursor(advisorOutput->document());
    cursor.movePosition(QTextCursor::Start);

    QTextCharFormat baseFmt;
    baseFmt.setBackground(Qt::blue);
    baseFmt.setForeground(Qt::white);

    while (true) {
        auto found = advisorOutput->document()->find(pattern, cursor);
        if (found.isNull())
            break;

        QTextEdit::ExtraSelection sel;
        sel.cursor = found;
        sel.format = baseFmt;
        allSelections.append(sel);

        cursor = found;
    }

    advisorOutput->setExtraSelections(allSelections);
}

void AdvisorWidget::highlightCurrent() const
{
    if (allSelections.isEmpty()) {
        searchLabel->setText("0 of 0");
        return;
    }

    auto sels = allSelections;

    QTextCharFormat activeFmt;
    activeFmt.setBackground(Qt::white);
    activeFmt.setForeground(Qt::black);

    sels[currentIndex].format = activeFmt;

    advisorOutput->setExtraSelections(sels);
    advisorOutput->setTextCursor(sels[currentIndex].cursor);

    searchLabel->setText(QString("%1 of %2").arg(currentIndex + 1).arg(sels.size()));
}

void AdvisorWidget::Clear() const { advisorOutput->clear(); }

void AdvisorWidget::toggleSearchPanel()
{
    if (this->searchWidget->isVisible()) {
        this->searchWidget->setVisible(false);
        searchLineEdit->setText("");
        handleSearch();
    }
    else {
        this->searchWidget->setVisible(true);
        searchLineEdit->setFocus();
        searchLineEdit->selectAll();
    }
}

void AdvisorWidget::handleSearch()
{
    const QString pattern = searchLineEdit->text();
    if (pattern.isEmpty() && allSelections.size()) {
        allSelections.clear();
        currentIndex = -1;
        searchLabel->setText("0 of 0");
        advisorOutput->setExtraSelections({});
        return;
    }

    if (currentIndex < 0 || allSelections.isEmpty() || allSelections[0].cursor.selectedText() != pattern) {
        findAndHighlightAll(pattern);
        currentIndex = 0;
    }
    else {
        currentIndex = (currentIndex + 1) % allSelections.size();
    }

    highlightCurrent();
}

void AdvisorWidget::handleSearchBackward()
{
    const QString pattern = searchLineEdit->text();
    if (pattern.isEmpty() && allSelections.size()) {
        allSelections.clear();
        currentIndex = -1;
        searchLabel->setText("0 of 0");
        advisorOutput->setExtraSelections({});
        return;
    }

    if (currentIndex < 0 || allSelections.isEmpty() || allSelections[0].cursor.selectedText() != pattern) {
        findAndHighlightAll(pattern);
        currentIndex = allSelections.size() - 1;
    }
    else {
        currentIndex = (currentIndex - 1 + allSelections.size()) % allSelections.size();
    }

    highlightCurrent();
}
