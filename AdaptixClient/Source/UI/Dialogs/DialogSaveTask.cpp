#include <UI/Dialogs/DialogSaveTask.h>

DialogSaveTask::DialogSaveTask()
{
    this->createUI();

    connect(createButton, &QPushButton::clicked, this, &DialogSaveTask::onButtonSave);
    connect(cancelButton, &QPushButton::clicked, this, &DialogSaveTask::onButtonCancel);
}

DialogSaveTask::~DialogSaveTask() = default;

void DialogSaveTask::createUI()
{
    this->resize(900, 500);
    this->setWindowTitle( "Save Task" );
    this->setProperty("Main", "base");

    commandLineLabel = new QLabel("CommandLine:", this);
    commandLineInput = new QLineEdit(this);

    messageLabel = new QLabel("Message:", this);
    messageCombo = new QComboBox(this);
    messageCombo->addItems(QStringList() << "Success" << "Error" );
    messageInput = new QLineEdit(this);

    textLabel = new QLabel("Output:", this);
    textEdit  = new QTextEdit(this);

    spacer_1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Maximum);
    spacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Maximum);

    createButton = new QPushButton("Save", this);
    createButton->setProperty("ButtonStyle", "dialog");
    cancelButton = new QPushButton("Cancel", this);
    cancelButton->setProperty("ButtonStyle", "dialog");

    hLayoutBottom = new QHBoxLayout();
    hLayoutBottom->addItem(spacer_1);
    hLayoutBottom->addWidget(createButton);
    hLayoutBottom->addWidget(cancelButton);
    hLayoutBottom->addItem(spacer_2);

    mainGridLayout = new QGridLayout(this);
    mainGridLayout->setContentsMargins(4, 4,  4, 4 );
    mainGridLayout->addWidget(commandLineLabel, 0, 0, 1, 1);
    mainGridLayout->addWidget(commandLineInput, 0, 1, 1, 2);
    mainGridLayout->addWidget(messageLabel,     1, 0, 1, 1);
    mainGridLayout->addWidget(messageCombo,     1, 1, 1, 1);
    mainGridLayout->addWidget(messageInput,     1, 2, 1, 1);
    mainGridLayout->addWidget(textLabel,        2, 0, 1, 1);
    mainGridLayout->addWidget(textEdit,         2, 1, 1, 2);
    mainGridLayout->addLayout(hLayoutBottom,    3, 0, 1, 3);

    int buttonWidth  = createButton->width();
    createButton->setFixedWidth(buttonWidth);
    cancelButton->setFixedWidth(buttonWidth);

    int buttonHeight = createButton->height();
    createButton->setFixedHeight(buttonHeight);
    cancelButton->setFixedHeight(buttonHeight);
}

void DialogSaveTask::StartDialog(const QString &text)
{
    QString firstLine = text.section('\n', 0, 0);
    QString restLines = text.section('\n', 1);

    commandLineInput->setText(firstLine);
    messageInput->setText("Remote Terminal output");
    textEdit->setText(restLines);

    this->valid = false;
    this->message = "";
    this->exec();
}

bool DialogSaveTask::IsValid() const { return this->valid; }

QString DialogSaveTask::GetMessage() const { return this->message; }

TaskData DialogSaveTask::GetData() const { return this->data; }

void DialogSaveTask::onButtonSave()
{
    QString commandLine = commandLineInput->text();
    if (commandLine.isEmpty()) {
        this->valid = false;
        return;
    }

    int type = 7;
    if (messageCombo->currentText() == "Error")
        type = 6;

    data = {};
    data.CommandLine = commandLine;
    data.MessageType = type;
    data.Message     = messageInput->text();
    data.Output      = textEdit->toPlainText();

    this->valid = true;
    this->close();
}

void DialogSaveTask::onButtonCancel() { this->close(); }