#include <UI/Widgets/ChatWidget.h>
#include <Utils/Convert.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>

ChatWidget::ChatWidget(AdaptixWidget* w) : DockTab("Chat", w->GetProfile()->GetProject(), ":/icons/chat"), adaptixWidget(w)
{
    this->createUI();

    connect(chatInput,      &QLineEdit::returnPressed,  this, &ChatWidget::handleChat);
    connect(searchLineEdit, &QLineEdit::returnPressed,  this, &ChatWidget::handleSearch);
    connect(nextButton,     &ClickableLabel::clicked,   this, &ChatWidget::handleSearch);
    connect(prevButton,     &ClickableLabel::clicked,   this, &ChatWidget::handleSearchBackward);
    connect(hideButton,     &ClickableLabel::clicked,   this, &ChatWidget::toggleSearchPanel);
    connect(chatTextEdit,   &TextEditConsole::ctx_find, this, &ChatWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), chatTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &ChatWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+L"), chatTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, chatTextEdit, &QTextEdit::clear);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+A"), chatTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, chatTextEdit, &QTextEdit::selectAll);

    this->dockWidget->setWidget(this);
}

ChatWidget::~ChatWidget() = default;

void ChatWidget::SetUpdatesEnabled(const bool enabled)
{
    chatTextEdit->setUpdatesEnabled(enabled);
}

void ChatWidget::createUI()
{
    searchWidget = new QWidget(this);
    searchWidget->setVisible(false);

    prevButton = new ClickableLabel("<");
    prevButton->setCursor( Qt::PointingHandCursor );

    nextButton = new ClickableLabel(">");
    nextButton->setCursor( Qt::PointingHandCursor );

    searchLabel    = new QLabel("0 of 0");
    searchLineEdit = new QLineEdit();
    searchLineEdit->setPlaceholderText("Find");
    searchLineEdit->setMaximumWidth(300);

    hideButton = new ClickableLabel("X");
    hideButton->setCursor( Qt::PointingHandCursor );

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

    usernameLabel = new QLabel(this );
    usernameLabel->setProperty( "LabelStyle", "console" );
    usernameLabel->setText( adaptixWidget->GetProfile()->GetUsername() );

    chatInput = new QLineEdit(this);
    chatInput->setProperty( "LineEditStyle", "console" );

    chatTextEdit = new TextEditConsole(this);
    chatTextEdit->setReadOnly(true);
    chatTextEdit->setProperty("TextEditStyle", "console" );

    chatGridLayout = new QGridLayout(this);
    chatGridLayout->setContentsMargins(0, 1, 0, 4);
    chatGridLayout->setVerticalSpacing(4);
    chatGridLayout->addWidget( searchWidget,  0, 0, 1, 1);
    chatGridLayout->addWidget( chatTextEdit,  1, 0, 1, 2);
    chatGridLayout->addWidget( usernameLabel, 2, 0, 1, 1);
    chatGridLayout->addWidget( chatInput,     2, 1, 1, 1);
}

void ChatWidget::handleChat()
{
    QString message = QString();
    bool ok = false;
    bool result = HttpReqChatSendMessage(chatInput->text(), *(adaptixWidget->GetProfile()), &message, &ok);
    if (!result) {
        MessageError("Response timeout");
        return;
    }
    if (!ok) {
        MessageError(message);
        return;
    }
    chatInput->clear();
}

void ChatWidget::AddChatMessage(const qint64 time, const QString &username, const QString &message ) const
{
    chatTextEdit->appendColor(UnixTimestampGlobalToStringLocal(time), QColor(COLOR_Gray));
    chatTextEdit->appendPlain(" [");
    if (username == adaptixWidget->GetProfile()->GetUsername())
        chatTextEdit->appendColor(username, QColor(COLOR_NeonGreen));
    else
        chatTextEdit->appendColor(username, QColor(COLOR_KellyGreen));
    chatTextEdit->appendPlain("] :: ");
    chatTextEdit->appendColor(message, QColor(COLOR_ConsoleWhite));
    chatTextEdit->appendPlain("\n");
}


void ChatWidget::findAndHighlightAll(const QString &pattern)
{
    allSelections.clear();

    QTextCursor cursor(chatTextEdit->document());
    cursor.movePosition(QTextCursor::Start);

    QTextCharFormat baseFmt;
    baseFmt.setBackground(Qt::blue);
    baseFmt.setForeground(Qt::white);

    while (true) {
        auto found = chatTextEdit->document()->find(pattern, cursor);
        if (found.isNull())
            break;

        QTextEdit::ExtraSelection sel;
        sel.cursor = found;
        sel.format = baseFmt;
        allSelections.append(sel);

        cursor = found;
    }

    chatTextEdit->setExtraSelections(allSelections);
}

void ChatWidget::highlightCurrent() const
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

    chatTextEdit->setExtraSelections(sels);
    chatTextEdit->setTextCursor(sels[currentIndex].cursor);

    searchLabel->setText(QString("%1 of %2").arg(currentIndex + 1).arg(sels.size()));
}

void ChatWidget::Clear() const { chatTextEdit->clear(); }

void ChatWidget::toggleSearchPanel()
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

void ChatWidget::handleSearch()
{
    const QString pattern = searchLineEdit->text();
    if ( pattern.isEmpty() && allSelections.size() ) {
        allSelections.clear();
        currentIndex = -1;
        searchLabel->setText("0 of 0");
        chatTextEdit->setExtraSelections({});
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

void ChatWidget::handleSearchBackward()
{
    const QString pattern = searchLineEdit->text();
    if (pattern.isEmpty() && allSelections.size()) {
        allSelections.clear();
        currentIndex = -1;
        searchLabel->setText("0 of 0");
        chatTextEdit->setExtraSelections({});
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