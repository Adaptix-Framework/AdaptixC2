#include <UI/Widgets/ConsoleWidget.h>
#include <Client/Requestor.h>

ConsoleWidget::ConsoleWidget( Agent* a, Commander* c)
{
    agent     = a;
    commander = c;
    this->createUI();
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

    QString info = QString("[%1] %2 @ %3.%4").arg( agent->data.Id ).arg( agent->data.Username ).arg( agent->data.Computer ).arg( agent->data.Domain );
    InfoLabel = new QLabel(this);
    InfoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    InfoLabel->setProperty( "LabelStyle", "console" );
    InfoLabel->setText ( info );

    OutputTextEdit = new QTextEdit( this );
    OutputTextEdit->setReadOnly(true);
    OutputTextEdit->setLineWrapMode( QTextEdit::LineWrapMode::NoWrap );
    OutputTextEdit->setProperty( "TextEditStyle", "console" );

    MainGridLayout = new QGridLayout(this );
    MainGridLayout->setVerticalSpacing(4 );
    MainGridLayout->setContentsMargins(0, 0, 0, 4 );
    MainGridLayout->addWidget( CmdLabel, 3, 0, 1, 1 );
    MainGridLayout->addWidget( InputLineEdit, 3, 1, 1, 1 );
    MainGridLayout->addWidget( OutputTextEdit, 0, 0, 1, 2);
    MainGridLayout->addWidget( InfoLabel, 2, 0, 1, 2);

    if (commander) {
        QStringList commandList = commander->GetCommands();
        CommandCompleter = new QCompleter(commandList, this);
        CommandCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        InputLineEdit->setCompleter(CommandCompleter);
    }

    connect( InputLineEdit, &QLineEdit::returnPressed, this, &ConsoleWidget::processInput );
}

void ConsoleWidget::ConsoleOutputMessage( qint64 timestamp, QString taskId, int type, QString message, QString text, bool completed )
{
    QString deleter = "<br>" + TextColorHtml( "+-------------------------------------------------------------------------------------+", COLOR_Gray) + "<br>";

    QString promptTime = UnixTimestampGlobalToStringLocal(timestamp);
    if ( !promptTime.isEmpty() )
        promptTime = TextColorHtml("[" + promptTime + "]", COLOR_SaturGray) + " ";

    if( !taskId.isEmpty() ) {
        deleter = QString("+--- Task [%1] closed ----------------------------------------------------------+").arg(taskId );
        deleter = "<br>" + TextColorHtml( deleter, COLOR_Gray) + "<br>";
    }

    if( !message.isEmpty() ) {
        QString mark = "[!]";
        if (type == CONSOLE_OUT_INFO || type == CONSOLE_OUT_LOCAL_INFO) {
            mark = TextBoltColorHtml("[*]", COLOR_BabyBlue);
        }
        else if (type == CONSOLE_OUT_SUCCESS || type == CONSOLE_OUT_LOCAL_SUCCESS) {
            mark = TextBoltColorHtml("[+]", COLOR_Yellow);
        }
        else if (type == CONSOLE_OUT_ERROR || type == CONSOLE_OUT_LOCAL_ERROR) {
            mark = TextBoltColorHtml("[-]", COLOR_ChiliPepper);
        }

        QString printMessage = promptTime + mark + " " + TrimmedEnds(message).toHtmlEscaped(); // + promptTask;

        if ( type == CONSOLE_OUT_LOCAL_SUCCESS || type == CONSOLE_OUT_LOCAL_ERROR || type == CONSOLE_OUT_LOCAL_INFO ){
            printMessage += "<br>";
        }

        OutputTextEdit->append( printMessage );
    }

    if ( !text.isEmpty() )
        OutputTextEdit->append( TrimmedEnds(text) );

    if (completed)
        OutputTextEdit->append( deleter );
}

void ConsoleWidget::ConsoleOutputPrompt( qint64 timestamp, QString taskId, QString user, QString commandLine )
{
    QString promptAgent = TextUnderlineColorHtml( agent->data.Name, COLOR_Gray) + " " + TextColorHtml( ">", COLOR_Gray) + " ";

    QString promptTime = UnixTimestampGlobalToStringLocal(timestamp);
    if ( !promptTime.isEmpty() )
        promptTime = TextColorHtml("[" + promptTime + "]", COLOR_SaturGray) + " ";

    QString promptTask  = "";
    if( !taskId.isEmpty() )
        promptTask = TextColorHtml("[" + taskId + "]", COLOR_SaturGray) + " ";

    QString promptUser = "";
    if ( !user.isEmpty() )
        promptUser = TextColorHtml(user, COLOR_Gray) + " ";

    if ( !commandLine.isEmpty() ) {
        QString promptCmd = TextBoltColorHtml(commandLine);

        QString prompt = promptTime + promptUser + promptTask + promptAgent + promptCmd;
        OutputTextEdit->append( prompt );
    }
}

/// SLOTS

void ConsoleWidget::processInput()
{
    QString commandLine = TrimmedEnds(InputLineEdit->text());
    InputLineEdit->clear();
    if (commandLine.isEmpty())
        return;

    auto cmdResult = commander->ProcessInput(commandLine );
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
        bool result = HttpReqAgentCommand(agent->data.Name, agent->data.Id, commandLine, cmdResult.message, agent->mainWidget->GetProfile(), &message, &ok);
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