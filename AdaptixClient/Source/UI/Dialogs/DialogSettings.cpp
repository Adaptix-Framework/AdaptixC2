#include <UI/Dialogs/DialogSettings.h>
#include <Utils/CustomElements.h>

DialogSettings::DialogSettings(Settings* s)
{
    settings = s;

    this->createUI();

    themeCombo->setCurrentText(s->data.MainTheme);
    fontFamilyCombo->setCurrentText(s->data.FontFamily);
    fontSizeSpin->setValue(s->data.FontSize);
    consoleTimeCheckbox->setChecked(s->data.ConsoleTime);

    for ( int i = 0; i < 15; i++)
        sessionsCheck[i]->setChecked(s->data.SessionsTableColumns[i]);

    connect(themeCombo,          &QComboBox::currentTextChanged, buttonApply, [=, this](const QString &text){buttonApply->setEnabled(true);} );
    connect(fontFamilyCombo,     &QComboBox::currentTextChanged, buttonApply, [=, this](const QString &text){buttonApply->setEnabled(true);} );
    connect(fontSizeSpin,        &QSpinBox::valueChanged,        buttonApply, [=, this](int ){buttonApply->setEnabled(true);} );
    connect(consoleTimeCheckbox, &QCheckBox::stateChanged,       buttonApply, [=, this](int ){buttonApply->setEnabled(true);} );

    for ( int i = 0; i < 15; i++)
        connect(sessionsCheck[i],  &QCheckBox::stateChanged, buttonApply, [=, this](int ){buttonApply->setEnabled(true);} );

    connect(listSettings, &QListWidget::currentRowChanged, this, &DialogSettings::onStackChange);
    connect(buttonApply, &QPushButton::clicked, this, &DialogSettings::onApply);
    connect(buttonClose, &QPushButton::clicked, this, &DialogSettings::onClose);
}

void DialogSettings::createUI()
{
    this->setWindowTitle("Adaptix Settings");
    this->resize(600, 300);

    /////////////// Main setting

    mainSettingWidget = new QWidget(this);
    mainSettingLayout = new QGridLayout(mainSettingWidget);

    themeLabel = new QLabel(mainSettingWidget);
    themeLabel->setText("Main theme: ");

    themeCombo = new QComboBox(mainSettingWidget);
    themeCombo->addItem("Light_Arc");
    themeCombo->addItem("Dark");

    fontSizeLabel = new QLabel(mainSettingWidget);
    fontSizeLabel->setText("Font size: ");

    fontSizeSpin = new QSpinBox(mainSettingWidget);
    fontSizeSpin->setMinimum(7);
    fontSizeSpin->setMaximum(30);

    fontFamilyLabel = new QLabel(mainSettingWidget);
    fontFamilyLabel->setText("Font family: ");

    fontFamilyCombo = new QComboBox(mainSettingWidget);
    fontFamilyCombo->addItem("Adaptix - DejaVu Sans Mono");
    fontFamilyCombo->addItem("Adaptix - Droid Sans Mono");
    fontFamilyCombo->addItem("Adaptix - Hack");

    consoleTimeCheckbox = new QCheckBox("Print date and time in agent console", mainSettingWidget);

    mainSettingLayout->addWidget(themeLabel, 0, 0, 1, 1);
    mainSettingLayout->addWidget(themeCombo, 0, 1, 1, 1);
    mainSettingLayout->addWidget(fontFamilyLabel, 1,0, 1, 1);
    mainSettingLayout->addWidget(fontFamilyCombo, 1, 1, 1, 1);
    mainSettingLayout->addWidget(fontSizeLabel, 2, 0, 1, 1);
    mainSettingLayout->addWidget(fontSizeSpin, 2, 1, 1, 1);
    mainSettingLayout->addWidget(consoleTimeCheckbox, 3, 0, 1, 2);

    mainSettingWidget->setLayout(mainSettingLayout);

    /////////////// Sessions Table

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
    sessionsLayout->addWidget(sessionsGroup, 0, 0, 1, 1);
    sessionsWidget->setLayout(sessionsLayout);

    //////////////

    listSettings = new QListWidget(this);
    listSettings->setFixedWidth(150);
    listSettings->addItem("Main settings");
    listSettings->addItem("Sessions table");
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
    buttonApply = new QPushButton("Apply ", this);
    buttonApply->setEnabled(false);

    stackSettings = new QStackedWidget(this);
    stackSettings->addWidget(mainSettingWidget);
    stackSettings->addWidget(sessionsWidget);

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

void DialogSettings::onApply() const
{
    buttonApply->setEnabled(false);

    settings->data.MainTheme   = themeCombo->currentText();
    settings->data.FontSize    = fontSizeSpin->value();
    settings->data.FontFamily  = fontFamilyCombo->currentText();
    settings->data.ConsoleTime = consoleTimeCheckbox->isChecked();

    for ( int i = 0; i < 15; i++)
        settings->data.SessionsTableColumns[i] = sessionsCheck[i]->isChecked();

    settings->SaveToDB();

    MessageSuccess("Settings saved. Please restart Adaptix.");
}

void DialogSettings::onClose()
{
    this->close();
}
