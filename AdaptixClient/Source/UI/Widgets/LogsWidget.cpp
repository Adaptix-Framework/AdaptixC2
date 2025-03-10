#include <UI/Widgets/LogsWidget.h>
#include <Utils/Convert.h>

LogsWidget::LogsWidget()
{
    this->createUI();
    logsConsoleTextEdit->setContextMenuPolicy(Qt::CustomContextMenu);
}

LogsWidget::~LogsWidget() = default;

void LogsWidget::createUI()
{
    /// Logs
//    logsLabel = new QLabel(this);
//    logsLabel->setText("Logs");
//    logsLabel->setAlignment(Qt::AlignCenter);

    logsConsoleTextEdit = new QTextEdit(this);
    logsConsoleTextEdit->setReadOnly(true);
    logsConsoleTextEdit->setProperty("TextEditStyle", "console" );

    logsGridLayout = new QGridLayout(this);
    logsGridLayout->setContentsMargins(1, 1, 1, 1);
    logsGridLayout->setVerticalSpacing(1);
//    logsGridLayout->addWidget(logsLabel, 0, 0, 1, 1);
    logsGridLayout->addWidget(logsConsoleTextEdit, 1, 0, 1, 1);

    logsWidget = new QWidget(this);
    logsWidget->setLayout(logsGridLayout);


    /// ToDo: todo list + sync chat
    todoLabel = new QLabel(this);
    todoLabel->setText("ToDo notes");
    todoLabel->setAlignment(Qt::AlignCenter);

    todoGridLayout = new QGridLayout(this);
    todoGridLayout->setContentsMargins(1, 1, 1, 1);
    todoGridLayout->setVerticalSpacing(1);
    todoGridLayout->setHorizontalSpacing(2);

    todoGridLayout->addWidget(todoLabel, 0, 0, 1, 1);

    todoWidget = new QWidget(this);
    todoWidget->setLayout(todoGridLayout);


    /// Main
    mainHSplitter = new QSplitter( Qt::Horizontal, this );
    mainHSplitter->setHandleWidth(3);
    mainHSplitter->addWidget(logsWidget);
//    mainHSplitter->addWidget(todoWidget);
//    mainHSplitter->setSizes(QList<int>({200, 40}));

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->setVerticalSpacing(0);
    mainGridLayout->addWidget( mainHSplitter, 0, 0, 1, 1);

    this->setLayout( mainGridLayout );
}

void LogsWidget::AddLogs( int type, qint64 time, const QString &message ) const
{
    QString sTime = UnixTimestampGlobalToStringLocal(time);
    QString log = QString("[%1] -> ").arg(sTime);

    if( type == EVENT_CLIENT_CONNECT )           log += TextColorHtml(message, COLOR_ConsoleWhite);
    else if( type == EVENT_CLIENT_DISCONNECT )   log += TextColorHtml(message, COLOR_Gray);
    else if( type == EVENT_LISTENER_START )      log += TextColorHtml(message, COLOR_BrightOrange);
    else if( type == EVENT_LISTENER_STOP )       log += TextColorHtml(message, COLOR_BrightOrange);
    else if( type == EVENT_AGENT_NEW )           log += TextColorHtml(message, COLOR_NeonGreen);
    else if( type == EVENT_TUNNEL_START )        log += TextColorHtml(message, COLOR_PastelYellow);
    else if( type == EVENT_TUNNEL_STOP )         log += TextColorHtml(message, COLOR_PastelYellow);
    else                                         log += message;

    logsConsoleTextEdit->append( log );
}

void LogsWidget::Clear() const
{
    logsConsoleTextEdit->clear();
}
