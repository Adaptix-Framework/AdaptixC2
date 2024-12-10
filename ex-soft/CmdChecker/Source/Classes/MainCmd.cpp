#include <Classes/MainCmd.h>

MainCmd::MainCmd()
{
    this->setStyle();

    this->createUI();

    connect(selectButton, &QPushButton::clicked, this, [&](){SelectFile();});
    connect(loadButton, &QPushButton::clicked, this, [&](){LoadFile();});
    connect(execButton, &QPushButton::clicked, this, [&](){ExecuteCommand();});
}

MainCmd::~MainCmd() = default;

void MainCmd::createUI()
{
    this->setWindowTitle("CmdChecker");
//    this->resize(600, 400);

    mainLayout = new QGridLayout(this);

    pathInput = new QLineEdit(this);
    pathInput->setReadOnly(true);

    commandInput = new QLineEdit(this);
    commandInput->setEnabled(false);

    consoleTextEdit = new QTextEdit(this);
    consoleTextEdit->setProperty( "TextEditStyle", "console" );

    selectButton = new QPushButton("Select", this);
    loadButton   = new QPushButton("Load", this);
    execButton   = new QPushButton("Execute", this);
    execButton->setEnabled(false);

    mainLayout->addWidget(pathInput,      0,0,1,1);
    mainLayout->addWidget(selectButton,   0,1,1,1);
    mainLayout->addWidget(loadButton,     0,2,1,1);
    mainLayout->addWidget(consoleTextEdit, 1, 0, 1, 3);
    mainLayout->addWidget(commandInput,   2,0,1,2);
    mainLayout->addWidget(execButton,     2,2,1,1);
}

void MainCmd::setStyle()
{
    QGuiApplication::setWindowIcon( QIcon( ":/LogoLin" ) );

    int FontID = QFontDatabase::addApplicationFont( ":/fonts/DroidSansMono" );
    QString FontFamily = QFontDatabase::applicationFontFamilies( FontID ).at( 0 );
    auto Font = QFont( FontFamily );

    Font.setPointSize( 10 );
    QApplication::setFont( Font );

    QString theme = ":/themes/Dark";
//    QString themes = ":/themes/Arc (Light)";
//    QString themes = ":/themes/Nord";

    bool result = false;
    QString style = ReadFileString(theme, &result);
    if (result) {
        auto *app = qobject_cast<QApplication*>(QCoreApplication::instance());
        app->setStyleSheet(style);
    }
}

void MainCmd::SelectFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Select JSON file", "", "JSON Files (*.json)");
    if (!filePath.isEmpty()) {
        pathInput->setText(filePath);
        loadButton->setEnabled(true);
    }
}

void MainCmd::LoadFile()
{
    QString filePath = pathInput->text();
    if (!filePath.isEmpty()) {
        bool result = false;
        QByteArray jsonData = ReadFileBytearray(filePath, &result);
        if (result) {
            commander = new Commander(jsonData);
            if ( commander->IsValid() ) {
                commandInput->setEnabled(true);
                execButton->setEnabled(true);

                QStringList commandList = commander->GetCommands();
                completer = new QCompleter(commandList, this);
                completer->setCaseSensitivity(Qt::CaseInsensitive);
                commandInput->setCompleter(completer);
            }
            else {
                consoleTextEdit->setText(commander->GetError());
            }
        }
    }
}

void MainCmd::ExecuteCommand()
{
    if ( !commander || !commander->IsValid() ) {
        consoleTextEdit->setText("Error: Commands no loaded");
        return;
    }

    QString command = commandInput->text().trimmed();
    if (command.isEmpty()) {
        consoleTextEdit->setText("Enter command");
        return;
    }

    CommanderResult result = commander->ProcessInput( command );
    consoleTextEdit->setText(result.message);
}

void MainCmd::Start()
{
    this->show();
    QApplication::exec();
}