#include <Agent/Agent.h>
#include <Konsole/konsole.h>
#include <Workers/TerminalWorker.h>
#include <UI/Dialogs/DialogSaveTask.h>
#include <UI/Widgets/TerminalContainerWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/Settings.h>
#include <Client/AuthProfile.h>
#include <Client/Requestor.h>
#include <MainAdaptix.h>

REGISTER_DOCK_WIDGET(TerminalContainerWidget, "Remote Terminal", false)

TerminalTab::TerminalTab(Agent* a, AdaptixWidget* w, TerminalMode mode, QWidget* parent) : QWidget(parent)
{
    this->agent = a;
    this->adaptixWidget = w;
    this->terminalMode = mode;
    this->termWidget = new QTermWidget(this, this);
    this->termWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->termWidget->setMinimumSize(100, 100);

    this->createUI();

    SetFont();
    SetSettings();
    SetKeys();

    connect(termWidget,      &QWidget::customContextMenuRequested, this, &TerminalTab::handleTerminalMenu);
    connect(programComboBox, &QComboBox::currentTextChanged,       this, &TerminalTab::onProgramChanged);
    connect(keytabComboBox,  &QComboBox::currentTextChanged,       this, &TerminalTab::onKeytabChanged);
    connect(startButton,     &QPushButton::clicked,                this, &TerminalTab::onStart);
    connect(stopButton,      &QPushButton::clicked,                this, &TerminalTab::onStop);
}

TerminalTab::~TerminalTab()
{
    if (terminalWorker)
        QMetaObject::invokeMethod(terminalWorker, "stop", Qt::QueuedConnection);
}

void TerminalTab::createUI()
{
    topWidget = new QWidget(this);

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

    smartOutputCheckBox = new QCheckBox("Smart Output", this);
    smartOutputCheckBox->setToolTip("Filter duplicated output in Shell mode");
    smartOutputCheckBox->setVisible(terminalMode == TerminalModeShell);
    connect(smartOutputCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        smartOutputEnabled = checked;
        filterPhase = 0;
        outputBuffer.clear();
        lastSentCommand.clear();
        lastPrompt.clear();
    });

    line_4 = new QFrame(this);
    line_4->setFrameShape(QFrame::VLine);
    line_4->setMinimumHeight(20);
    line_4->setVisible(terminalMode == TerminalModeShell);

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
    topHBoxLayout->addWidget(smartOutputCheckBox);
    topHBoxLayout->addWidget(line_4);
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

void TerminalTab::setStatus(const QString &text)
{
    this->statusLabel->setText(text);

    if (text == "Stopped") {
        programInput->setEnabled(programComboBox->currentText() == "Custom program");
        programComboBox->setEnabled(true);
        startButton->setEnabled(true);
        stopButton->setEnabled(false);
    }
}

QTermWidget* TerminalTab::Konsole() { return this->termWidget; }

void TerminalTab::SetFont()
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

void TerminalTab::SetSettings()
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

void TerminalTab::handleTerminalMenu(const QPoint &pos)
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

    QAction *saveToTasksAction = menu.addAction("Save to Tasks Manager");
    connect(saveToTasksAction, &QAction::triggered, this, [this]() {
        QString text = termWidget->selectedText();

        DialogSaveTask* dialogTask = new DialogSaveTask();
        while (true) {
            dialogTask->StartDialog(text);
            if (dialogTask->IsValid())
                break;

            QString msg = dialogTask->GetMessage();
            if (msg.isEmpty()) {
                delete dialogTask;
                return;
            }

            MessageError(msg);
        }

        TaskData taskData = dialogTask->GetData();
        delete dialogTask;

        HttpReqTasksSaveAsync(agent->data.Id, taskData.CommandLine, taskData.MessageType, taskData.Message, taskData.Output, *(adaptixWidget->GetProfile()), [](bool success, const QString &message, const QJsonObject&) {
            if (!success)
                MessageError(message);
        });
    });

    menu.exec(this->termWidget->mapToGlobal(pos));
}

void TerminalTab::SetKeys()
{
    QShortcut *copyShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this->termWidget);
    connect(copyShortcut, &QShortcut::activated, this->termWidget, &QTermWidget::copyClipboard);

    QShortcut *pasteShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), this->termWidget);
    connect(pasteShortcut, &QShortcut::activated, this->termWidget, &QTermWidget::pasteClipboard);

    QShortcut *findShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), this->termWidget);
    connect(findShortcut, &QShortcut::activated, this->termWidget, &QTermWidget::toggleShowSearchBar);

    QShortcut *clearShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L), this->termWidget);
    connect(clearShortcut, &QShortcut::activated, this->termWidget, &QTermWidget::clear);
}

void TerminalTab::onStart()
{
    if ( !adaptixWidget )
        return;

    programInput->setEnabled(false);
    programComboBox->setEnabled(false);
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    this->setStatus("Waiting...");

    auto profile = adaptixWidget->GetProfile();

    QString urlTemplate = "wss://%1:%2%3/channel";
    QString sUrl = urlTemplate.arg( profile->GetHost() ).arg( profile->GetPort() ).arg( profile->GetEndpoint() );

    QString agentId = this->agent->data.Id;
    QString terminalId = GenerateRandomString(8, "hex");
    QString program    = programInput->text();
    int sizeW  = this->termWidget->columns();
    int sizeH  = this->termWidget->lines();
    int OecmCP = this->agent->data.OemCP;

    if (sizeW <= 0 || sizeH <= 0) {
        sizeW = 80;
        sizeH = 24;
    }

    QJsonObject otpData;
    otpData["agent_id"]     = agentId;
    otpData["terminal_id"]  = terminalId;
    otpData["program"]      = program;
    otpData["size_h"]       = sizeH;
    otpData["size_w"]       = sizeW;
    otpData["oem_cp"]       = OecmCP;

    QString otp;
    bool otpResult = HttpReqGetOTP("channel_terminal", otpData, profile->GetURL(), profile->GetAccessToken(), &otp);
    if (!otpResult) {
        programInput->setEnabled(true);
        programComboBox->setEnabled(true);
        startButton->setEnabled(true);
        stopButton->setEnabled(false);
        this->setStatus("OTP error");
        return;
    }

    terminalThread = new QThread;
    terminalWorker = new TerminalWorker(this, otp, sUrl);
    terminalWorker->moveToThread(terminalThread);

    connect(terminalThread, &QThread::started,         terminalWorker, &TerminalWorker::start);
    connect(terminalWorker, &TerminalWorker::finished, terminalThread, &QThread::quit);
    connect(terminalWorker, &TerminalWorker::finished, terminalWorker, &TerminalWorker::deleteLater);
    connect(terminalThread, &QThread::finished,        terminalThread, &QThread::deleteLater);

    connect(terminalWorker, &TerminalWorker::finished, this, [this]() { setStatus("Stopped"); }, Qt::QueuedConnection);
    connect(terminalWorker, &TerminalWorker::errorStop, this, [this]() { onStop(); }, Qt::QueuedConnection);

    connect(terminalWorker, &TerminalWorker::connectedToTerminal,     this, [this]() { setStatus("Running"); }, Qt::QueuedConnection);
    connect(terminalWorker, &TerminalWorker::binaryMessageToTerminal, this, &TerminalTab::recvDataFromSocket, Qt::QueuedConnection);

    connect(termWidget, SIGNAL(sendData(const char*,int)), this, SLOT(sendDataToSocket(const char*,int)), Qt::UniqueConnection);

    terminalThread->start();
}

void TerminalTab::onStop()
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

void TerminalTab::onProgramChanged()
{
    QString program = programComboBox->currentText();
    if (program == "Custom program") {
        programInput->setEnabled(true);
        programInput->clear();
        programInput->setFocus();
    }
    else {
        programInput->setEnabled(false);

        if (this->agent && this->agent->data.Os == OS_WINDOWS) {
            if (program == "Cmd")
                programInput->setText("C:\\Windows\\System32\\cmd.exe");
            else if (program == "Powershell")
                programInput->setText("C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe");
        }
        else if (this->agent && this->agent->data.Os == OS_LINUX) {
            if (program == "Shell")
                programInput->setText("/bin/sh");
            else if (program == "Bash")
                programInput->setText("/bin/bash");
        }
        else {
            if (program == "ZSH")
                programInput->setText("/bin/zsh");
            else if (program == "Shell")
                programInput->setText("/bin/sh");
            else if (program == "Bash")
                programInput->setText("/bin/bash");
        }
    }
}

void TerminalTab::onKeytabChanged()
{
    QString keytab = keytabComboBox->currentText();
    termWidget->setKeyBindings(keytab);
}

void TerminalTab::recvDataFromSocket(const QByteArray &msg)
{
    if (smartOutputEnabled && terminalMode == TerminalModeShell && !lastSentCommand.isEmpty()) {
        QByteArray filtered = processSmartOutput(msg);
        if (!filtered.isEmpty())
            termWidget->recvData(filtered.data(), filtered.size());
    }
    else {
        termWidget->recvData(msg.data(), msg.size());
    }
}

QByteArray TerminalTab::processSmartOutput(const QByteArray &data)
{
    outputBuffer.append(data);
    QByteArray result;

    bool emptyCommandMode = (lastSentCommand == QByteArray("\x01"));
    
    while (true) {
        int nlPos = outputBuffer.indexOf('\n');
        if (nlPos == -1) {
            // No newline - check if it's a prompt (ends with >)
            QByteArray trimmedBuf = outputBuffer.trimmed();
            if (trimmedBuf.endsWith(">")) {
                if (filterPhase == 2) {
                    // Clean prompt without newline - show it and move to phase 3
                    result.append(outputBuffer);
                    outputBuffer.clear();
                    filterPhase = 3;
                }
                else if (emptyCommandMode && filterPhase == 1) {
                    // Empty command mode - check for duplicate prompt
                    if (lastPrompt.isEmpty() || trimmedBuf != lastPrompt) {
                        lastPrompt = trimmedBuf;
                        result.append(outputBuffer);
                    }
                    outputBuffer.clear();
                    filterPhase = 3;
                }
                else if (filterPhase >= 1 && filterPhase != 2) {
                    result.append(outputBuffer);
                    outputBuffer.clear();
                }
            }
            else if (filterPhase >= 1 && filterPhase != 2) {
                result.append(outputBuffer);
                outputBuffer.clear();
            }
            break;
        }
        
        QByteArray line = outputBuffer.left(nlPos + 1);
        outputBuffer.remove(0, nlPos + 1);
        
        QByteArray trimmedLine = line.trimmed();
        
        // Build pattern: ">command" to detect "prompt>command" line
        QByteArray promptCmdPattern = ">" + lastSentCommand;
        
        switch (filterPhase) {
            case 0:
                // Phase 0: Skip echo of command
                if (trimmedLine == lastSentCommand) {
                    filterPhase = 1;
                }
                else {
                    result.append(line);
                }
                break;
                
            case 1:
                if (emptyCommandMode) {
                    // Empty command mode - skip prompts with newline, keep only last one (without \n)
                    if (trimmedLine.endsWith(">")) {
                        // Skip prompts with newline - we'll show the last one without \n
                    }
                    else {
                        result.append(line);
                    }
                }
                else {
                    // Normal mode: Show result until we see "prompt>command" line
                    if (trimmedLine.contains(promptCmdPattern)) {
                        // Found duplicate marker, start skipping
                        filterPhase = 2;
                    }
                    else {
                        result.append(line);
                    }
                }
                break;
                
            case 2:
                // Phase 2: Skip duplicates until clean prompt (ends with > but no command after)
                if (trimmedLine.endsWith(">") && !trimmedLine.contains(promptCmdPattern)) {
                    result.append(line);
                    filterPhase = 3;
                }
                break;
                
            default:
                result.append(line);
                break;
        }
    }
    
    return result;
}

void TerminalTab::sendDataToSocket(const char* data, int size)
{
    if (!terminalWorker)
        return;

    if (terminalMode == TerminalModeShell) {
        for (int i = 0; i < size; i++) {
            char ch = data[i];

            if (ch == '\r' || ch == '\n') {
                shellInputBuffer.append('\n');
                termWidget->recvData("\r\n", 2);

                if (!shellInputBuffer.isEmpty()) {
                    QByteArray payload = shellInputBuffer;
                    
                    if (smartOutputEnabled) {
                        QByteArray trimmedCmd = shellInputBuffer.trimmed();
                        if (!trimmedCmd.isEmpty()) {
                            lastSentCommand = trimmedCmd;
                            filterPhase = 0;
                        } else {
                            // Empty command - use special marker to filter duplicate prompts
                            lastSentCommand = QByteArray("\x01");  // Special marker for empty command
                            filterPhase = 1;
                        }
                        outputBuffer.clear();
                        lastPrompt.clear();
                    }
                    
                    shellInputBuffer.clear();
                    terminalWorker->sendData(payload);
                }
            }
            else if (ch == 0x03) {
                shellInputBuffer.clear();
                termWidget->recvData("^C\r\n", 4);
                QByteArray ctrlC(1, 0x03);
                terminalWorker->sendData(ctrlC);
            }
            else if (ch == 0x7f || ch == '\b') {
                if (!shellInputBuffer.isEmpty()) {
                    // Remove full UTF-8 character (may be multiple bytes)
                    // UTF-8 continuation bytes: 10xxxxxx (0x80-0xBF)
                    int bytesToRemove = 1;
                    while (shellInputBuffer.size() > bytesToRemove) {
                        unsigned char prevByte = static_cast<unsigned char>(shellInputBuffer[shellInputBuffer.size() - bytesToRemove]);
                        if ((prevByte & 0xC0) == 0x80) {
                            // This is a continuation byte, need to remove more
                            bytesToRemove++;
                        } else {
                            break;
                        }
                    }
                    shellInputBuffer.chop(bytesToRemove);
                    termWidget->recvData("\b \b", 3);
                }
            }
            else {
                unsigned char uch = static_cast<unsigned char>(ch);
                if (uch >= 0x20 || ch == '\t') {
                    shellInputBuffer.append(ch);
                    termWidget->recvData(&ch, 1);
                }
            }
        }
    }
    else {
        QByteArray payload(data, size);
        terminalWorker->sendData(payload);
    }
}

bool TerminalTab::isRunning() const { return terminalWorker != nullptr; }





TerminalContainerWidget::TerminalContainerWidget(Agent* a, AdaptixWidget* w, TerminalMode mode) : DockTab(QString("%1 [%2]").arg(mode == TerminalModeShell ? "Shell" : "Terminal").arg(a->data.Id), w->GetProfile()->GetProject())
{
    this->agent = a;
    this->adaptixWidget = w;
    this->terminalMode = mode;

    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    tabWidget = new VerticalTabWidget(this);
    tabWidget->setTabsClosable(true);
    tabWidget->tabBar()->setShowAddButton(true);

    connect(tabWidget->tabBar(), &VerticalTabBar::addTabRequested,      this, &TerminalContainerWidget::addNewTerminal);
    connect(tabWidget,                 &VerticalTabWidget::tabCloseRequested, this, &TerminalContainerWidget::onTabCloseRequested);

    mainLayout->addWidget(tabWidget);
    this->setLayout(mainLayout);

    addNewTerminal();

    this->dockWidget->setWidget(this);
}

TerminalContainerWidget::~TerminalContainerWidget() = default;

void TerminalContainerWidget::addNewTerminal()
{
    tabCounter++;
    TerminalTab* terminalTab = new TerminalTab(agent, adaptixWidget, terminalMode, this);
    QString tabName = (terminalMode == TerminalModeShell) ? QString("Shell %1").arg(tabCounter) : QString("Term %1").arg(tabCounter);
    int index = tabWidget->addTab(terminalTab, tabName);
    tabWidget->setCurrentIndex(index);
}

void TerminalContainerWidget::closeTab(int index)
{
    if (tabWidget->count() > 1) {
        TerminalTab* tab = qobject_cast<TerminalTab*>(tabWidget->widget(index));
        if (tab) {
            tab->onStop();
        }
        tabWidget->removeTab(index);
        if (tab) {
            tab->deleteLater();
        }
    }
}

void TerminalContainerWidget::onTabCloseRequested(int index)
{
    TerminalTab* tab = qobject_cast<TerminalTab*>(tabWidget->widget(index));
    if (tab && tab->isRunning()) {
        QMessageBox::StandardButton reply = QMessageBox::question(nullptr, "Close Confirmation",
                                          "Terminal is still running. Stop and close it?",
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
    }

    closeTab(index);
}
