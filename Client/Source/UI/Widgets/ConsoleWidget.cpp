#include <UI/Widgets/ConsoleWidget.h>
#include <Client/Requestor.h>

ConsoleWidget::ConsoleWidget( Agent* a, Commander* c)
{
    agent = a;
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

void ConsoleWidget::OutputConsole( int type, qint64 timestamp, QString taskId, QString user, QString commandLine, QString message, QString data, bool taskFinished )
{
    QString prompt      = "";
    QString promptTime  = "";
    QString promptUser  = "";
    QString promptTask  = "";
    QString promptAgent = "";
    QString promptCmd   = "";

    promptTime = UnixTimestampGlobalToStringLocal(timestamp);
    if ( !promptTime.isEmpty() ) {
        promptTime = TextColorHtml("[" + promptTime + "]", COLOR_SaturGray) + " ";
    }

    if( !taskId.isEmpty() ) {
        promptTask = TextColorHtml("[" + taskId + "]", COLOR_SaturGray) + " ";
    }

    if ( !commandLine.isEmpty() ) {
        promptAgent = TextUnderlineColorHtml( agent->data.Name, COLOR_Gray) + " " + TextColorHtml( ">", COLOR_Gray) + " ";
        promptCmd   = TextBoltColorHtml(commandLine);

        if(type == CONSOLE_OUT_INFO || type == CONSOLE_OUT_SUCCESS || type == CONSOLE_OUT_ERROR ) {
            if ( !user.isEmpty() ) {
                promptUser = TextColorHtml(user, COLOR_Gray) + " ";
            }
        }
        prompt = promptTime + promptUser + promptTask + promptAgent + promptCmd;
        OutputTextEdit->append( prompt );
        OutputTextEdit->append("");
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

        QString printMessage = promptTime + mark + " " + message.trimmed().toHtmlEscaped() + " " + promptTask;
        OutputTextEdit->append( printMessage );
    }

    if ( !data.isEmpty() ) {
        OutputTextEdit->append( data.trimmed().toHtmlEscaped() );
        OutputTextEdit->append( "");
    }

    if (taskFinished) {
        QString deleter = TextBoltColorHtml( "+-------------------------------------------------------------+", COLOR_Gray) + "<br>";
        OutputTextEdit->append( deleter );
    }
}

/// SLOTS

void ConsoleWidget::processInput()
{
    auto cmd = InputLineEdit->text().trimmed();
    InputLineEdit->clear();
    if (cmd.isEmpty())
        return;

    auto cmdResult = commander->ProcessInput(cmd );
    if ( cmdResult.output ){
        if(cmdResult.error)
            this->OutputConsole(CONSOLE_OUT_LOCAL_ERROR, 0, "", "", cmd, cmdResult.message, "", true);
        else
            this->OutputConsole(CONSOLE_OUT_LOCAL, 0, "", "", cmd, "", cmdResult.message, true);
    }
    else {
        QString message = QString();
        bool ok = false;
        bool result = HttpReqAgentCommand(agent->data.Name, agent->data.Id, cmd, cmdResult.message, agent->mainWidget->GetProfile(), &message, &ok);
        if( !result ) {
            MessageError("JWT error");
            return;
        }
        if (!ok) {
            this->OutputConsole(CONSOLE_OUT_LOCAL_ERROR, 0, "", "", cmd, message, "", true);
        }
    }
}