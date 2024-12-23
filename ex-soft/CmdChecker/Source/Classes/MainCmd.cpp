#include <Classes/MainCmd.h>

MainCmd::MainCmd()
{
    this->setStyle();

    this->createUI();

    commander = new Commander();

    connect(selectRegButton, &QPushButton::clicked, this, [&](){ SelectRegFile();});
    connect(selectExtButton, &QPushButton::clicked, this, [&](){ SelectExtFile();});
    connect(loadRegButton, &QPushButton::clicked, this, [&](){ LoadRegFile();});
    connect(loadExtButton, &QPushButton::clicked, this, [&](){ LoadExtFile();});
    connect(execButton, &QPushButton::clicked, this, [&](){ExecuteCommand();});
}

MainCmd::~MainCmd() = default;

void MainCmd::createUI()
{
    this->setWindowTitle("CmdChecker");
    this->resize(1000, 800);

    mainLayout = new QGridLayout(this);

    pathRegInput = new QLineEdit(this);
    pathRegInput->setReadOnly(true);

    pathExtInput = new QLineEdit(this);
    pathExtInput->setReadOnly(true);

    commandInput = new QLineEdit(this);
    commandInput->setEnabled(false);

    consoleTextEdit = new QTextEdit(this);
    consoleTextEdit->setProperty( "TextEditStyle", "console" );

    selectRegButton = new QPushButton("Select Reg", this);
    selectExtButton = new QPushButton("Select Ext", this);
    loadRegButton   = new QPushButton("Load Reg", this);
    loadExtButton   = new QPushButton("Load Ext", this);
    execButton      = new QPushButton("Execute", this);
    execButton->setEnabled(false);

    model = new QStringListModel();
    completer = new QCompleter(model, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    commandInput->setCompleter(completer);

    mainLayout->addWidget(pathRegInput,    0, 0, 1, 1);
    mainLayout->addWidget(selectRegButton, 0, 1, 1, 1);
    mainLayout->addWidget(loadRegButton,   0, 2, 1, 1);
    mainLayout->addWidget(pathExtInput,    1, 0, 1, 1);
    mainLayout->addWidget(selectExtButton, 1, 1, 1, 1);
    mainLayout->addWidget(loadExtButton,   1, 2, 1, 1);
    mainLayout->addWidget(consoleTextEdit, 2, 0, 1, 3);
    mainLayout->addWidget(commandInput,    3, 0, 1, 2);
    mainLayout->addWidget(execButton,      3, 2, 1, 1);
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

    bool result = false;
    QString style = ReadFileString(theme, &result);
    if (result) {
        auto *app = qobject_cast<QApplication*>(QCoreApplication::instance());
        app->setStyleSheet(style);
    }
}

void MainCmd::SelectRegFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Select JSON file", "", "JSON Files (*.json)");
    if (!filePath.isEmpty()) {
        pathRegInput->setText(filePath);
        loadRegButton->setEnabled(true);
    }
}

void MainCmd::LoadRegFile()
{
    QString filePath = pathRegInput->text();
    if (!filePath.isEmpty()) {
        bool result = false;
        QByteArray jsonData = ReadFileBytearray(filePath, &result);
        if (result) {
            bool result2 = true;
            QString msg = ValidCommandsFile(jsonData, &result2);
            if ( result2 ) {
                commander->AddRegCommands(jsonData);
                commandInput->setEnabled(true);
                execButton->setEnabled(true);

                model->setStringList(commander->GetCommands());
            }
            else {
                consoleTextEdit->setText(msg);
            }
        }
    }
}

void MainCmd::SelectExtFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Select JSON file", "", "JSON Files (*.json)");
    if (!filePath.isEmpty()) {
        pathExtInput->setText(filePath);
        loadExtButton->setEnabled(true);
    }
}

void MainCmd::LoadExtFile()
{
    QString filePath = pathExtInput->text();
    if (!filePath.isEmpty()) {
        bool result = false;
        QByteArray jsonData = ReadFileBytearray(filePath, &result);
        if (result) {

            QJsonParseError parseError;
            QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonData, &parseError);
            if ( jsonDocument.isNull() || !jsonDocument.isObject()) {
                consoleTextEdit->setText("Invalid JSON document!");
                return;
            }

            QJsonObject rootObj = jsonDocument.object();
            if( !rootObj.contains("name") || !rootObj["name"].isString() ) {
                consoleTextEdit->setText("JSON document must include a required 'name' parameter");
                return;
            }

            QString extName = rootObj["name"].toString();

            if(; !rootObj.contains("extensions") || !rootObj["extensions"].isArray() ) {
                consoleTextEdit->setText("JSON document must include a required 'extensions' parameter");
                return;
            }

            QList<QJsonObject> exCommands;

            QJsonArray extensionsArray = rootObj.value("extensions").toArray();
            for (QJsonValue extensionValue : extensionsArray) {
                QJsonObject extJsonObject = extensionValue.toObject();

                if( !extJsonObject.contains("type") || !extJsonObject["type"].isString() ) {
                    consoleTextEdit->setText("Extension must include a required 'type' parameter");
                    return;
                }

                QString type = extJsonObject.value("type").toString();
                if(type == "command") {

                    if( !extJsonObject.contains("agents") || !extJsonObject["agents"].isArray() ) {
                        consoleTextEdit->setText("Extension must include a required 'agents' parameter");
                        return;
                    }
                    bool result2 = true;
                    QString msg = ValidExtCommand(extJsonObject, &result2);
                    if (!result2) {
                        consoleTextEdit->setText(msg);
                        return;
                    }

                    exCommands.push_back(extJsonObject);
                }
            }

            result = commander->AddExtCommands(filePath, extName, exCommands);
            if ( result ) {
                model->setStringList(commander->GetCommands());
            }
            else {
                consoleTextEdit->setText(commander->GetError());
            }
        }
    }
}

void MainCmd::ExecuteCommand()
{
    if ( !commander ) {
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