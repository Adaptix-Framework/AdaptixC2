#include <Agent/Agent.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ConsoleWidget.h>
#include <UI/Dialogs/DialogUploader.h>
#include <Client/Requestor.h>
#include <Client/Settings.h>
#include <Client/AuthProfile.h>
#include <MainAdaptix.h>

ConsoleWidget::ConsoleWidget( AdaptixWidget* w, Agent* a, Commander* c)
{
    adaptixWidget = w;
    agent         = a;
    commander     = c;

    this->createUI();
    this->upgradeCompleter();

    connect(CommandCompleter, QOverload<const QString &>::of(&QCompleter::activated), this, &ConsoleWidget::onCompletionSelected, Qt::DirectConnection);
    connect(InputLineEdit,    &QLineEdit::returnPressed,                              this, &ConsoleWidget::processInput,         Qt::QueuedConnection );
    connect(searchLineEdit,   &QLineEdit::returnPressed,                              this, &ConsoleWidget::handleSearch);
    connect(nextButton,       &ClickableLabel::clicked,                               this, &ConsoleWidget::handleSearch);
    connect(prevButton,       &ClickableLabel::clicked,                               this, &ConsoleWidget::handleSearchBackward);
    connect(searchInput,      &KPH_SearchInput::escPressed,                           this, &ConsoleWidget::toggleSearchPanel );
    connect(hideButton,       &ClickableLabel::clicked,                               this, &ConsoleWidget::toggleSearchPanel);
    connect(OutputTextEdit,   &TextEditConsole::ctx_find,                             this, &ConsoleWidget::toggleSearchPanel);
    connect(OutputTextEdit,   &TextEditConsole::ctx_history,                          this, &ConsoleWidget::handleShowHistory);
    connect(commander,        &Commander::commandsUpdated,                            this, &ConsoleWidget::upgradeCompleter);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+F"), OutputTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &ConsoleWidget::toggleSearchPanel);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+L"), OutputTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, OutputTextEdit, &QTextEdit::clear);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+A"), OutputTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, OutputTextEdit, &QTextEdit::selectAll);

    shortcutSearch = new QShortcut(QKeySequence("Ctrl+H"), OutputTextEdit);
    shortcutSearch->setContext(Qt::WidgetShortcut);
    connect(shortcutSearch, &QShortcut::activated, this, &ConsoleWidget::handleShowHistory);

    kphInputLineEdit = new KPH_ConsoleInput(InputLineEdit, OutputTextEdit, this);
    InputLineEdit->installEventFilter(kphInputLineEdit);
}

ConsoleWidget::~ConsoleWidget() {}

void ConsoleWidget::createUI()
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

    searchInput = new KPH_SearchInput(searchLineEdit, this);

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

    QString prompt = QString("%1 >").arg(agent->data.Name);
    CmdLabel = new QLabel(this );
    CmdLabel->setProperty( "LabelStyle", "console" );
    CmdLabel->setText( prompt );

    InputLineEdit = new QLineEdit(this);
    InputLineEdit->setProperty( "LineEditStyle", "console" );
    InputLineEdit->setFont( QFont( "Hack" ));

    QString info = "";
    if ( agent->data.Domain == "" || agent->data.Computer == agent->data.Domain )
        info = QString("[%1] %2 @ %3").arg( agent->data.Id ).arg( agent->data.Username ).arg( agent->data.Computer );
    else
        info = QString("[%1] %2 @ %3.%4").arg( agent->data.Id ).arg( agent->data.Username ).arg( agent->data.Computer ).arg( agent->data.Domain );

    InfoLabel = new QLabel(this);
    InfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    InfoLabel->setProperty( "LabelStyle", "console" );
    InfoLabel->setText ( info );

    OutputTextEdit = new TextEditConsole(this, GlobalClient->settings->data.ConsoleBufferSize, GlobalClient->settings->data.ConsoleNoWrap, GlobalClient->settings->data.ConsoleAutoScroll);
    OutputTextEdit->setReadOnly(true);
    OutputTextEdit->setProperty( "TextEditStyle", "console" );
    OutputTextEdit->setFont( QFont( "Hack" ));

    MainGridLayout = new QGridLayout(this );
    MainGridLayout->setVerticalSpacing(4 );
    MainGridLayout->setContentsMargins(0, 1, 0, 4 );
    MainGridLayout->addWidget( searchWidget,   0, 0, 1, 2 );
    MainGridLayout->addWidget( OutputTextEdit, 1, 0, 1, 2 );
    MainGridLayout->addWidget( InfoLabel,      2, 0, 1, 2 );
    MainGridLayout->addWidget( CmdLabel,       3, 0, 1, 1 );
    MainGridLayout->addWidget( InputLineEdit,  3, 1, 1, 1 );

    completerModel = new QStringListModel();
    CommandCompleter = new QCompleter(completerModel, this);
    CommandCompleter->popup()->setObjectName("Completer");
    CommandCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    CommandCompleter->setCompletionMode(QCompleter::PopupCompletion);

    InputLineEdit->setCompleter(CommandCompleter);
}

void ConsoleWidget::findAndHighlightAll(const QString &pattern)
{
    allSelections.clear();

    QTextCursor cursor(OutputTextEdit->document());
    cursor.movePosition(QTextCursor::Start);

    QTextCharFormat baseFmt;
    baseFmt.setBackground(Qt::blue);
    baseFmt.setForeground(Qt::white);

    while (true) {
        auto found = OutputTextEdit->document()->find(pattern, cursor);
        if (found.isNull())
            break;

        QTextEdit::ExtraSelection sel;
        sel.cursor = found;
        sel.format = baseFmt;
        allSelections.append(sel);

        cursor = found;
    }

    OutputTextEdit->setExtraSelections(allSelections);
}

void ConsoleWidget::highlightCurrent() const
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

    OutputTextEdit->setExtraSelections(sels);

    OutputTextEdit->setTextCursor(sels[currentIndex].cursor);

    searchLabel->setText(QString("%1 of %2").arg(currentIndex + 1).arg(sels.size()));
}



void ConsoleWidget::upgradeCompleter() const
{
    if (commander)
        completerModel->setStringList(commander->GetCommands());
}

void ConsoleWidget::InputFocus() const { InputLineEdit->setFocus(); }

void ConsoleWidget::AddToHistory(const QString &command) { kphInputLineEdit->AddToHistory(command); }

void ConsoleWidget::SetInput(const QString &command) { InputLineEdit->setText(command); }

void ConsoleWidget::Clear() { OutputTextEdit->clear(); }

void ConsoleWidget::ConsoleOutputMessage(const qint64 timestamp, const QString &taskId, const int type, const QString &message, const QString &text, const bool completed ) const
{
    QString promptTime = "";
    if (GlobalClient->settings->data.ConsoleTime)
        promptTime = UnixTimestampGlobalToStringLocal(timestamp);

    if( !message.isEmpty() ) {

        if ( !promptTime.isEmpty() )
            OutputTextEdit->appendColor("[" + promptTime + "] ", QColor(COLOR_SaturGray));

        if (type == CONSOLE_OUT_INFO || type == CONSOLE_OUT_LOCAL_INFO)
            OutputTextEdit->appendColor("[*] ", QColor(COLOR_BabyBlue));
        else if (type == CONSOLE_OUT_SUCCESS || type == CONSOLE_OUT_LOCAL_SUCCESS)
            OutputTextEdit->appendColor("[+] ", QColor(COLOR_Yellow));
        else if (type == CONSOLE_OUT_ERROR || type == CONSOLE_OUT_LOCAL_ERROR)
            OutputTextEdit->appendColor("[-] ", QColor(COLOR_ChiliPepper));
        else
            OutputTextEdit->appendPlain(" ");

        QString printMessage = TrimmedEnds(message);
        if ( text.isEmpty() || type == CONSOLE_OUT_LOCAL_INFO || type == CONSOLE_OUT_LOCAL_SUCCESS || type == CONSOLE_OUT_LOCAL_ERROR || type == CONSOLE_OUT_SUCCESS || type == CONSOLE_OUT_ERROR)
            printMessage += "\n";
        OutputTextEdit->appendPlain(printMessage);
    }

    if ( !text.isEmpty() )
        OutputTextEdit->appendPlain( TrimmedEnds(text) + "\n");

    if (completed) {
        QString deleter = "\n+-------------------------------------------------------------------------------------+\n";
        if ( !taskId.isEmpty() )
            deleter = QString("\n+--- Task [%1] closed ----------------------------------------------------------+\n").arg(taskId);

        OutputTextEdit->appendColor(deleter, QColor(COLOR_Gray));
    }
}

void ConsoleWidget::ConsoleOutputPrompt(const qint64 timestamp, const QString &taskId, const QString &user, const QString &commandLine ) const
{
    QString promptTime = "";
    if (GlobalClient->settings->data.ConsoleTime)
        promptTime = UnixTimestampGlobalToStringLocal(timestamp);

    if ( !commandLine.isEmpty() ) {
        OutputTextEdit->appendPlain("\n");

        if ( !promptTime.isEmpty() )
            OutputTextEdit->appendColor("[" + promptTime + "] ", QColor(COLOR_SaturGray));

        if ( !user.isEmpty() )
            OutputTextEdit->appendColor(user + " ", QColor(COLOR_Gray));

        if( !taskId.isEmpty() )
            OutputTextEdit->appendColor("[" + taskId + "] ", QColor(COLOR_SaturGray));

        OutputTextEdit->appendColorUnderline(agent->data.Name, QColor(COLOR_Gray));
        OutputTextEdit->appendColor(" > ", QColor(COLOR_Gray));

        OutputTextEdit->appendBold(commandLine + "\n");
    }
}

void ConsoleWidget::ProcessCmdResult(const QString &commandLine, const CommanderResult &cmdResult, const bool UI)
{
    if ( cmdResult.output ) {
        if (UI) {
            if (cmdResult.error)
                MessageError(cmdResult.message);
        }
        else {
            QString message = "";
            QString text    = "";
            int     type    = 0;

            if (cmdResult.error) {
                type    = CONSOLE_OUT_LOCAL_ERROR;
                message = cmdResult.message;
            }
            else {
                type = CONSOLE_OUT_LOCAL;
                text = cmdResult.message;
            }

            this->ConsoleOutputPrompt(0, "", "", commandLine);
            this->ConsoleOutputMessage(0, "", type, message, text, true);
        }
        return;
    }

    QString hookId = "";
    if (cmdResult.post_hook.isSet) {
        hookId = GenerateRandomString(8, "hex");
        while (adaptixWidget->PostHooksJS.contains(hookId))
            hookId = GenerateRandomString(8, "hex");

        adaptixWidget->PostHooksJS[hookId] = cmdResult.post_hook;
    }

    QJsonDocument jsonDoc(cmdResult.data);
    QString commandData = jsonDoc.toJson();

    QJsonObject dataJson;
    dataJson["name"]       = agent->data.Name;
    dataJson["id"]         = agent->data.Id;
    dataJson["ui"]         = UI;
    dataJson["cmdline"]    = commandLine;
    dataJson["data"]       = commandData;
    dataJson["ax_hook_id"] = hookId;
    QByteArray jsonData = QJsonDocument(dataJson).toJson();

    /// 5 Mb
    if (commandData.size() < 0x500000) {
        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentCommand(jsonData, *(agent->adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
                adaptixWidget->PostHooksJS.remove(hookId);
            MessageError("Response timeout");
            return;
        }
        if (!ok) {
            if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
                adaptixWidget->PostHooksJS.remove(hookId);
            this->ConsoleOutputPrompt(0, "", "", commandLine);
            this->ConsoleOutputMessage(0, "", CONSOLE_OUT_LOCAL_ERROR, message, "", true);
        }
    }
    else {

        /// 1. Get OTP

        QString message = QString();
        bool ok = false;
        QString objId = GenerateRandomString(8, "hex");
        bool result = HttpReqGetOTP("tmp_upload", objId, *(agent->adaptixWidget->GetProfile()), &message, &ok);
        if (!result) {
            if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
                adaptixWidget->PostHooksJS.remove(hookId);
            MessageError("Response timeout");
            return;
        }
        if (!ok) {
            if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
                adaptixWidget->PostHooksJS.remove(hookId);
            MessageError(message);
            return;
        }
        QString otp = message;

        /// 2. Upload with OTP

        QString sUrl = agent->adaptixWidget->GetProfile()->GetURL() + "/otp/upload/temp";

        auto* uploaderDialog = new DialogUploader(sUrl, otp, jsonData);
        uploaderDialog->setAttribute(Qt::WA_DeleteOnClose);

        connect(uploaderDialog, &DialogUploader::finished, [&](const bool success) {
            if (!success) {
                if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
                    adaptixWidget->PostHooksJS.remove(hookId);
                return;
            }

            /// 3. Send Command

            QJsonObject data2Json;
            data2Json["object_id"] = objId;
            QByteArray json2Data = QJsonDocument(data2Json).toJson();

            sUrl = agent->adaptixWidget->GetProfile()->GetURL() + "/agent/command/file";
            QJsonObject jsonObject = HttpReq(sUrl, json2Data, agent->adaptixWidget->GetProfile()->GetAccessToken(), 10000);
            if ( jsonObject.contains("message") && jsonObject.contains("ok") ) {
                if (jsonObject["ok"].toBool() == false) {
                    if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
                        adaptixWidget->PostHooksJS.remove(hookId);
                    MessageError( jsonObject["message"].toString());
                }
            }
            else {
                if (cmdResult.post_hook.isSet && adaptixWidget->PostHooksJS.contains(hookId))
                    adaptixWidget->PostHooksJS.remove(hookId);
                MessageError("Response timeout");
                return;
            }
        });

        uploaderDialog->exec();
    }
}

/// SLOTS

void ConsoleWidget::processInput()
{
    if (!commander)
        return;

    QString commandLine = TrimmedEnds(InputLineEdit->text());

    if ( this->userSelectedCompletion ) {
        this->userSelectedCompletion = false;
            return;
    }

    InputLineEdit->clear();
    if (commandLine.isEmpty())
        return;

    this->AddToHistory(commandLine);

    auto cmdResult = commander->ProcessInput( agent->data.Id, commandLine );
    if (cmdResult.is_pre_hook)
        return;

    this->ProcessCmdResult(commandLine, cmdResult, false);
}

void ConsoleWidget::toggleSearchPanel()
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

void ConsoleWidget::handleSearch()
{
    const QString pattern = searchLineEdit->text();
    if ( pattern.isEmpty() && allSelections.size() ) {
        allSelections.clear();
        currentIndex = -1;
        searchLabel->setText("0 of 0");
        OutputTextEdit->setExtraSelections({});
        return;
    }

    if (currentIndex < 0 || allSelections.isEmpty() || allSelections[0].cursor.selectedText().compare( pattern, Qt::CaseInsensitive) != 0 ) {
        findAndHighlightAll(pattern);
        currentIndex = 0;
    }
    else {
        currentIndex = (currentIndex + 1) % allSelections.size();
    }

    highlightCurrent();
}

void ConsoleWidget::handleSearchBackward()
{
    const QString pattern = searchLineEdit->text();
    if (pattern.isEmpty() && allSelections.size()) {
        allSelections.clear();
        currentIndex = -1;
        searchLabel->setText("0 of 0");
        OutputTextEdit->setExtraSelections({});
        return;
    }

    if (currentIndex < 0 || allSelections.isEmpty() || allSelections[0].cursor.selectedText().compare( pattern, Qt::CaseInsensitive) != 0 ) {
        findAndHighlightAll(pattern);
        currentIndex = allSelections.size() - 1;
    }
    else {
        currentIndex = (currentIndex - 1 + allSelections.size()) % allSelections.size();
    }

    highlightCurrent();
}

void ConsoleWidget::handleShowHistory()
{
    if (!kphInputLineEdit)
        return;

    QDialog *historyDialog = new QDialog(this);
    historyDialog->setWindowTitle(tr("Command History"));
    historyDialog->setAttribute(Qt::WA_DeleteOnClose);


    QListWidget *historyList = new QListWidget(historyDialog);
    historyList->setWordWrap(true);
    historyList->setTextElideMode(Qt::ElideNone);
    historyList->setAlternatingRowColors(true);
    historyList->setItemDelegate(new QStyledItemDelegate(historyList));

    QPushButton *closeButton = new QPushButton(tr("Close"), historyDialog);

    QVBoxLayout *layout = new QVBoxLayout(historyDialog);
    layout->addWidget(historyList);
    layout->addWidget(closeButton);

    const QStringList& history = kphInputLineEdit->getHistory();

    for (const QString &command : history) {
        QListWidgetItem *item = new QListWidgetItem(command);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setToolTip(command);
        int lines = (command.length() / 80) + 1;
        item->setSizeHint(QSize(item->sizeHint().width(), lines * 20));
        historyList->addItem(item);
    }

    if (history.isEmpty()) {
        QListWidgetItem *item = new QListWidgetItem(tr("No command history available"));
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        historyList->addItem(item);
    }

    connect(closeButton, &QPushButton::clicked, historyDialog, &QDialog::accept);

    connect(historyList, &QListWidget::itemDoubleClicked, this, [this, historyDialog](const QListWidgetItem *item) {
        InputLineEdit->setText(item->text());
        historyDialog->accept();
        InputLineEdit->setFocus();
    });

    historyDialog->resize(800, 500);
    historyDialog->move(QCursor::pos() - QPoint(historyDialog->width()/2, historyDialog->height()/2));

    historyDialog->setModal(true);
    historyDialog->show();
}

void ConsoleWidget::onCompletionSelected(const QString &selectedText) { userSelectedCompletion = true; }
