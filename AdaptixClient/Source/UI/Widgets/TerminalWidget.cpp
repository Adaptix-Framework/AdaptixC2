#include <Agent/Agent.h>
#include <Konsole/konsole.h>
#include <Workers/TerminalWorker.h>
#include <UI/Widgets/TerminalWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Settings.h>
#include <Client/AuthProfile.h>
#include <MainAdaptix.h>

TerminalWidget::TerminalWidget(Agent* a, QWidget* w)
{
    this->agent = a;
    this->mainWidget = w;
    this->termWidget = new QTermWidget(this, this);

    this->createUI();

    SetFont();
    SetSettings();
    SetKeys();

    connect(termWidget, &QWidget::customContextMenuRequested, this, &TerminalWidget::handleTerminalMenu);

    connect(programComboBox, &QComboBox::currentTextChanged, this, &TerminalWidget::onProgramChanged);
    connect(keytabComboBox,  &QComboBox::currentTextChanged, this, &TerminalWidget::onKeytabChanged);
    connect(startButton,     &QPushButton::clicked,          this, &TerminalWidget::onStart);
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
    else if (this->agent && this->agent->data.Os == OS_LINUX){
        programComboBox->addItem("Shell");
        programComboBox->addItem("Bash");
        programInput->setText("/bin/sh");
    }
    else {
        programComboBox->addItem("ZSH");
        programComboBox->addItem("Shell");
        programComboBox->addItem("Bash");
        programInput->setText("/bin/zsh");
    }
    programComboBox->addItem("Custom program");

    keytabLabel = new QLabel(this);
    keytabLabel->setText("Keytab:");

    keytabComboBox = new QComboBox(this);
    keytabComboBox->addItem("linux_console");
    keytabComboBox->addItem("linux_default");
    keytabComboBox->addItem("macos_macbook");
    keytabComboBox->addItem("macos_default");
    keytabComboBox->addItem("windows_conpty");
    keytabComboBox->addItem("windows_winpty");
    keytabComboBox->addItem("solaris");
    keytabComboBox->addItem("vt100");
    keytabComboBox->addItem("vt420pc");
    keytabComboBox->addItem("x11");

    line_1 = new QFrame(this);
    line_1->setFrameShape(QFrame::VLine);
    line_1->setMinimumHeight(20);

    startButton = new QPushButton( QIcon(":/icons/start"), "", this );
    startButton->setIconSize( QSize( 24,24 ));
    startButton->setFixedSize(37, 28);
    startButton->setToolTip("Start terminal");

    stopButton = new QPushButton( QIcon(":/icons/stop"), "", this );
    stopButton->setIconSize( QSize( 24,24 ));
    stopButton->setFixedSize(37, 28);
    stopButton->setToolTip("Stop terminal");
    stopButton->setEnabled(false);

    line_2 = new QFrame(this);
    line_2->setFrameShape(QFrame::VLine);
    line_2->setMinimumHeight(20);

    line_3 = new QFrame(this);
    line_3->setFrameShape(QFrame::VLine);
    line_3->setMinimumHeight(20);

    statusDescLabel = new QLabel(this);
    statusDescLabel->setText("status:");

    statusLabel = new QLabel(this);
    statusLabel->setText("Stopped");

    spacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    topHBoxLayout = new QHBoxLayout(this);
    topHBoxLayout->setContentsMargins(1, 3, 1, 3);
    topHBoxLayout->setSpacing(4);
    topHBoxLayout->addWidget(keytabLabel);
    topHBoxLayout->addWidget(keytabComboBox);
    topHBoxLayout->addWidget(line_1);
    topHBoxLayout->addWidget(programInput);
    topHBoxLayout->addWidget(programComboBox);
    topHBoxLayout->addWidget(line_2);
    topHBoxLayout->addWidget(startButton);
    topHBoxLayout->addWidget(stopButton);
    topHBoxLayout->addWidget(line_3);
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

void TerminalWidget::setStatus(const QString &text)
{
    this->statusLabel->setText(text);

    if (text == "Stopped") {
        programInput->setEnabled(programComboBox->currentText() == "Custom program");
        programComboBox->setEnabled(true);
        startButton->setEnabled(true);
        stopButton->setEnabled(false);
    }
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
#elif defined(Q_OS_WIN)
    font.setFamily(QStringLiteral("Consolas"));
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

    termWidget->setHistorySize(GlobalClient->settings->data.RemoteTerminalBufferSize);

    termWidget->setColorScheme("iTerm2 Default");

    if (this->agent->data.Os == OS_WINDOWS) {
        termWidget->setKeyBindings("windows_conpty");
        keytabComboBox->setCurrentText("windows_conpty");
    }
    else if (this->agent->data.Os == OS_MAC) {
        termWidget->setKeyBindings("macos_macbook");
        keytabComboBox->setCurrentText("macos_macbook");
    }
    else if (this->agent->data.Os == OS_LINUX) {
        termWidget->setKeyBindings("linux_console");
        keytabComboBox->setCurrentText("linux_console");
    }

    termWidget->setContextMenuPolicy(Qt::CustomContextMenu);
}

void TerminalWidget::handleTerminalMenu(const QPoint &pos)
{
    QMenu menu(this->termWidget);
    menu.addAction("Copy  (Ctrl+Shift+C)", this->termWidget, &QTermWidget::copyClipboard);
    menu.addAction("Paste (Ctrl+Shift+V)", this->termWidget, &QTermWidget::pasteClipboard);
    menu.addAction("Clear (Ctrl+Shift+L)", this->termWidget, &QTermWidget::clear);
    menu.addAction("Find  (Ctrl+Shift+F)", this->termWidget, &QTermWidget::toggleShowSearchBar);
    menu.addSeparator();

    QAction *setBufferSizeAction = menu.addAction("Set buffer size...");
    connect(setBufferSizeAction, &QAction::triggered, this, [this]() {
        bool ok;
        int newSize = QInputDialog::getInt(this, "Set buffer size", "Enter maximum number of lines:", termWidget->historySize(), 100, 100000, 100, &ok);
        if (ok)
            termWidget->setHistorySize(newSize);
    });

    menu.exec(this->termWidget->mapToGlobal(pos));
}

void TerminalWidget::SetKeys()
{
    /// Ctrl+Shift+C: Copy
    QShortcut *copyShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this->termWidget);
    connect(copyShortcut, &QShortcut::activated, this->termWidget, &QTermWidget::copyClipboard);
    /// Ctrl+Shift+V: Paste
    QShortcut *pasteShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), this->termWidget);
    connect(pasteShortcut, &QShortcut::activated, this->termWidget, &QTermWidget::pasteClipboard);
    /// Ctrl+Shift+F: Find
    QShortcut *findShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), this->termWidget);
    connect(findShortcut, &QShortcut::activated, this->termWidget, &QTermWidget::toggleShowSearchBar);
    /// Ctrl+Shift+L: Clear
    QShortcut *clearShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L), this->termWidget);
    connect(clearShortcut, &QShortcut::activated, this->termWidget, &QTermWidget::clear);
}

void TerminalWidget::onStart()
{
    programInput->setEnabled(false);
    programComboBox->setEnabled(false);
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    this->setStatus("Waiting...");

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

    terminalThread = new QThread;
    terminalWorker = new TerminalWorker(this, profile->GetAccessToken(), sUrl, terminalData);
    terminalWorker->moveToThread(terminalThread);

    connect(terminalThread, &QThread::started,         terminalWorker, &TerminalWorker::start);
    connect(terminalWorker, &TerminalWorker::finished, terminalThread, &QThread::quit);
    connect(terminalWorker, &TerminalWorker::finished, terminalWorker, &TerminalWorker::deleteLater);
    connect(terminalThread, &QThread::finished,        terminalThread, &QThread::deleteLater);

    connect(terminalWorker, &TerminalWorker::finished, this, [this]() { setStatus("Stopped"); }, Qt::QueuedConnection);
    connect(terminalWorker, &TerminalWorker::errorStop, this, [this]() { onStop(); }, Qt::QueuedConnection);

    connect(terminalWorker, &TerminalWorker::connectedToTerminal,     this, [this]() { setStatus("Running"); }, Qt::QueuedConnection);
    connect(terminalWorker, &TerminalWorker::binaryMessageToTerminal, this, &TerminalWidget::recvDataFromSocket, Qt::QueuedConnection);

    terminalThread->start();
}

void TerminalWidget::onRestart()
{

}

void TerminalWidget::onStop()
{

    if (!terminalWorker || !terminalThread)
        return;

    auto worker = terminalWorker;
    auto thread = terminalThread;

    terminalWorker = nullptr;
    terminalThread = nullptr;

    connect(worker, &TerminalWorker::finished, this, [this, thread]() {
        if (thread->isRunning()) {
            thread->quit();
            thread->wait();
        }

        thread->deleteLater();

        setStatus("Stopped");
    });

    QMetaObject::invokeMethod(worker, "stop", Qt::QueuedConnection);
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
    else if (programComboBox->currentText() == "ZSH") {
        programInput->setText("/bin/zsh");
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

void TerminalWidget::onKeytabChanged()
{
    termWidget->setKeyBindings(keytabComboBox->currentText());
}

void TerminalWidget::recvDataFromSocket(const QByteArray &msg)
{
    this->termWidget->recvData(msg.constData(), msg.size());
}
