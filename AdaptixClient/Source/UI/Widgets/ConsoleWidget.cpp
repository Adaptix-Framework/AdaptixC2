#include <UI/Widgets/ConsoleWidget.h>
#include <Client/Requestor.h>
#include <MainAdaptix.h>
#include <Client/Settings.h>

ConsoleWidget::ConsoleWidget( Agent* a, Commander* c)
{
    agent     = a;
    commander = c;

    this->createUI();
    this->UpgradeCompleter();

    connect(CommandCompleter, QOverload<const QString &>::of(&QCompleter::activated), this, &ConsoleWidget::onCompletionSelected, Qt::DirectConnection);
    connect(InputLineEdit, &QLineEdit::returnPressed, this, &ConsoleWidget::processInput, Qt::QueuedConnection );

    kphInputLineEdit = new KPH_ConsoleInput(InputLineEdit, OutputTextEdit, this);
}

ConsoleWidget::~ConsoleWidget() {}

void ConsoleWidget::createUI()
{
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

    OutputTextEdit = new TextEditConsole(this);
    OutputTextEdit->setReadOnly(true);
    OutputTextEdit->setLineWrapMode( QTextEdit::LineWrapMode::NoWrap );
    OutputTextEdit->setProperty( "TextEditStyle", "console" );
    OutputTextEdit->setFont( QFont( "Hack" ));

    MainGridLayout = new QGridLayout(this );
    MainGridLayout->setVerticalSpacing(4 );
    MainGridLayout->setContentsMargins(0, 0, 0, 4 );
    MainGridLayout->addWidget( CmdLabel, 3, 0, 1, 1 );
    MainGridLayout->addWidget( InputLineEdit, 3, 1, 1, 1 );
    MainGridLayout->addWidget( OutputTextEdit, 0, 0, 1, 2);
    MainGridLayout->addWidget( InfoLabel, 2, 0, 1, 2);

    completerModel = new QStringListModel();
    CommandCompleter = new QCompleter(completerModel, this);
    CommandCompleter->popup()->setObjectName("Completer");
    CommandCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    CommandCompleter->setCompletionMode(QCompleter::PopupCompletion);

    InputLineEdit->setCompleter(CommandCompleter);
}

void ConsoleWidget::InputFocus() const
{
    InputLineEdit->setFocus();
}


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
            OutputTextEdit->appendPlain("[!] ");

        QString printMessage = TrimmedEnds(message);// +"\n";
        if ( text.isEmpty() || type == CONSOLE_OUT_LOCAL_SUCCESS || type == CONSOLE_OUT_LOCAL_ERROR || type == CONSOLE_OUT_SUCCESS || type == CONSOLE_OUT_ERROR)
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

    kphInputLineEdit->AddToHistory(commandLine);

    auto cmdResult = commander->ProcessInput( agent->data, commandLine );
    if ( cmdResult.output ) {
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
    else {
        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentCommand(agent->data.Name, agent->data.Id, commandLine, cmdResult.message, *(agent->adaptixWidget->GetProfile()), &message, &ok);
        if( !result ) {
            MessageError("JWT error");
            return;
        }
        if (!ok) {
            this->ConsoleOutputPrompt(0, "", "", commandLine);
            this->ConsoleOutputMessage(0, "", CONSOLE_OUT_LOCAL_ERROR, message, "", true);
        }
    }
}

void ConsoleWidget::UpgradeCompleter() const
{
    if (commander) {
        completerModel->setStringList(commander->GetCommands());
    }
}

void ConsoleWidget::onCompletionSelected(const QString &selectedText)
{
    userSelectedCompletion = true;
}