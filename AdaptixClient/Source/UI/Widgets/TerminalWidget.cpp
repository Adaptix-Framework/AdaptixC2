#include <UI/Widgets/TerminalWidget.h>
#include <Client/TerminalWorker.h>

TerminalWidget::TerminalWidget(Agent* a, QWidget* w)
{
    this->agent = a;
    this->mainWidget = w;
    this->termWidget = new QTermWidget(this, this);

    SetFont();
    SetSettings();

    this->createUI();

    connect(termWidget, &QWidget::customContextMenuRequested, this, &TerminalWidget::handleTerminalMenu);

    connect(programComboBox, &QComboBox::currentTextChanged, this, &TerminalWidget::onProgramChanged);
    connect(startButton,     &QPushButton::clicked,          this, &TerminalWidget::onStart);
    connect(restartButton,   &QPushButton::clicked,          this, &TerminalWidget::onRestart);
    connect(stopButton,      &QPushButton::clicked,          this, &TerminalWidget::onStop);
}

TerminalWidget::~TerminalWidget() = default;

void TerminalWidget::createUI()
{
    auto topWidget = new QWidget(this);

    programInput = new QLineEdit(this);
    programInput->setEnabled(false);

    programComboBox = new QComboBox(this);
    if (this->agent && this->agent->data.Os == OS_WINDOWS) {
        programComboBox->addItem("Cmd");
        programComboBox->addItem("Powershell");
        programInput->setText("C:\\Windows\\System32\\cmd.exe");
    }
    else {
        programComboBox->addItem("Shell");
        programComboBox->addItem("Bash");
        programInput->setText("/bin/sh");
    }
    programComboBox->addItem("Custom program");

    line_1 = new QFrame(this);
    line_1->setFrameShape(QFrame::VLine);
    line_1->setMinimumHeight(20);

    startButton = new QPushButton( QIcon(":/icons/start"), "", this );
    startButton->setIconSize( QSize( 24,24 ));
    startButton->setFixedSize(37, 28);
    startButton->setToolTip("Start terminal");

    restartButton = new QPushButton( QIcon(":/icons/restart"), "", this );
    restartButton->setIconSize( QSize( 24,24 ));
    restartButton->setFixedSize(37, 28);
    restartButton->setToolTip("Restart terminal");
    restartButton->setEnabled(false);

    stopButton = new QPushButton( QIcon(":/icons/stop"), "", this );
    stopButton->setIconSize( QSize( 24,24 ));
    stopButton->setFixedSize(37, 28);
    stopButton->setToolTip("Stop terminal");
    stopButton->setEnabled(false);

    line_2 = new QFrame(this);
    line_2->setFrameShape(QFrame::VLine);
    line_2->setMinimumHeight(20);

    statusDescLabel = new QLabel(this);
    statusDescLabel->setText("status:");

    statusLabel = new QLabel(this);
    statusLabel->setText("ready");

    spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    topHBoxLayout = new QHBoxLayout(this);
    topHBoxLayout->setContentsMargins(1, 3, 1, 3);
    topHBoxLayout->setSpacing(4);
    topHBoxLayout->addWidget(programInput);
    topHBoxLayout->addWidget(programComboBox);
    topHBoxLayout->addWidget(line_1);
    topHBoxLayout->addWidget(startButton);
    topHBoxLayout->addWidget(restartButton);
    topHBoxLayout->addWidget(stopButton);
    topHBoxLayout->addWidget(line_2);
    topHBoxLayout->addWidget(statusDescLabel);
    topHBoxLayout->addWidget(statusLabel);
    topHBoxLayout->addItem(spacer);

    topWidget->setLayout(topHBoxLayout);

    mainGridLayout = new QGridLayout( this );
    mainGridLayout->setContentsMargins(0, 0, 0, 0);
    mainGridLayout->setVerticalSpacing(1);
    mainGridLayout->addWidget( topWidget, 0, 0, 1, 1);
    mainGridLayout->addWidget( termWidget, 1, 0, 1, 1);

    mainGridLayout->setRowStretch(0, 0);
    mainGridLayout->setRowStretch(1, 1);

    programInput->setMinimumHeight(programComboBox->height());

    this->setLayout( mainGridLayout );
}

QTermWidget* TerminalWidget::Konsole()
{
    return this->termWidget;
}

void TerminalWidget::SetFont()
{
    QFont font = QApplication::font();

#ifdef Q_OS_MACOS
    font.setFamily(QStringLiteral("Monaco"));
#elif defined(Q_WS_QWS)
    font.setFamily(QStringLiteral("fixed"));
#else
    font.setFamily(QStringLiteral("Monospace"));
#endif

    font.setPointSize(10);
    termWidget->setTerminalFont(font);
}

void TerminalWidget::SetSettings()
{
    termWidget->setScrollBarPosition(QTermWidget::ScrollBarRight);
    termWidget->setBlinkingCursor(true);
    termWidget->setMargin(0);
    termWidget->setDrawLineChars(false);

    termWidget->setColorScheme("iTerm2 Default");

    for (const QString &arg : QApplication::arguments()) {
        if (termWidget->availableColorSchemes().contains(arg))
            termWidget->setColorScheme(arg);
        if (termWidget->availableKeyBindings().contains(arg))
            termWidget->setKeyBindings(arg);
    }

    termWidget->setContextMenuPolicy(Qt::CustomContextMenu);
}

void TerminalWidget::handleTerminalMenu(const QPoint &pos)
{
    QMenu menu(this->termWidget);
    menu.addAction("Find  (Ctrl+Shift+F)", this->termWidget, &QTermWidget::toggleShowSearchBar);
    menu.addAction("Copy  (Ctrl+Shift+C)", this->termWidget, &QTermWidget::copyClipboard);
    menu.addAction("Paste (Ctrl+Shift+V)", this->termWidget, &QTermWidget::pasteClipboard);
    menu.addAction("Clear (Ctrl+Shift+L)", this->termWidget, &QTermWidget::clear);
    menu.exec(this->termWidget->mapToGlobal(pos));
}

void TerminalWidget::onStart()
{
    programInput->setEnabled(false);
    programComboBox->setEnabled(false);
    startButton->setEnabled(false);
    restartButton->setEnabled(true);
    stopButton->setEnabled(true);

    auto adaptixWidget = qobject_cast<AdaptixWidget*>( mainWidget );
    if ( !adaptixWidget )
        return;

    auto profile = adaptixWidget->GetProfile();

    QString urlTemplate = "wss://%1:%2%3/channel";
    QString sUrl = urlTemplate.arg( profile->GetHost() ).arg( profile->GetPort() ).arg( profile->GetEndpoint() );

    QString agentId = this->agent->data.Id;
    QString terminalId = GenerateRandomString(8, "hex");
    QString program    = programInput->text().toUtf8().toBase64();
    int sizeW = this->termWidget->columns();
    int sizeH = this->termWidget->lines();

    if (sizeW < 512)
        sizeW = 512;

    QString terminalData = QString("%1|%2|%3|%4|%5").arg(agentId).arg(terminalId).arg(program).arg(sizeH).arg(sizeW).toUtf8().toBase64();

    QThread* thread = new QThread;
    TerminalWorker* worker = new TerminalWorker(this, profile->GetAccessToken(), sUrl, terminalData);
    worker->moveToThread(thread);

    connect(thread, &QThread::started,         worker, &TerminalWorker::start);
    connect(worker, &TerminalWorker::finished, thread, &QThread::quit);
    connect(worker, &TerminalWorker::finished, worker, &TerminalWorker::deleteLater);
    connect(thread, &QThread::finished,        thread, &QThread::deleteLater);

    connect(worker, &TerminalWorker::binaryMessageToTerminal, this, &TerminalWidget::recvDataFromSocket, Qt::QueuedConnection);

    thread->start();
}

void TerminalWidget::onRestart()
{

}

void TerminalWidget::onStop()
{
    programInput->setEnabled(programComboBox->currentText() == "Custom program");
    programComboBox->setEnabled(true);
    startButton->setEnabled(true);
    restartButton->setEnabled(false);
    stopButton->setEnabled(false);
}

void TerminalWidget::onProgramChanged()
{
    if (programComboBox->currentText() == "Custom program") {
        programInput->setEnabled(true);
        programInput->setFocus();
    }
    else if (programComboBox->currentText() == "Shell") {
        programInput->setText("/bin/sh");
        programInput->setEnabled(false);
    }
    else if (programComboBox->currentText() == "Bash") {
        programInput->setText("/bin/bash");
        programInput->setEnabled(false);
    }
    else if (programComboBox->currentText() == "Cmd") {
        programInput->setText("C:\\Windows\\System32\\cmd.exe");
        programInput->setEnabled(false);
    }
    else if (programComboBox->currentText() == "Powershell") {
        programInput->setText("C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
        programInput->setEnabled(false);
    }
}

void TerminalWidget::recvDataFromSocket(const QByteArray &msg)
{
    this->termWidget->recvData(msg.constData(), msg.size());
}
