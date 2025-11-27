#include <UI/MainUI.h>
#include <UI/Dialogs/DialogSettings.h>
#include <MainAdaptix.h>
#include <Client/Settings.h>
#include <Utils/TitleBarStyle.h>

DialogSettings::DialogSettings(Settings* s)
{
    settings = s;

    this->createUI();

    themeCombo->setCurrentText(s->data.MainTheme);
    fontFamilyCombo->setCurrentText(s->data.FontFamily);
    fontSizeSpin->setValue(s->data.FontSize);
    graphCombo1->setCurrentText(s->data.GraphVersion);
    terminalSizeSpin->setValue(s->data.RemoteTerminalBufferSize);

    consoleSizeSpin->setValue(s->data.ConsoleBufferSize);
    consoleTimeCheckbox->setChecked(s->data.ConsoleTime);
    consoleNoWrapCheckbox->setChecked(s->data.ConsoleNoWrap);
    consoleAutoScrollCheckbox->setChecked(s->data.ConsoleAutoScroll);

    for ( int i = 0; i < 15; i++)
        sessionsCheck[i]->setChecked(s->data.SessionsTableColumns[i]);

    sessionsHealthCheck->setChecked(s->data.CheckHealth);
    sessionsCoafSpin->setValue(s->data.HealthCoaf);
    sessionsOffsetSpin->setValue(s->data.HealthOffset);

    for ( int i = 0; i < 11; i++)
        tasksCheck[i]->setChecked(s->data.TasksTableColumns[i]);

    connect(themeCombo,         &QComboBox::currentTextChanged, buttonApply, [this](const QString &text){buttonApply->setEnabled(true);} );
    connect(fontFamilyCombo,    &QComboBox::currentTextChanged, buttonApply, [this](const QString &text){buttonApply->setEnabled(true);} );
    connect(fontSizeSpin,       &QSpinBox::valueChanged,        buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(sessionsCoafSpin,   &QDoubleSpinBox::valueChanged,  buttonApply, [this](double){buttonApply->setEnabled(true);} );
    connect(sessionsOffsetSpin, &QSpinBox::valueChanged,        buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(terminalSizeSpin,   &QSpinBox::valueChanged,        buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(consoleSizeSpin,    &QSpinBox::valueChanged,        buttonApply, [this](int){buttonApply->setEnabled(true);} );

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(consoleTimeCheckbox,       &QCheckBox::checkStateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(consoleNoWrapCheckbox,     &QCheckBox::checkStateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(consoleAutoScrollCheckbox, &QCheckBox::checkStateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(sessionsHealthCheck,       &QCheckBox::checkStateChanged, this, &DialogSettings::onHealthChange );
#else
    connect(consoleTimeCheckbox,       &QCheckBox::stateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(consoleNoWrapCheckbox,     &QCheckBox::stateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(consoleAutoScrollCheckbox, &QCheckBox::stateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(sessionsHealthCheck,       &QCheckBox::stateChanged, this, &DialogSettings::onHealthChange );
#endif

    for ( int i = 0; i < 15; i++) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        connect(sessionsCheck[i],  &QCheckBox::checkStateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );
#else
        connect(sessionsCheck[i],  &QCheckBox::stateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );
#endif
    }

    connect(graphCombo1, &QComboBox::currentTextChanged, buttonApply, [this](const QString &text){buttonApply->setEnabled(true);} );

    for ( int i = 0; i < 11; i++) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        connect(tasksCheck[i],  &QCheckBox::checkStateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );
#else
        connect(tasksCheck[i],  &QCheckBox::stateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );
#endif
    }

    connect(listSettings, &QListWidget::currentRowChanged, this, &DialogSettings::onStackChange);
    connect(buttonApply,  &QPushButton::clicked,           this, &DialogSettings::onApply);
    connect(buttonClose,  &QPushButton::clicked,           this, &DialogSettings::onClose);
}

void DialogSettings::createUI()
{
    this->setWindowTitle("Adaptix Settings");
    this->resize(600, 300);
    this->setProperty("Main", "base");

    mainSettingWidget = new QWidget(this);
    mainSettingLayout = new QGridLayout(mainSettingWidget);

    themeLabel = new QLabel("Main theme: ", mainSettingWidget);
    themeCombo = new QComboBox(mainSettingWidget);
    themeCombo->addItem("Light_Arc");
    themeCombo->addItem("Dark");
    themeCombo->addItem("Dracula");
    themeCombo->addItem("Fallout");

    fontSizeLabel = new QLabel("Font size: ", mainSettingWidget);
    fontSizeSpin  = new QSpinBox(mainSettingWidget);
    fontSizeSpin->setMinimum(7);
    fontSizeSpin->setMaximum(30);

    fontFamilyLabel = new QLabel("Font family: ", mainSettingWidget);
    fontFamilyCombo = new QComboBox(mainSettingWidget);
    fontFamilyCombo->addItem("Adaptix - DejaVu Sans Mono");
    fontFamilyCombo->addItem("Adaptix - Droid Sans Mono");
    fontFamilyCombo->addItem("Adaptix - VT323");
    fontFamilyCombo->addItem("Adaptix - Hack");
    fontFamilyCombo->addItem("Adaptix - Anonymous Pro");
    fontFamilyCombo->addItem("Adaptix - Space Mono");

    graphLabel1 = new QLabel("Session Graph version:", mainSettingWidget);
    graphCombo1 = new QComboBox(mainSettingWidget);
    graphCombo1->addItem("Version 1");
    graphCombo1->addItem("Version 2");

    terminalSizeLabel = new QLabel("RemoteTerminal buffer (lines):", mainSettingWidget);
    terminalSizeSpin  = new QSpinBox(mainSettingWidget);
    terminalSizeSpin->setMinimum(1);
    terminalSizeSpin->setMaximum(100000);

    consoleGroup  = new QGroupBox("Agent Console", mainSettingWidget);

    consoleSizeLabel = new QLabel("Buffer size (lines):", consoleGroup);
    consoleSizeSpin  = new QSpinBox(consoleGroup);
    consoleSizeSpin->setMinimum(1);
    consoleSizeSpin->setMaximum(100000);

    consoleTimeCheckbox = new QCheckBox("Print date and time", consoleGroup);
    consoleNoWrapCheckbox = new QCheckBox("No Wrap mode", consoleGroup);
    consoleAutoScrollCheckbox = new QCheckBox("Auto Scroll mode", consoleGroup);

    consoleGroupLayout = new QGridLayout(consoleGroup);
    consoleGroupLayout->addWidget(consoleSizeLabel,          0, 0, 1, 1);
    consoleGroupLayout->addWidget(consoleSizeSpin,           0, 1, 1, 1);
    consoleGroupLayout->addWidget(consoleTimeCheckbox,       1, 0, 1, 2);
    consoleGroupLayout->addWidget(consoleNoWrapCheckbox,     2, 0, 1, 2);
    consoleGroupLayout->addWidget(consoleAutoScrollCheckbox, 3, 0, 1, 2);
    consoleGroup->setLayout(consoleGroupLayout);

    mainSettingLayout->addWidget(themeLabel,        0, 0, 1, 1);
    mainSettingLayout->addWidget(themeCombo,        0, 1, 1, 1);
    mainSettingLayout->addWidget(fontFamilyLabel,   1, 0, 1, 1);
    mainSettingLayout->addWidget(fontFamilyCombo,   1, 1, 1, 1);
    mainSettingLayout->addWidget(fontSizeLabel,     2, 0, 1, 1);
    mainSettingLayout->addWidget(fontSizeSpin,      2, 1, 1, 1);
    mainSettingLayout->addWidget(graphLabel1,       3, 0, 1, 1);
    mainSettingLayout->addWidget(graphCombo1,       3, 1, 1, 1);
    mainSettingLayout->addWidget(terminalSizeLabel, 4, 0, 1, 1);
    mainSettingLayout->addWidget(terminalSizeSpin,  4, 1, 1, 1);
    mainSettingLayout->addWidget(consoleGroup,      5, 0, 1, 2);

    mainSettingWidget->setLayout(mainSettingLayout);


    sessionsWidget = new QWidget(this);
    sessionsLayout = new QGridLayout(sessionsWidget);
    sessionsGroup  = new QGroupBox("Columns", sessionsWidget);

    QStringList sessionsCheckboxLabels = {
        "Agent ID", "Agent Type", "External", "Listener", "Internal",
        "Domain", "Computer", "User", "OS", "Process",
        "PID", "TID", "Tags", "Last", "Sleep"
    };

    for (int i = 0; i < 15; ++i)
        sessionsCheck[i] = new QCheckBox(sessionsCheckboxLabels[i], sessionsGroup);

    sessionsGroupLayout = new QGridLayout(sessionsGroup);
    sessionsGroupLayout->addWidget(sessionsCheck[0], 0, 0, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[1], 0, 1, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[2], 1, 0, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[3], 1, 1, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[4], 2, 0, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[5], 2, 1, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[6], 3, 0, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[7], 3, 1, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[8], 4, 0, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[9], 4, 1, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[10], 5, 0, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[11], 5, 1, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[12], 6, 0, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[13], 6, 1, 1, 1);
    sessionsGroupLayout->addWidget(sessionsCheck[14], 7, 0, 1, 1);
    sessionsGroup->setLayout(sessionsGroupLayout);

    sessionsHealthCheck = new QCheckBox("Check Health", sessionsWidget);

    QSpacerItem* horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

    sessionsLabel1 = new QLabel(sessionsWidget);
    sessionsLabel1->setText("Sleeptime *");

    sessionsLabel2 = new QLabel(sessionsWidget);
    sessionsLabel2->setText("+");

    sessionsLabel3 = new QLabel(sessionsWidget);
    sessionsLabel3->setText("sec");

    sessionsCoafSpin = new QDoubleSpinBox(sessionsWidget);
    sessionsCoafSpin->setMinimum(1.0);
    sessionsCoafSpin->setMaximum(5.0);
    sessionsCoafSpin->setSingleStep(0.1);

    sessionsOffsetSpin = new QSpinBox(sessionsWidget);
    sessionsOffsetSpin->setMinimum(1);
    sessionsOffsetSpin->setMaximum(10000);

    sessionsLayout->addWidget(sessionsGroup, 0, 0, 1, 7);
    sessionsLayout->addWidget(sessionsHealthCheck, 1, 0, 1, 1);
    sessionsLayout->addItem(horizontalSpacer, 1, 1, 1, 1);
    sessionsLayout->addWidget(sessionsLabel1, 1, 2, 1, 1);
    sessionsLayout->addWidget(sessionsCoafSpin, 1, 3, 1, 1);
    sessionsLayout->addWidget(sessionsLabel2, 1, 4, 1, 1);
    sessionsLayout->addWidget(sessionsOffsetSpin, 1, 5, 1, 1);
    sessionsLayout->addWidget(sessionsLabel3, 1, 6, 1, 1);

    sessionsWidget->setLayout(sessionsLayout);


    tasksWidget = new QWidget(this);
    tasksLayout = new QGridLayout(tasksWidget);
    tasksGroup  = new QGroupBox("Columns", tasksWidget);

    QStringList tasksCheckboxLabels = {
        "Task ID", "Task Type", "Agent ID", "Client", "User",
        "Computer", "Start Time", "Finish Time", "Commandline",
        "Result", "Output"
    };

    for (int i = 0; i < 11; ++i)
        tasksCheck[i] = new QCheckBox(tasksCheckboxLabels[i], tasksGroup);

    tasksGroupLayout = new QGridLayout(tasksGroup);
    tasksGroupLayout->addWidget(tasksCheck[0], 0, 0, 1, 1);
    tasksGroupLayout->addWidget(tasksCheck[1], 0, 1, 1, 1);
    tasksGroupLayout->addWidget(tasksCheck[2], 1, 0, 1, 1);
    tasksGroupLayout->addWidget(tasksCheck[3], 1, 1, 1, 1);
    tasksGroupLayout->addWidget(tasksCheck[4], 2, 0, 1, 1);
    tasksGroupLayout->addWidget(tasksCheck[5], 2, 1, 1, 1);
    tasksGroupLayout->addWidget(tasksCheck[6], 3, 0, 1, 1);
    tasksGroupLayout->addWidget(tasksCheck[7], 3, 1, 1, 1);
    tasksGroupLayout->addWidget(tasksCheck[8], 4, 0, 1, 1);
    tasksGroupLayout->addWidget(tasksCheck[9], 4, 1, 1, 1);
    tasksGroupLayout->addWidget(tasksCheck[10], 5, 0, 1, 1);

    tasksGroup->setLayout(tasksGroupLayout);
    tasksLayout->addWidget(tasksGroup, 0, 0, 1, 1);
    tasksWidget->setLayout(tasksLayout);


    listSettings = new QListWidget(this);
    listSettings->setFixedWidth(150);
    listSettings->addItem("Main settings");
    listSettings->addItem("Sessions table");
    listSettings->addItem("Tasks table");
    listSettings->setCurrentRow(0);

    labelHeader = new QLabel(this);
    labelHeader->setStyleSheet("font-size: 16px; font-weight: bold; margin-bottom: 10px;");
    labelHeader->setText("Main settings");

    lineFrame = new QFrame(this);
    lineFrame->setFrameShape(QFrame::HLine);
    lineFrame->setFrameShadow(QFrame::Plain);

    headerLayout = new QVBoxLayout();
    headerLayout->addWidget(labelHeader);
    headerLayout->addWidget(lineFrame);
    headerLayout->setSpacing(0);

    hSpacer     = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    buttonClose = new QPushButton("Close", this);
    buttonClose->setProperty("ButtonStyle", "dialog");

    buttonApply = new QPushButton("Apply ", this);
    buttonApply->setProperty("ButtonStyle", "dialog");
    buttonApply->setEnabled(false);

    stackSettings = new QStackedWidget(this);
    stackSettings->addWidget(mainSettingWidget);
    stackSettings->addWidget(sessionsWidget);
    stackSettings->addWidget(tasksWidget);

    layoutMain = new QGridLayout(this);
    layoutMain->setContentsMargins(4, 4, 4, 4);
    layoutMain->addWidget(listSettings, 0, 0, 2, 1);
    layoutMain->addLayout(headerLayout, 0, 1, 1, 3);
    layoutMain->addWidget(stackSettings, 1, 1, 1, 3);
    layoutMain->addItem(hSpacer, 2, 1, 1, 1);
    layoutMain->addWidget(buttonApply, 2, 2, 1, 1);
    layoutMain->addWidget(buttonClose, 2, 3, 1, 1);

    this->setLayout(layoutMain);
}

void DialogSettings::onStackChange(int index) const
{
    QString text = listSettings->item(index)->text();
    labelHeader->setText(text);
    stackSettings->setCurrentIndex(index);
}

void DialogSettings::onHealthChange() const
{
    buttonApply->setEnabled(true);
    bool active = sessionsHealthCheck->isChecked();
    sessionsLabel1->setEnabled(active);
    sessionsLabel2->setEnabled(active);
    sessionsLabel3->setEnabled(active);
    sessionsCoafSpin->setEnabled(active);
    sessionsOffsetSpin->setEnabled(active);
}

void DialogSettings::onApply() const
{
    buttonApply->setEnabled(false);

    if(settings->data.MainTheme != themeCombo->currentText()) {
        settings->data.MainTheme = themeCombo->currentText();

        QString appTheme = ":/themes/" + settings->data.MainTheme;
        bool result = false;
        QString style = ReadFileString(appTheme, &result);
        if (result) {
            QApplication *app = qobject_cast<QApplication*>(QCoreApplication::instance());
            app->setStyleSheet(style);
        }

        TitleBarStyle::applyForTheme(settings->getMainAdaptix()->mainUI, settings->data.MainTheme);
    }

    if(settings->data.FontSize != fontSizeSpin->value() || settings->data.FontFamily != fontFamilyCombo->currentText()) {
        settings->data.FontSize   = fontSizeSpin->value();
        settings->data.FontFamily = fontFamilyCombo->currentText();

        QString appFontFamily = settings->data.FontFamily;
        if (appFontFamily.startsWith("Adaptix"))
            appFontFamily = appFontFamily.split("-")[1].trimmed();

        auto appFont = QFont(appFontFamily);
        appFont.setPointSize(settings->data.FontSize);
        QApplication::setFont(appFont);
    }

    if (settings->data.GraphVersion != graphCombo1->currentText()) {
        settings->data.GraphVersion = graphCombo1->currentText();
        settings->getMainAdaptix()->mainUI->UpdateGraphIcons();
    }

    settings->data.RemoteTerminalBufferSize = terminalSizeSpin->value();

    settings->data.ConsoleBufferSize = consoleSizeSpin->value();
    settings->data.ConsoleTime = consoleTimeCheckbox->isChecked();
    settings->data.ConsoleNoWrap = consoleNoWrapCheckbox->isChecked();
    settings->data.ConsoleAutoScroll = consoleAutoScrollCheckbox->isChecked();

    bool updateTable = false;
    for ( int i = 0; i < 15; i++) {
        if (settings->data.SessionsTableColumns[i] != sessionsCheck[i]->isChecked()) {
            settings->data.SessionsTableColumns[i] = sessionsCheck[i]->isChecked();
            updateTable = true;
        }
    }
    if (updateTable)
        settings->getMainAdaptix()->mainUI->UpdateSessionsTableColumns();

    settings->data.CheckHealth = sessionsHealthCheck->isChecked();
    settings->data.HealthCoaf = sessionsCoafSpin->value();
    settings->data.HealthOffset = sessionsOffsetSpin->value();

    updateTable = false;
    for ( int i = 0; i < 11; i++) {
        if (settings->data.TasksTableColumns[i] != tasksCheck[i]->isChecked()) {
            settings->data.TasksTableColumns[i] = tasksCheck[i]->isChecked();
            updateTable = true;
        }
    }
    if (updateTable)
        settings->getMainAdaptix()->mainUI->UpdateTasksTableColumns();

    settings->SaveToDB();
}

void DialogSettings::onClose()
{
    this->close();
}
