#include <UI/MainUI.h>
#include <UI/Dialogs/DialogSettings.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <MainAdaptix.h>
#include <Client/Settings.h>
#include <Client/ConsoleTheme.h>
#include <Utils/TitleBarStyle.h>
#include <QShowEvent>
#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <oclero/qlementine.hpp>

DialogSettings::DialogSettings(Settings* s)
{
    settings = s;

    this->createUI();

    connect(themeCombo,         &QComboBox::currentTextChanged, buttonApply, [this](auto&&){buttonApply->setEnabled(true);} );
    connect(fontFamilyCombo,    &QComboBox::currentTextChanged, buttonApply, [this](auto&&){buttonApply->setEnabled(true);} );
    connect(fontSizeSpin,       &QSpinBox::valueChanged,        buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(sessionsCoafSpin,   &QDoubleSpinBox::valueChanged,  buttonApply, [this](double){buttonApply->setEnabled(true);} );
    connect(sessionsOffsetSpin, &QSpinBox::valueChanged,        buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(sessionsDeadShiftSpin, &QDoubleSpinBox::valueChanged, buttonApply, [this](double){buttonApply->setEnabled(true);} );
    connect(terminalSizeSpin,   &QSpinBox::valueChanged,        buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(consoleSizeSpin,    &QSpinBox::valueChanged,        buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(consolePageSizeSpin, &QSpinBox::valueChanged,       buttonApply, [this](int){buttonApply->setEnabled(true);} );
    connect(consoleThemeCombo, &QComboBox::currentTextChanged, buttonApply, [this](const QString &){buttonApply->setEnabled(true);} );

    connect(consoleTimeCheckbox,           &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);} );
    connect(consoleNoWrapCheckbox,         &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);} );
    connect(consoleAutoScrollCheckbox,     &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);} );
    connect(consoleAutoLoadEarlierCheckbox, &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);} );
    connect(consoleShowBackgroundCheckbox, &oclero::qlementine::Switch::toggled, this, [this](bool checked) {
        buttonApply->setEnabled(true);
        consoleBgDimmingLabel->setEnabled(checked);
        consoleBgDimmingSlider->setEnabled(checked);
        consoleBgDimmingValueLabel->setEnabled(checked);
    });
    connect(consoleUseAppThemeCheckbox,   &oclero::qlementine::Switch::toggled, this, &DialogSettings::onUseAppThemeChange );
    connect(sessionsHealthCheck,           &oclero::qlementine::Switch::toggled, this, &DialogSettings::onHealthChange );

    for ( int i = 0; i < sessionsCheckCount; i++)
        connect(sessionsCheck[i],  &QCheckBox::checkStateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );

    connect(graphCombo1, &QComboBox::currentTextChanged, buttonApply, [this](auto&&){buttonApply->setEnabled(true);} );

    connect(tabblinkEnabledCheckbox, &oclero::qlementine::Switch::toggled, this, &DialogSettings::onBlinkChange );

    for (auto* check : m_tabblinkChecks)
        connect(check, &QCheckBox::checkStateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );


    for ( int i = 0; i < tasksCheckCount; i++)
        connect(tasksCheck[i],  &QCheckBox::checkStateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );

    for ( int i = 0; i < targetsCheckCount; i++)
        connect(targetsCheck[i], &QCheckBox::checkStateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );

    for ( int i = 0; i < credsCheckCount; i++)
        connect(credsCheck[i], &QCheckBox::checkStateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );

    for ( int i = 0; i < filesCheckCount; i++)
        connect(filesCheck[i], &QCheckBox::checkStateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );

    for (int i = 0; i < 4; ++i) {
        connect(toolbarPosBtn[i], &QPushButton::toggled, buttonApply, [this](bool){ buttonApply->setEnabled(true); });
    }

    connect(sessionsAutoHideInactiveSwitch, &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(sessionsCompactSwitch,          &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(tasksInProcessSwitch,           &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(tasksCompactSwitch,             &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(targetsCompactSwitch,           &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(credsCompactSwitch,             &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(filesCompactSwitch,             &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(graphAutoHideInactiveSwitch,    &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(graphAutoHideNoChildsSwitch,    &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});

    connect(listSettings, &QListWidget::currentRowChanged, this, &DialogSettings::onStackChange);
    connect(buttonApply,  &QPushButton::clicked,           this, &DialogSettings::onApply);
    connect(buttonClose,  &QPushButton::clicked,           this, &DialogSettings::onClose);
}

void DialogSettings::createUI()
{
    this->setWindowTitle("Adaptix Settings");
    this->resize(600, 300);
    this->setProperty("Main", "base");

    appearanceWidget = new QWidget(this);
    appearanceLayout = new QGridLayout(appearanceWidget);

    themeLabel = new QLabel("Main theme: ", appearanceWidget);
    themeCombo = new QComboBox(appearanceWidget);
    refreshAppThemeCombo();
    themeImportBtn = new QPushButton("Import", appearanceWidget);
    themeImportBtn->setFixedWidth(80);
    connect(themeImportBtn, &QPushButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Import Application Theme", QString(), "JSON files (*.json)");
        if (filePath.isEmpty()) return;
        if (importAppTheme(filePath)) {
            QString name = QFileInfo(filePath).baseName();
            refreshAppThemeCombo();
            themeCombo->setCurrentText(name);
            buttonApply->setEnabled(true);
        }
    });

    themeDeleteBtn = new QPushButton("Delete", appearanceWidget);
    themeDeleteBtn->setFixedWidth(80);
    connect(themeDeleteBtn, &QPushButton::clicked, this, [this]() {
        QString name = themeCombo->currentText();
        if (name.isEmpty()) return;
        QString userPath = userAppThemeDir() + "/" + name + ".json";
        if (!QFile::exists(userPath)) {
            QMessageBox::warning(this, "Delete Theme", "Built-in themes cannot be deleted.");
            return;
        }
        auto reply = QMessageBox::question(this, "Delete Theme", QString("Delete theme '%1'?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        if (deleteAppTheme(name)) {
            refreshAppThemeCombo();
            buttonApply->setEnabled(true);
        }
    });

    themeSwatchesFrame = new QFrame(appearanceWidget);
    themeSwatchesFrame->setObjectName("ThemeSwatchesFrame");
    themeSwatchesFrame->setStyleSheet("QFrame#ThemeSwatchesFrame { background: transparent; }");
    themeSwatchesLayout = new QHBoxLayout(themeSwatchesFrame);
    themeSwatchesLayout->setContentsMargins(0, 4, 0, 0);
    themeSwatchesLayout->setSpacing(6);
    QStringList swatchLabels = {"Primary", "Secondary", "Success", "Error", "Border"};
    QStringList swatchColors = {"#238636", "#404040", "#2bb5a0", "#e96b72", "#d3d3d3"};
    for (int i = 0; i < 5; ++i) {
        auto* swatch = new QLabel(themeSwatchesFrame);
        swatch->setFixedSize(20, 20);
        swatch->setStyleSheet(QString("background: %1; border-radius: 10px; border: 1px solid rgba(255,255,255,0.1);").arg(swatchColors[i]));
        swatch->setToolTip(swatchLabels[i]);
        themeSwatchesLayout->addWidget(swatch);
        themeSwatchLabels[i] = swatch;
    }
    themeSwatchesLayout->addStretch();
    connect(themeCombo, &QComboBox::currentTextChanged, this, &DialogSettings::updateThemeSwatches);

    fontSizeLabel = new QLabel("Font size: ", appearanceWidget);
    fontSizeSpin  = new QSpinBox(appearanceWidget);
    fontSizeSpin->setMinimum(7);
    fontSizeSpin->setMaximum(30);

    fontFamilyLabel = new QLabel("Font family: ", appearanceWidget);
    fontFamilyCombo = new QComboBox(appearanceWidget);
    fontFamilyCombo->addItem("Adaptix - JetBrains Mono");
    fontFamilyCombo->addItem("Adaptix - Hack");
    fontFamilyCombo->addItem("Qlementine - Inter");
    fontFamilyCombo->addItem("Qlementine - Roboto Mono");

    graphLabel1 = new QLabel("Session Graph version:", sessionsWidget);
    graphCombo1 = new QComboBox(sessionsWidget);
    graphCombo1->addItem("Version 1");
    graphCombo1->addItem("Version 2");
    graphCombo1->addItem("Version 3");

    consolePageWidget = new QWidget(this);
    consolePageLayout = new QGridLayout(consolePageWidget);

    terminalSizeLabel = new QLabel("RemoteTerminal buffer (lines):", consolePageWidget);
    terminalSizeSpin  = new QSpinBox(consolePageWidget);
    terminalSizeSpin->setMinimum(1);
    terminalSizeSpin->setMaximum(100000);

    toolbarPosLabel = new QLabel("Toolbar position:", appearanceWidget);
    toolbarPosFrame = new QFrame(appearanceWidget);
    toolbarPosFrame->setStyleSheet(QStringLiteral(
        "QFrame#ToolbarPosFrame { background: palette(mid); border: 1px solid palette(mid); border-radius: 4px; }"
        "QPushButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 3px;"
        "  background: palette(window);"
        "  padding: 14px;"
        "}"
        "QPushButton:hover { background: palette(dark); }"
        "QPushButton:checked {"
        "  background: palette(highlight);"
        "  border-color: palette(highlight);"
        "}"
    ));
    toolbarPosFrame->setObjectName("ToolbarPosFrame");
    toolbarPosGrid = new QGridLayout(toolbarPosFrame);
    toolbarPosGrid->setContentsMargins(4, 4, 4, 4);
    toolbarPosGrid->setSpacing(3);
    const QStringList posLabels = { QStringLiteral("Top"), QStringLiteral("Bottom"), QStringLiteral("Left"), QStringLiteral("Right") };
    for (int i = 0; i < 4; ++i) {
        toolbarPosBtn[i] = new QPushButton(posLabels[i], toolbarPosFrame);
        toolbarPosBtn[i]->setCheckable(true);
        toolbarPosBtn[i]->setAutoExclusive(true);
        toolbarPosBtn[i]->setMinimumWidth(70);
        toolbarPosBtn[i]->setToolTip(QStringLiteral("Place the toolbar on the %1 edge. Takes effect on next project open.").arg(posLabels[i].toLower()));
    }
    toolbarPosGrid->addWidget(toolbarPosBtn[0], 0, 1);
    toolbarPosGrid->addWidget(toolbarPosBtn[1], 2, 1);
    toolbarPosGrid->addWidget(toolbarPosBtn[2], 1, 0);
    toolbarPosGrid->addWidget(toolbarPosBtn[3], 1, 2);

    consoleThemeGroup = new QGroupBox("Console Theme", consolePageWidget);

    consoleUseAppThemeCheckbox = new oclero::qlementine::Switch(consoleThemeGroup);
    consoleUseAppThemeCheckbox->setText("Use application theme");
    consoleUseAppThemeCheckbox->setToolTip("Derive console colors from the active Qlementine application theme instead of a separate console theme.");

    consoleShowBackgroundCheckbox = new oclero::qlementine::Switch(consoleThemeGroup);
    consoleShowBackgroundCheckbox->setText("Show background image");

    consoleBgImageLabel = new QLabel("Background image:", consoleThemeGroup);
    consoleBgImagePathEdit = new QLineEdit(consoleThemeGroup);
    consoleBgImagePathEdit->setPlaceholderText("Default background image");
    consoleBgImagePathEdit->setReadOnly(true);
    consoleBgImageBrowseBtn = new QPushButton("Browse", consoleThemeGroup);
    consoleBgImageBrowseBtn->setFixedWidth(80);
    connect(consoleBgImageBrowseBtn, &QPushButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Select Background Image", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.gif)");
        if (filePath.isEmpty())
            return;

        consoleBgImagePathEdit->setText(filePath);
        consoleBgDimmingLabel->setEnabled(true);
        consoleBgDimmingSlider->setEnabled(true);
        consoleBgDimmingValueLabel->setEnabled(true);
        consoleBgPreviewLabel->setPixmap(QPixmap(filePath));
        buttonApply->setEnabled(true);
    });
    consoleBgImageClearBtn = new QPushButton("Clear", consoleThemeGroup);
    consoleBgImageClearBtn->setFixedWidth(80);
    connect(consoleBgImageClearBtn, &QPushButton::clicked, this, [this]() {
        consoleBgImagePathEdit->setText(":/Back (Default)");
        consoleBgPreviewLabel->setPixmap(QPixmap(":/Back"));
        consoleBgImageClearBtn->setEnabled(false);
        buttonApply->setEnabled(true);
    });

    consoleBgDimmingLabel = new QLabel("Dimming:", consoleThemeGroup);
    consoleBgDimmingSlider = new QSlider(Qt::Horizontal, consoleThemeGroup);
    consoleBgDimmingSlider->setMinimum(0);
    consoleBgDimmingSlider->setMaximum(100);
    consoleBgDimmingSlider->setValue(80);
    consoleBgDimmingValueLabel = new QLabel("80%", consoleThemeGroup);
    consoleBgDimmingValueLabel->setFixedWidth(36);
    connect(consoleBgDimmingSlider, &QSlider::valueChanged, this, [this](int val) {
        consoleBgDimmingValueLabel->setText(QString("%1%").arg(val));
        buttonApply->setEnabled(true);
    });

    consoleThemeLabel = new QLabel("Console theme:", consoleThemeGroup);
    consoleThemeCombo = new QComboBox(consoleThemeGroup);
    refreshConsoleThemeCombo();
    consoleThemeImportBtn = new QPushButton("Import", consoleThemeGroup);
    consoleThemeImportBtn->setFixedWidth(80);
    connect(consoleThemeImportBtn, &QPushButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(this, "Import Console Theme", QString(), "JSON files (*.json)");
        if (filePath.isEmpty()) return;
        if (ConsoleThemeManager::instance().importTheme(filePath)) {
            QString name = QFileInfo(filePath).baseName();
            refreshConsoleThemeCombo();
            consoleThemeCombo->setCurrentText(name);
            buttonApply->setEnabled(true);
        }
    });

    consoleThemeDeleteBtn = new QPushButton("Delete", consoleThemeGroup);
    consoleThemeDeleteBtn->setFixedWidth(80);
    connect(consoleThemeDeleteBtn, &QPushButton::clicked, this, [this]() {
        QString name = consoleThemeCombo->currentText();
        if (name.isEmpty()) return;
        QString userPath = ConsoleThemeManager::userThemeDir() + "/" + name + ".json";
        if (!QFile::exists(userPath)) {
            QMessageBox::warning(this, "Delete Theme", "Built-in themes cannot be deleted.");
            return;
        }
        auto reply = QMessageBox::question(this, "Delete Theme", QString("Delete theme '%1'?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        if (ConsoleThemeManager::instance().deleteTheme(name)) {
            refreshConsoleThemeCombo();
            buttonApply->setEnabled(true);
        }
    });

    auto* consoleThemeButtons = new QHBoxLayout();
    consoleThemeButtons->setContentsMargins(0, 0, 0, 0);
    consoleThemeButtons->setSpacing(4);
    consoleThemeButtons->addWidget(consoleThemeImportBtn);
    consoleThemeButtons->addWidget(consoleThemeDeleteBtn);

    auto* consoleBgButtons = new QHBoxLayout();
    consoleBgButtons->setContentsMargins(0, 0, 0, 0);
    consoleBgButtons->setSpacing(4);
    consoleBgButtons->addWidget(consoleBgImageBrowseBtn);
    consoleBgButtons->addWidget(consoleBgImageClearBtn);

    auto* consoleDimmingLayout = new QHBoxLayout();
    consoleDimmingLayout->setContentsMargins(0, 0, 0, 0);
    consoleDimmingLayout->setSpacing(4);
    consoleDimmingLayout->addWidget(consoleBgDimmingSlider, 1);
    consoleDimmingLayout->addWidget(consoleBgDimmingValueLabel, 0);

    consoleBgPreviewFrame = new QFrame(consoleThemeGroup);
    consoleBgPreviewFrame->setObjectName("BgPreviewFrame");
    consoleBgPreviewFrame->setStyleSheet("QFrame#BgPreviewFrame { background: palette(base); border: 1px solid palette(mid); border-radius: 6px; }");
    consoleBgPreviewFrame->setMinimumHeight(100);
    consoleBgPreviewFrame->setMaximumHeight(140);
    auto* previewLayout = new QVBoxLayout(consoleBgPreviewFrame);
    previewLayout->setContentsMargins(4, 4, 4, 4);
    consoleBgPreviewLabel = new QLabel(consoleBgPreviewFrame);
    consoleBgPreviewLabel->setAlignment(Qt::AlignCenter);
    consoleBgPreviewLabel->setMinimumSize(200, 80);
    consoleBgPreviewLabel->setStyleSheet("color: palette(placeholderText); font-size: 11px;");
    consoleBgPreviewLabel->setText("No background image");
    consoleBgPreviewLabel->setScaledContents(true);
    previewLayout->addWidget(consoleBgPreviewLabel);

    consoleThemeGroupLayout = new QGridLayout(consoleThemeGroup);
    consoleThemeGroupLayout->addWidget(consoleUseAppThemeCheckbox, 0, 0, 1, 3);
    consoleThemeGroupLayout->addWidget(consoleThemeLabel,          1, 0, 1, 1);
    consoleThemeGroupLayout->addWidget(consoleThemeCombo,          1, 1, 1, 1);
    consoleThemeGroupLayout->addLayout(consoleThemeButtons,        1, 2, 1, 1);
    consoleThemeGroupLayout->addWidget(consoleShowBackgroundCheckbox, 2, 0, 1, 3);
    consoleThemeGroupLayout->addWidget(consoleBgImageLabel,        3, 0, 1, 1);
    consoleThemeGroupLayout->addWidget(consoleBgImagePathEdit,     3, 1, 1, 1);
    consoleThemeGroupLayout->addLayout(consoleBgButtons,           3, 2, 1, 1);
    consoleThemeGroupLayout->addWidget(consoleBgDimmingLabel,      4, 0, 1, 1);
    consoleThemeGroupLayout->addLayout(consoleDimmingLayout,       4, 1, 1, 2);
    consoleThemeGroupLayout->addWidget(consoleBgPreviewFrame,      5, 0, 1, 3);
    consoleThemeGroup->setLayout(consoleThemeGroupLayout);

    consoleBehaviorGroup = new QGroupBox("Console Behavior", consolePageWidget);

    consoleSizeLabel = new QLabel("Buffer size (lines):", consoleBehaviorGroup);
    consoleSizeSpin  = new QSpinBox(consoleBehaviorGroup);
    consoleSizeSpin->setMinimum(10000);
    consoleSizeSpin->setMaximum(1000000);

    consoleTimeCheckbox       = new oclero::qlementine::Switch(consoleBehaviorGroup);
    consoleTimeCheckbox->setText("Print date and time");
    consoleNoWrapCheckbox     = new oclero::qlementine::Switch(consoleBehaviorGroup);
    consoleNoWrapCheckbox->setText("No Wrap mode");
    consoleAutoScrollCheckbox = new oclero::qlementine::Switch(consoleBehaviorGroup);
    consoleAutoScrollCheckbox->setText("Auto Scroll mode");
    consoleAutoLoadEarlierCheckbox = new oclero::qlementine::Switch(consoleBehaviorGroup);
    consoleAutoLoadEarlierCheckbox->setText("Auto load earlier history");
    consoleAutoLoadEarlierCheckbox->setToolTip("When scrolling to the top of the console, automatically load older history pages");

    consolePageSizeLabel = new QLabel("History page size:", consoleBehaviorGroup);
    consolePageSizeSpin  = new QSpinBox(consoleBehaviorGroup);
    consolePageSizeSpin->setMinimum(10);
    consolePageSizeSpin->setMaximum(2000);
    consolePageSizeSpin->setSingleStep(10);
    consolePageSizeSpin->setToolTip("Number of history items loaded per page (Load earlier / initial load)");

    consoleBehaviorGroupLayout = new QGridLayout(consoleBehaviorGroup);
    consoleBehaviorGroupLayout->addWidget(consoleSizeLabel,               0, 0, 1, 1);
    consoleBehaviorGroupLayout->addWidget(consoleSizeSpin,                0, 1, 1, 2);
    consoleBehaviorGroupLayout->addWidget(consolePageSizeLabel,           1, 0, 1, 1);
    consoleBehaviorGroupLayout->addWidget(consolePageSizeSpin,            1, 1, 1, 2);
    consoleBehaviorGroupLayout->addWidget(consoleTimeCheckbox,            2, 0, 1, 3);
    consoleBehaviorGroupLayout->addWidget(consoleNoWrapCheckbox,          3, 0, 1, 3);
    consoleBehaviorGroupLayout->addWidget(consoleAutoScrollCheckbox,      4, 0, 1, 3);
    consoleBehaviorGroupLayout->addWidget(consoleAutoLoadEarlierCheckbox, 5, 0, 1, 3);
    consoleBehaviorGroup->setLayout(consoleBehaviorGroupLayout);

    auto* themeButtons = new QHBoxLayout();
    themeButtons->setContentsMargins(0, 0, 0, 0);
    themeButtons->setSpacing(4);
    themeButtons->addWidget(themeImportBtn);
    themeButtons->addWidget(themeDeleteBtn);

    appearanceLayout->addWidget(themeLabel,         0, 0, 1, 1);
    appearanceLayout->addWidget(themeCombo,         0, 1, 1, 1);
    appearanceLayout->addLayout(themeButtons,       0, 2, 1, 1);
    appearanceLayout->addWidget(themeSwatchesFrame, 1, 0, 1, 3);
    appearanceLayout->addWidget(fontFamilyLabel,    2, 0, 1, 1);
    appearanceLayout->addWidget(fontFamilyCombo,    2, 1, 1, 2);
    appearanceLayout->addWidget(fontSizeLabel,      3, 0, 1, 1);
    appearanceLayout->addWidget(fontSizeSpin,       3, 1, 1, 2);
    appearanceLayout->addWidget(toolbarPosLabel,    4, 0, 1, 1);
    appearanceLayout->addWidget(toolbarPosFrame,    4, 1, 1, 2, Qt::AlignLeft);
    appearanceLayout->setRowStretch(5, 1);
    appearanceWidget->setLayout(appearanceLayout);

    consolePageLayout->addWidget(consoleThemeGroup,    0, 0, 1, 3);
    consolePageLayout->addWidget(consoleBehaviorGroup, 1, 0, 1, 3);
    consolePageLayout->addWidget(terminalSizeLabel,    2, 0, 1, 1);
    consolePageLayout->addWidget(terminalSizeSpin,     2, 1, 1, 2);
    consolePageLayout->setRowStretch(3, 1);
    consolePageWidget->setLayout(consolePageLayout);

    sessionsWidget = new QWidget(this);
    sessionsLayout = new QGridLayout(sessionsWidget);

    sessionsAutoHideInactiveSwitch = new oclero::qlementine::Switch(sessionsWidget);
    sessionsAutoHideInactiveSwitch->setText("Auto hide inactive");
    sessionsAutoHideInactiveSwitch->setToolTip("When enabled, sessions with status Terminated/Inactive/Disconnect are hidden by default.");
    sessionsCompactSwitch = new oclero::qlementine::Switch(sessionsWidget);
    sessionsCompactSwitch->setText("Compact mode");
    sessionsCompactSwitch->setToolTip("Single-line feed rows for Sessions.");
    auto* sessionsPrefsRow = new QHBoxLayout();
    sessionsPrefsRow->setContentsMargins(0, 0, 0, 0);
    sessionsPrefsRow->setSpacing(16);
    sessionsPrefsRow->addWidget(sessionsAutoHideInactiveSwitch);
    sessionsPrefsRow->addWidget(sessionsCompactSwitch);
    sessionsPrefsRow->addStretch();
    sessionsLayout->addLayout(sessionsPrefsRow, 0, 0, 1, 1);

    sessionsGroup = new QGroupBox("Visible fields", sessionsWidget);
    sessionsGroup->setToolTip("Fields map to feed card blocks.");

    QStringList sessionsCheckboxLabels = {
        "Agent ID", "Agent Type", "External", "Listener", "Internal",
        "Domain", "Computer", "User", "OS", "Process",
        "PID", "Icon", "Tags", "Created", "Last", "Sleep"
    };

    for (int i = 0; i < sessionsCheckCount; ++i)
        sessionsCheck[i] = new QCheckBox(sessionsCheckboxLabels[i], sessionsGroup);

    sessionsGroupLayout = new QVBoxLayout(sessionsGroup);
    sessionsGroupLayout->setSpacing(8);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    auto* identityGroup = new QGroupBox("Identity", sessionsGroup);
    auto* identityLayout = new QVBoxLayout(identityGroup);
    identityLayout->setContentsMargins(8, 8, 8, 8);
    identityLayout->addWidget(sessionsCheck[11]); // Icon
    identityLayout->addWidget(sessionsCheck[0]);  // Agent ID
    identityLayout->addWidget(sessionsCheck[1]);  // Agent Type
    identityLayout->addWidget(sessionsCheck[13]); // Created
    identityLayout->addStretch();
    topRow->addWidget(identityGroup, 1);

    auto* mainGroup = new QGroupBox("Main", sessionsGroup);
    auto* mainLayout = new QVBoxLayout(mainGroup);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->addWidget(sessionsCheck[7]); // User
    mainLayout->addWidget(sessionsCheck[6]); // Computer
    mainLayout->addWidget(sessionsCheck[5]); // Domain
    mainLayout->addWidget(sessionsCheck[4]); // Internal
    mainLayout->addStretch();
    topRow->addWidget(mainGroup, 1);

    auto* statusGroup = new QGroupBox("Status", sessionsGroup);
    auto* statusLayout = new QVBoxLayout(statusGroup);
    statusLayout->setContentsMargins(8, 8, 8, 8);
    statusLayout->addWidget(sessionsCheck[14]); // Last
    statusLayout->addWidget(sessionsCheck[15]); // Sleep
    statusLayout->addStretch();
    topRow->addWidget(statusGroup, 1);

    sessionsGroupLayout->addLayout(topRow);

    auto* detailsGroup = new QGroupBox("Details", sessionsGroup);
    auto* detailsLayout = new QGridLayout(detailsGroup);
    detailsLayout->setContentsMargins(8, 8, 8, 8);
    detailsLayout->addWidget(sessionsCheck[3], 0, 0); // Listener
    detailsLayout->addWidget(sessionsCheck[2], 0, 1); // External
    detailsLayout->addWidget(sessionsCheck[8], 0, 2); // OS
    detailsLayout->addWidget(sessionsCheck[9], 1, 0); // Process
    detailsLayout->addWidget(sessionsCheck[10], 1, 1); // PID
    sessionsGroupLayout->addWidget(detailsGroup);

    auto* tagsGroup = new QGroupBox("Tags", sessionsGroup);
    auto* tagsLayout = new QHBoxLayout(tagsGroup);
    tagsLayout->setContentsMargins(8, 8, 8, 8);
    tagsLayout->addWidget(sessionsCheck[12]); // Tags
    tagsLayout->addStretch();
    sessionsGroupLayout->addWidget(tagsGroup);

    sessionsGroup->setLayout(sessionsGroupLayout);

    auto* healthGroup = new QGroupBox("Health Check", sessionsWidget);
    auto* healthGroupLayout = new QGridLayout(healthGroup);

    sessionsHealthCheck = new oclero::qlementine::Switch(healthGroup);
    sessionsHealthCheck->setText("Enable health monitoring");
    sessionsHealthCheck->setToolTip("Monitor agent liveness based on expected sleep intervals.");

    sessionsLabel1 = new QLabel("Threshold:", healthGroup);
    sessionsLabel1->setStyleSheet("font-weight: 500;");

    sessionsCoafSpin = new QDoubleSpinBox(healthGroup);
    sessionsCoafSpin->setMinimum(1.0);
    sessionsCoafSpin->setMaximum(5.0);
    sessionsCoafSpin->setSingleStep(0.1);
    sessionsCoafSpin->setPrefix("Sleep × ");

    sessionsLabel2 = new QLabel("+", healthGroup);
    sessionsLabel2->setAlignment(Qt::AlignCenter);

    sessionsOffsetSpin = new QSpinBox(healthGroup);
    sessionsOffsetSpin->setMinimum(1);
    sessionsOffsetSpin->setMaximum(10000);
    sessionsOffsetSpin->setSuffix(" sec");

    sessionsLabel3 = new QLabel("", healthGroup);

    auto* sessionsDeadShiftLabel = new QLabel("Dead row shift:", healthGroup);
    sessionsDeadShiftLabel->setStyleSheet("font-weight: 500;");
    sessionsDeadShiftSpin = new QDoubleSpinBox(healthGroup);
    sessionsDeadShiftSpin->setMinimum(0.0);
    sessionsDeadShiftSpin->setMaximum(0.5);
    sessionsDeadShiftSpin->setSingleStep(0.01);
    sessionsDeadShiftSpin->setDecimals(2);
    sessionsDeadShiftSpin->setToolTip("Lightness shift for inactive rows. Light: darker, Dark: lighter.");

    healthGroupLayout->addWidget(sessionsHealthCheck,      0, 0, 1, 6);
    healthGroupLayout->addWidget(sessionsLabel1,           1, 0, 1, 1);
    healthGroupLayout->addWidget(sessionsCoafSpin,        1, 1, 1, 1);
    healthGroupLayout->addWidget(sessionsLabel2,          1, 2, 1, 1, Qt::AlignCenter);
    healthGroupLayout->addWidget(sessionsOffsetSpin,      1, 3, 1, 1);
    healthGroupLayout->addWidget(sessionsDeadShiftLabel,  2, 0, 1, 1);
    healthGroupLayout->addWidget(sessionsDeadShiftSpin,   2, 1, 1, 1);
    healthGroup->setLayout(healthGroupLayout);

    auto* graphGroup = new QGroupBox("Session Graph", sessionsWidget);
    auto* graphGroupLayout = new QGridLayout(graphGroup);
    graphGroupLayout->addWidget(graphLabel1, 0, 0, 1, 1);
    graphGroupLayout->addWidget(graphCombo1, 0, 1, 1, 1);

    graphAutoHideInactiveSwitch = new oclero::qlementine::Switch(graphGroup);
    graphAutoHideInactiveSwitch->setText("Auto hide inactive");
    graphAutoHideInactiveSwitch->setToolTip("Hide inactive/terminated sessions from the graph by default.");

    graphAutoHideNoChildsSwitch = new oclero::qlementine::Switch(graphGroup);
    graphAutoHideNoChildsSwitch->setText("Auto hide without children");
    graphAutoHideNoChildsSwitch->setToolTip("Hide sessions that have no child sessions in the graph by default.");

    graphGroupLayout->addWidget(graphAutoHideInactiveSwitch, 1, 0, 1, 2);
    graphGroupLayout->addWidget(graphAutoHideNoChildsSwitch, 2, 0, 1, 2);
    graphGroup->setLayout(graphGroupLayout);

    sessionsLayout->addWidget(sessionsGroup,  1, 0, 1, 1);
    sessionsLayout->addWidget(healthGroup,    2, 0, 1, 1);
    sessionsLayout->addWidget(graphGroup,     3, 0, 1, 1);
    sessionsLayout->setRowStretch(4, 1);

    sessionsWidget->setLayout(sessionsLayout);


    tasksWidget = new QWidget(this);
    tasksLayout = new QGridLayout(tasksWidget);

    tasksInProcessSwitch = new oclero::qlementine::Switch(tasksWidget);
    tasksInProcessSwitch->setText("In process only");
    tasksInProcessSwitch->setToolTip("Show only incomplete tasks (Hosted / Running).");
    tasksCompactSwitch = new oclero::qlementine::Switch(tasksWidget);
    tasksCompactSwitch->setText("Compact mode");
    tasksCompactSwitch->setToolTip("Single-line feed rows for Tasks.");
    auto* tasksPrefsRow = new QHBoxLayout();
    tasksPrefsRow->setContentsMargins(0, 0, 0, 0);
    tasksPrefsRow->setSpacing(16);
    tasksPrefsRow->addWidget(tasksInProcessSwitch);
    tasksPrefsRow->addWidget(tasksCompactSwitch);
    tasksPrefsRow->addStretch();
    tasksLayout->addLayout(tasksPrefsRow, 0, 0, 1, 1);

    tasksGroup = new QGroupBox("Visible fields", tasksWidget);
    tasksGroup->setToolTip("Fields map to feed card blocks.");

    QStringList tasksCheckboxLabels = {
        "Task ID", "Task Type", "Agent ID", "Client", "User",
        "Computer", "Start Time", "Finish Time", "Commandline",
        "Result"
    };

    for (int i = 0; i < tasksCheckCount; ++i)
        tasksCheck[i] = new QCheckBox(tasksCheckboxLabels[i], tasksGroup);

    tasksGroupLayout = new QVBoxLayout(tasksGroup);
    tasksGroupLayout->setSpacing(8);

    auto* tasksTopRow = new QHBoxLayout();
    tasksTopRow->setSpacing(8);

    auto* tasksIdentityGroup = new QGroupBox("Identity", tasksGroup);
    auto* tasksIdentityLayout = new QVBoxLayout(tasksIdentityGroup);
    tasksIdentityLayout->setContentsMargins(8, 8, 8, 8);
    tasksIdentityLayout->addWidget(tasksCheck[0]); // Task ID
    tasksIdentityLayout->addWidget(tasksCheck[1]); // Task Type
    tasksIdentityLayout->addWidget(tasksCheck[6]); // Start Time
    tasksIdentityLayout->addStretch();
    tasksTopRow->addWidget(tasksIdentityGroup);

    auto* tasksMainGroup = new QGroupBox("Main", tasksGroup);
    auto* tasksMainLayout = new QVBoxLayout(tasksMainGroup);
    tasksMainLayout->setContentsMargins(8, 8, 8, 8);
    tasksMainLayout->addWidget(tasksCheck[8]); // Commandline
    tasksMainLayout->addWidget(tasksCheck[3]); // Client
    tasksMainLayout->addWidget(tasksCheck[4]); // User
    tasksMainLayout->addWidget(tasksCheck[5]); // Computer
    tasksMainLayout->addWidget(tasksCheck[2]); // Agent ID
    tasksMainLayout->addStretch();
    tasksTopRow->addWidget(tasksMainGroup);

    auto* tasksStatusGroup = new QGroupBox("Status", tasksGroup);
    auto* tasksStatusLayout = new QVBoxLayout(tasksStatusGroup);
    tasksStatusLayout->setContentsMargins(8, 8, 8, 8);
    tasksStatusLayout->addWidget(tasksCheck[7]); // Finish Time
    tasksStatusLayout->addWidget(tasksCheck[9]); // Result
    tasksStatusLayout->addStretch();
    tasksTopRow->addWidget(tasksStatusGroup);

    tasksGroupLayout->addLayout(tasksTopRow);

    tasksGroup->setLayout(tasksGroupLayout);
    tasksLayout->addWidget(tasksGroup, 1, 0, 1, 1);
    tasksLayout->setRowStretch(2, 1);
    tasksWidget->setLayout(tasksLayout);


    targetsWidget = new QWidget(this);
    targetsLayout = new QGridLayout(targetsWidget);
    targetsCompactSwitch = new oclero::qlementine::Switch(targetsWidget);
    targetsCompactSwitch->setText("Compact mode");
    targetsCompactSwitch->setToolTip("Single-line feed rows for Targets");
    auto* targetsPrefsRow = new QHBoxLayout();
    targetsPrefsRow->setContentsMargins(0, 0, 0, 0);
    targetsPrefsRow->addWidget(targetsCompactSwitch);
    targetsPrefsRow->addStretch();
    targetsLayout->addLayout(targetsPrefsRow, 0, 0, 1, 1);

    targetsGroup = new QGroupBox("Visible fields", targetsWidget);
    targetsGroup->setToolTip("Fields map to feed card blocks.");

    QStringList targetsCheckboxLabels = {
        "Icon", "Target ID", "Created", "Computer", "Domain",
        "Address", "OS", "Info", "Tags", "Status"
    };
    for (int i = 0; i < targetsCheckCount; ++i)
        targetsCheck[i] = new QCheckBox(targetsCheckboxLabels[i], targetsGroup);

    auto* targetsGroupLayout = new QVBoxLayout(targetsGroup);
    targetsGroupLayout->setSpacing(8);
    auto* targetsTopRow = new QHBoxLayout();
    targetsTopRow->setSpacing(8);

    auto* targetsIdentityGroup = new QGroupBox("Identity", targetsGroup);
    auto* targetsIdentityLayout = new QVBoxLayout(targetsIdentityGroup);
    targetsIdentityLayout->setContentsMargins(8, 8, 8, 8);
    targetsIdentityLayout->addWidget(targetsCheck[0]); // Icon
    targetsIdentityLayout->addWidget(targetsCheck[1]); // Target ID
    targetsIdentityLayout->addWidget(targetsCheck[2]); // Created
    targetsIdentityLayout->addStretch();
    targetsTopRow->addWidget(targetsIdentityGroup);

    auto* targetsMainGroup = new QGroupBox("Main", targetsGroup);
    auto* targetsMainLayout = new QVBoxLayout(targetsMainGroup);
    targetsMainLayout->setContentsMargins(8, 8, 8, 8);
    targetsMainLayout->addWidget(targetsCheck[3]); // Computer
    targetsMainLayout->addWidget(targetsCheck[4]); // Domain
    targetsMainLayout->addWidget(targetsCheck[5]); // Address
    targetsMainLayout->addWidget(targetsCheck[6]); // OS
    targetsMainLayout->addWidget(targetsCheck[7]); // Info
    targetsMainLayout->addStretch();
    targetsTopRow->addWidget(targetsMainGroup);

    auto* targetsRightGroup = new QGroupBox("Status / Tags", targetsGroup);
    auto* targetsRightLayout = new QVBoxLayout(targetsRightGroup);
    targetsRightLayout->setContentsMargins(8, 8, 8, 8);
    targetsRightLayout->addWidget(targetsCheck[9]); // Status
    targetsRightLayout->addWidget(targetsCheck[8]); // Tags
    targetsRightLayout->addStretch();
    targetsTopRow->addWidget(targetsRightGroup);

    targetsGroupLayout->addLayout(targetsTopRow);
    targetsGroup->setLayout(targetsGroupLayout);
    targetsLayout->addWidget(targetsGroup, 1, 0, 1, 1);
    targetsLayout->setRowStretch(2, 1);
    targetsWidget->setLayout(targetsLayout);


    credsWidget = new QWidget(this);
    credsLayout = new QGridLayout(credsWidget);
    credsCompactSwitch = new oclero::qlementine::Switch(credsWidget);
    credsCompactSwitch->setText("Compact mode");
    credsCompactSwitch->setToolTip("Single-line feed rows for Credentials.");
    auto* credsPrefsRow = new QHBoxLayout();
    credsPrefsRow->setContentsMargins(0, 0, 0, 0);
    credsPrefsRow->addWidget(credsCompactSwitch);
    credsPrefsRow->addStretch();
    credsLayout->addLayout(credsPrefsRow, 0, 0, 1, 1);

    credsGroup = new QGroupBox("Visible fields", credsWidget);
    credsGroup->setToolTip("Fields map to feed card blocks.");

    QStringList credsCheckboxLabels = {
        "Cred ID", "Type", "Created", "Username", "Realm",
        "Password", "Host", "Storage", "Agent ID", "Tags"
    };
    for (int i = 0; i < credsCheckCount; ++i)
        credsCheck[i] = new QCheckBox(credsCheckboxLabels[i], credsGroup);

    auto* credsGroupLayout = new QVBoxLayout(credsGroup);
    credsGroupLayout->setSpacing(8);
    auto* credsTopRow = new QHBoxLayout();
    credsTopRow->setSpacing(8);

    auto* credsIdentityGroup = new QGroupBox("Identity", credsGroup);
    auto* credsIdentityLayout = new QVBoxLayout(credsIdentityGroup);
    credsIdentityLayout->setContentsMargins(8, 8, 8, 8);
    credsIdentityLayout->addWidget(credsCheck[0]); // Cred ID
    credsIdentityLayout->addWidget(credsCheck[1]); // Type
    credsIdentityLayout->addWidget(credsCheck[2]); // Created
    credsIdentityLayout->addStretch();
    credsTopRow->addWidget(credsIdentityGroup);

    auto* credsSecretGroup = new QGroupBox("Credential", credsGroup);
    auto* credsSecretLayout = new QVBoxLayout(credsSecretGroup);
    credsSecretLayout->setContentsMargins(8, 8, 8, 8);
    credsSecretLayout->addWidget(credsCheck[3]); // Username
    credsSecretLayout->addWidget(credsCheck[4]); // Realm
    credsSecretLayout->addWidget(credsCheck[5]); // Password
    credsSecretLayout->addStretch();
    credsTopRow->addWidget(credsSecretGroup);

    auto* credsCtxGroup = new QGroupBox("Context", credsGroup);
    auto* credsCtxLayout = new QVBoxLayout(credsCtxGroup);
    credsCtxLayout->setContentsMargins(8, 8, 8, 8);
    credsCtxLayout->addWidget(credsCheck[6]); // Host
    credsCtxLayout->addWidget(credsCheck[7]); // Storage
    credsCtxLayout->addWidget(credsCheck[8]); // Agent ID
    credsCtxLayout->addWidget(credsCheck[9]); // Tags
    credsCtxLayout->addStretch();
    credsTopRow->addWidget(credsCtxGroup);

    credsGroupLayout->addLayout(credsTopRow);
    credsGroup->setLayout(credsGroupLayout);
    credsLayout->addWidget(credsGroup, 1, 0, 1, 1);
    credsLayout->setRowStretch(2, 1);
    credsWidget->setLayout(credsLayout);


    filesWidget = new QWidget(this);
    filesLayout = new QGridLayout(filesWidget);
    filesCompactSwitch = new oclero::qlementine::Switch(filesWidget);
    filesCompactSwitch->setText("Compact mode");
    filesCompactSwitch->setToolTip("Single-line feed rows for Downloads / Uploads / Sync.");
    auto* filesPrefsRow = new QHBoxLayout();
    filesPrefsRow->setContentsMargins(0, 0, 0, 0);
    filesPrefsRow->addWidget(filesCompactSwitch);
    filesPrefsRow->addStretch();
    filesLayout->addLayout(filesPrefsRow, 0, 0, 1, 1);

    filesGroup = new QGroupBox("Visible fields", filesWidget);
    filesGroup->setToolTip("Fields map to Downloads / Uploads / Sync feed cards.");

    QStringList filesCheckboxLabels = {
        "File ID", "Type", "Created", "Name", "Path",
        "User", "Computer", "Agent ID", "Tags", "Size", "Status"
    };
    for (int i = 0; i < filesCheckCount; ++i)
        filesCheck[i] = new QCheckBox(filesCheckboxLabels[i], filesGroup);

    auto* filesGroupLayout = new QVBoxLayout(filesGroup);
    filesGroupLayout->setSpacing(8);
    auto* filesTopRow = new QHBoxLayout();
    filesTopRow->setSpacing(8);

    auto* filesIdentityGroup = new QGroupBox("Identity", filesGroup);
    auto* filesIdentityLayout = new QVBoxLayout(filesIdentityGroup);
    filesIdentityLayout->setContentsMargins(8, 8, 8, 8);
    filesIdentityLayout->addWidget(filesCheck[0]); // File ID
    filesIdentityLayout->addWidget(filesCheck[1]); // Type
    filesIdentityLayout->addWidget(filesCheck[2]); // Created
    filesIdentityLayout->addStretch();
    filesTopRow->addWidget(filesIdentityGroup);

    auto* filesMainGroup = new QGroupBox("Main", filesGroup);
    auto* filesMainLayout = new QVBoxLayout(filesMainGroup);
    filesMainLayout->setContentsMargins(8, 8, 8, 8);
    filesMainLayout->addWidget(filesCheck[3]); // Name
    filesMainLayout->addWidget(filesCheck[4]); // Path
    filesMainLayout->addWidget(filesCheck[5]); // User
    filesMainLayout->addWidget(filesCheck[6]); // Computer
    filesMainLayout->addWidget(filesCheck[7]); // Agent ID
    filesMainLayout->addStretch();
    filesTopRow->addWidget(filesMainGroup);

    auto* filesRightGroup = new QGroupBox("Status / Tags", filesGroup);
    auto* filesRightLayout = new QVBoxLayout(filesRightGroup);
    filesRightLayout->setContentsMargins(8, 8, 8, 8);
    filesRightLayout->addWidget(filesCheck[10]); // Status
    filesRightLayout->addWidget(filesCheck[9]);  // Size
    filesRightLayout->addWidget(filesCheck[8]);  // Tags
    filesRightLayout->addStretch();
    filesTopRow->addWidget(filesRightGroup);

    filesGroupLayout->addLayout(filesTopRow);
    filesGroup->setLayout(filesGroupLayout);
    filesLayout->addWidget(filesGroup, 1, 0, 1, 1);
    filesLayout->setRowStretch(2, 1);
    filesWidget->setLayout(filesLayout);

    tabblinkWidget = new QWidget(this);
    tabblinkEnabledCheckbox = new oclero::qlementine::Switch(tabblinkWidget);
    tabblinkEnabledCheckbox->setText("Enable tab blink");

    tabblinkGroup = new QGroupBox("Blinking tabs", tabblinkWidget);
    tabblinkGroupLayout = new QGridLayout(tabblinkGroup);
    tabblinkGroupLayout->setContentsMargins(15, 15, 15, 15);
    tabblinkGroupLayout->setHorizontalSpacing(40);
    tabblinkGroupLayout->setVerticalSpacing(12);

    auto widgetsList = WidgetRegistry::instance().widgets();
    std::ranges::sort(widgetsList, [](const auto& a, const auto& b) { return a.displayName < b.displayName; });

    int row = 0, col = 0;
    for (const auto& info : widgetsList) {
        auto* check = new QCheckBox(info.displayName, tabblinkGroup);
        check->setChecked(info.defaultState);
        tabblinkGroupLayout->addWidget(check, row, col);
        m_tabblinkChecks[info.className] = check;

        col++;
        if (col > 1) { col = 0; row++; }
    }
    tabblinkGroup->setLayout(tabblinkGroupLayout);

    tabblinkLayout = new QGridLayout(tabblinkWidget);
    tabblinkLayout->addWidget(tabblinkEnabledCheckbox, 0, 0, 1, 1);
    tabblinkLayout->addWidget(tabblinkGroup,           1, 0, 1, 1);
    tabblinkLayout->setRowStretch(3, 1);

    tabblinkWidget->setLayout(tabblinkLayout);

    shortcutsWidget = new QWidget(this);
    shortcutsLayout = new QGridLayout(shortcutsWidget);

    shortcutsTable = new QTableWidget(shortcutsWidget);
    shortcutsTable->setColumnCount(3);
    shortcutsTable->setHorizontalHeaderLabels({"Shortcut", "Context", "Action"});
    shortcutsTable->horizontalHeader()->setStretchLastSection(true);
    shortcutsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    shortcutsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    shortcutsTable->verticalHeader()->setVisible(false);
    shortcutsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    shortcutsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    shortcutsTable->setSelectionMode(QAbstractItemView::NoSelection);
    shortcutsTable->setAlternatingRowColors(true);

    auto addRow = [&](const QString& key, const QString& context, const QString& action) {
        int row = shortcutsTable->rowCount();
        shortcutsTable->insertRow(row);
        auto* keyItem = new QTableWidgetItem(key);
        keyItem->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        shortcutsTable->setItem(row, 0, keyItem);
        shortcutsTable->setItem(row, 1, new QTableWidgetItem(context));
        shortcutsTable->setItem(row, 2, new QTableWidgetItem(action));
    };

    auto addSection = [&](const QString& title) {
        int row = shortcutsTable->rowCount();
        shortcutsTable->insertRow(row);
        auto* item = new QTableWidgetItem(title);
        item->setFlags(Qt::ItemIsEnabled);
        item->setTextAlignment(Qt::AlignCenter);
        QFont f = item->font();
        f.setBold(true);
        item->setFont(f);
        QHeaderView* h = shortcutsTable->horizontalHeader();
        item->setBackground(h->palette().brush(QPalette::Normal, QPalette::Window));
        item->setForeground(h->palette().brush(QPalette::Normal, QPalette::WindowText));
        shortcutsTable->setItem(row, 0, item);
        shortcutsTable->setSpan(row, 0, 1, 3);
    };

    addSection("Global");
    addRow("Ctrl+Shift+S",       "Main UI",    "Switch to Sessions tab");
    addRow("Ctrl+Shift+G",       "Main UI",    "Switch to Graph tab");
    addRow("Ctrl+Shift+L",       "Main UI",    "Switch to Listeners tab");
    addRow("Ctrl+Shift+N",       "Main UI",    "Switch to Logs tab");
    addRow("Ctrl+Shift+J",       "Main UI",    "Switch to Tasks tab");
    addRow("Ctrl+Shift+E",       "Main UI",    "Switch to Script Manager");
    addRow("Ctrl+Shift+F",       "Main UI",    "Switch to Downloads tab");
    addRow("Ctrl+Shift+T",       "Main UI",    "Switch to Targets tab");
    addRow("Ctrl+Shift+C",       "Main UI",    "Switch to Credentials tab");
    addRow("Ctrl+Shift+P",       "Main UI",    "Switch to Tunnels tab");
    addRow("Ctrl+Shift+I",       "Main UI",    "Switch to Screenshots tab");
    addRow("Ctrl+Shift+R",       "Main UI",    "Switch to Settings");
    addRow("Ctrl+Left",          "Main UI",    "Navigate to previous dock");
    addRow("Ctrl+Right",         "Main UI",    "Navigate to next dock");
    addRow("Ctrl+D",             "Main UI",    "Close current dock");
    addRow("Ctrl+W",             "Main UI",    "Float/Unfloat dock");

    addSection("Sessions Table");
    addRow("Ctrl+F",             "Sessions",   "Search / Filter");
    addRow("Esc",                "Sessions",   "Clear filter");
    addRow("Ctrl+L",             "Sessions",   "Open File Browser");
    addRow("Ctrl+P",             "Sessions",   "Open Process Browser");
    addRow("Ctrl+T",             "Sessions",   "Open Terminal");
    addRow("Ctrl+I",             "Sessions",   "Open Console");

    addSection("Console / Chat / Logs");
    addRow("Ctrl+F",             "Console",    "Find in view");
    addRow("Ctrl+Shift+F",       "Console",    "Search history (server)");
    addRow("Ctrl+L",             "Console",    "Clear output");
    addRow("Ctrl+A",             "Console",    "Select all");
    addRow("Ctrl+H",             "Console",    "Command history");

    addSection("Terminal");
    addRow("Ctrl+Shift+C",      "Terminal",   "Copy selection");
    addRow("Ctrl+Shift+V",      "Terminal",   "Paste from clipboard");
    addRow("Ctrl+Shift+F",      "Terminal",   "Search in terminal");
    addRow("Ctrl+Shift+L",      "Terminal",   "Clear terminal");

    addSection("Other Widgets");
    addRow("Ctrl+F",             "List/Table", "Search / Filter");
    addRow("Esc",                "List/Table", "Clear filter");

    shortcutsLayout->addWidget(shortcutsTable, 0, 0);
    shortcutsLayout->setContentsMargins(0, 0, 0, 0);
    shortcutsWidget->setLayout(shortcutsLayout);

    listSettings = new QListWidget(this);
    listSettings->setFixedWidth(150);
    listSettings->setSpacing(2);
    listSettings->setObjectName("SettingsSidebar");

    auto addSectionHeader = [&](const QString& title) {
        auto* item = new QListWidgetItem(listSettings);
        item->setFlags(Qt::NoItemFlags);
        item->setSizeHint(QSize(0, 28));
        auto* label = new QLabel(title, listSettings);
        label->setObjectName("SidebarSectionLabel");
        label->setStyleSheet(
            "QLabel#SidebarSectionLabel {"
            "  color: palette(highlight);"
            "  font-size: 10px;"
            "  font-weight: 700;"
            "  letter-spacing: 1px;"
            "  padding: 6px 8px 2px 8px;"
            "  border-bottom: 1px solid palette(mid);"
            "}"
        );
        listSettings->setItemWidget(item, label);
    };

    int pageIndex = 0;
    auto addNavItem = [&](const QString& title) {
        auto* item = new QListWidgetItem(title, listSettings);
        item->setSizeHint(QSize(0, 28));
        item->setData(Qt::UserRole, pageIndex++);
    };

    addSectionHeader("INTERFACE");
    addNavItem("Appearance");
    addNavItem("Console");
    addSectionHeader("DATA");
    addNavItem("Sessions");
    addNavItem("Tasks");
    addNavItem("Targets");
    addNavItem("Credentials");
    addNavItem("Files");
    addSectionHeader("NOTIFICATIONS");
    addNavItem("Tab Blinking");
    addSectionHeader("REFERENCE");
    addNavItem("Shortcuts");

    for (int i = 0; i < listSettings->count(); ++i) {
        if (listSettings->item(i)->flags() & Qt::ItemIsSelectable) {
            listSettings->setCurrentRow(i);
            break;
        }
    }

    labelHeader = new QLabel(this);
    QFont headerFont = labelHeader->font();
    headerFont.setPointSize(14);
    headerFont.setBold(true);
    labelHeader->setFont(headerFont);
    labelHeader->setContentsMargins(0, 0, 0, 10);
    labelHeader->setText("Appearance");

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
    buttonApply->setDefault(true);
    buttonApply->setEnabled(false);

    stackSettings = new QStackedWidget(this);
    stackSettings->addWidget(appearanceWidget);
    stackSettings->addWidget(consolePageWidget);
    stackSettings->addWidget(sessionsWidget);
    stackSettings->addWidget(tasksWidget);
    stackSettings->addWidget(targetsWidget);
    stackSettings->addWidget(credsWidget);
    stackSettings->addWidget(filesWidget);
    stackSettings->addWidget(tabblinkWidget);
    stackSettings->addWidget(shortcutsWidget);

    layoutMain = new QGridLayout(this);
    layoutMain->setContentsMargins(4, 4, 4, 4);
    layoutMain->addWidget(listSettings, 0, 0, 3, 1);
    layoutMain->addLayout(headerLayout, 0, 1, 1, 3);
    layoutMain->addWidget(stackSettings, 1, 1, 1, 3);
    layoutMain->addItem(hSpacer, 2, 1, 1, 1);
    layoutMain->addWidget(buttonApply, 2, 2, 1, 1);
    layoutMain->addWidget(buttonClose, 2, 3, 1, 1);

    this->setLayout(layoutMain);

    int buttonWidth = buttonApply->width();
    buttonApply->setFixedWidth(buttonWidth);
    buttonClose->setFixedWidth(buttonWidth);

    int buttonHeight = buttonClose->height();
    buttonApply->setFixedHeight(buttonHeight);
    buttonClose->setFixedHeight(buttonHeight);
}

void DialogSettings::onStackChange(int index) const
{
    auto* item = listSettings->item(index);
    if (!item || !(item->flags() & Qt::ItemIsSelectable))
        return;

    labelHeader->setText(item->text());
    stackSettings->setCurrentIndex(item->data(Qt::UserRole).toInt());
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

void DialogSettings::onBlinkChange() const
{
    buttonApply->setEnabled(true);
    bool active = tabblinkEnabledCheckbox->isChecked();
    tabblinkGroup->setEnabled(active);
}

void DialogSettings::onUseAppThemeChange() const
{
    buttonApply->setEnabled(true);
    bool useApp = consoleUseAppThemeCheckbox->isChecked();
    consoleThemeLabel->setEnabled(!useApp);
    consoleThemeCombo->setEnabled(!useApp);
    consoleThemeImportBtn->setEnabled(!useApp);
    consoleThemeDeleteBtn->setEnabled(!useApp);
}

void DialogSettings::onApply() const
{
    buttonApply->setEnabled(false);

    bool themeChanged = settings->data.MainTheme != themeCombo->currentText();
    bool fontChanged  = settings->data.FontSize != fontSizeSpin->value() || settings->data.FontFamily != fontFamilyCombo->currentText();

    if(themeChanged) {
        settings->data.MainTheme = themeCombo->currentText();

        if (auto* style = settings->getMainAdaptix()->qlementineStyle) {
            QString userPath = userAppThemeDir() + "/" + settings->data.MainTheme + ".json";
            QString themePath = QFile::exists(userPath) ? userPath : QString(":/qlementine-themes/%1").arg(settings->data.MainTheme);
            style->setThemeJsonPath(themePath);
        }

        TitleBarStyle::applyForTheme(settings->getMainAdaptix()->mainUI, settings->data.MainTheme);

        if (consoleUseAppThemeCheckbox->isChecked())
            Q_EMIT ConsoleThemeManager::instance().themeChanged();
    }

    if(fontChanged) {
        settings->data.FontSize   = fontSizeSpin->value();
        settings->data.FontFamily = fontFamilyCombo->currentText();
    }

    if(themeChanged || fontChanged) {
        settings->getMainAdaptix()->ApplyApplicationFont();
    }

    if (settings->data.GraphVersion != graphCombo1->currentText()) {
        settings->data.GraphVersion = graphCombo1->currentText();
        settings->getMainAdaptix()->mainUI->UpdateGraphIcons();
    }
    settings->data.GraphAutoHideInactive = graphAutoHideInactiveSwitch->isChecked();
    settings->data.GraphAutoHideNoChilds = graphAutoHideNoChildsSwitch->isChecked();

    settings->data.RemoteTerminalBufferSize = terminalSizeSpin->value();

    const int prevToolbarPos = settings->data.ToolbarPosition;
    for (int i = 0; i < 4; ++i) {
        if (toolbarPosBtn[i]->isChecked()) {
            settings->data.ToolbarPosition = i;
            break;
        }
    }
    if (settings->data.ToolbarPosition != prevToolbarPos) {
        settings->getMainAdaptix()->mainUI->RebuildToolbars();
    }

    settings->data.ConsoleBufferSize = consoleSizeSpin->value();
    settings->data.ConsoleTime = consoleTimeCheckbox->isChecked();
    settings->data.ConsoleNoWrap = consoleNoWrapCheckbox->isChecked();
    settings->data.ConsoleAutoScroll = consoleAutoScrollCheckbox->isChecked();
    settings->data.ConsoleAutoLoadEarlier = consoleAutoLoadEarlierCheckbox->isChecked();
    settings->data.ConsolePageSize = consolePageSizeSpin->value();
    settings->getMainAdaptix()->mainUI->UpdateConsolePrefs();

    bool bgChanged = settings->data.ConsoleShowBackground != consoleShowBackgroundCheckbox->isChecked();
    settings->data.ConsoleShowBackground = consoleShowBackgroundCheckbox->isChecked();

    bool useAppThemeChanged = settings->data.ConsoleUseAppTheme != consoleUseAppThemeCheckbox->isChecked();
    settings->data.ConsoleUseAppTheme = consoleUseAppThemeCheckbox->isChecked();

    QString bgPath = consoleBgImagePathEdit->text();
    if (bgPath.endsWith(" (Default)"))
        bgPath.chop(10);
    bool bgImageChanged = settings->data.ConsoleBgImagePath != bgPath || settings->data.ConsoleBgDimming != consoleBgDimmingSlider->value();
    settings->data.ConsoleBgImagePath = bgPath;
    settings->data.ConsoleBgDimming   = consoleBgDimmingSlider->value();

    if (settings->data.ConsoleTheme != consoleThemeCombo->currentText() || bgChanged) {
        settings->data.ConsoleTheme = consoleThemeCombo->currentText();
        ConsoleThemeManager::instance().loadTheme(settings->data.ConsoleTheme);
    }
    else if (useAppThemeChanged || bgImageChanged) {
        Q_EMIT ConsoleThemeManager::instance().themeChanged();
    }

    bool updateTable = false;
    for ( int i = 0; i < sessionsCheckCount; i++) {
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
    settings->data.DeadLightnessShift = sessionsDeadShiftSpin->value();
    settings->data.SessionsAutoHideInactive = sessionsAutoHideInactiveSwitch->isChecked();
    settings->data.SessionsCompactMode = sessionsCompactSwitch->isChecked();

    updateTable = false;
    for ( int i = 0; i < tasksCheckCount; i++) {
        if (settings->data.TasksTableColumns[i] != tasksCheck[i]->isChecked()) {
            settings->data.TasksTableColumns[i] = tasksCheck[i]->isChecked();
            updateTable = true;
        }
    }
    if (updateTable)
        settings->getMainAdaptix()->mainUI->UpdateTasksTableColumns();

    settings->data.TasksInProcessOnly = tasksInProcessSwitch->isChecked();
    settings->data.TasksCompactMode = tasksCompactSwitch->isChecked();

    updateTable = false;
    for (int i = 0; i < targetsCheckCount; i++) {
        if (settings->data.TargetsTableColumns[i] != targetsCheck[i]->isChecked()) {
            settings->data.TargetsTableColumns[i] = targetsCheck[i]->isChecked();
            updateTable = true;
        }
    }
    if (updateTable)
        settings->getMainAdaptix()->mainUI->UpdateTargetsColumns();

    settings->data.TargetsCompactMode = targetsCompactSwitch->isChecked();

    updateTable = false;
    for (int i = 0; i < credsCheckCount; i++) {
        if (settings->data.CredentialsTableColumns[i] != credsCheck[i]->isChecked()) {
            settings->data.CredentialsTableColumns[i] = credsCheck[i]->isChecked();
            updateTable = true;
        }
    }
    if (updateTable)
        settings->getMainAdaptix()->mainUI->UpdateCredentialsColumns();

    settings->data.CredentialsCompactMode = credsCompactSwitch->isChecked();

    updateTable = false;
    for (int i = 0; i < filesCheckCount; i++) {
        if (settings->data.FilesTableColumns[i] != filesCheck[i]->isChecked()) {
            settings->data.FilesTableColumns[i] = filesCheck[i]->isChecked();
            updateTable = true;
        }
    }
    if (updateTable)
        settings->getMainAdaptix()->mainUI->UpdateFilesColumns();

    settings->data.FilesCompactMode = filesCompactSwitch->isChecked();

    for (auto it = m_tabblinkChecks.begin(); it != m_tabblinkChecks.end(); ++it)
        settings->data.BlinkWidgets[it.key()] = it.value()->isChecked();

    settings->SaveToDB();
    settings->getMainAdaptix()->mainUI->ApplyFeedViewPreferences();
}

void DialogSettings::onClose()
{
    this->close();
}

void DialogSettings::updateThemeSwatches()
{
    QString name = themeCombo->currentText();
    if (name.isEmpty())
        return;

    QString userPath = userAppThemeDir() + "/" + name + ".json";
    QString resPath  = QString(":/qlementine-themes/%1.json").arg(name);
    QString jsonPath = QFile::exists(userPath) ? userPath : (QFile::exists(resPath) ? resPath : QString());
    if (jsonPath.isEmpty())
        return;

    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    QJsonObject root = doc.object();

    auto readColor = [&](const QString& key, const QString& fallback) -> QColor {
        return root.contains(key) ? QColor(root[key].toString()) : QColor(fallback);
    };

    QColor primary   = readColor("primaryColor", "#1890ff");
    QColor secondary = readColor("secondaryColor", "#404040");
    QColor success   = readColor("statusColorSuccess", "#2bb5a0");
    QColor error     = readColor("statusColorError", "#e96b72");
    QColor border    = readColor("borderColor", "#d3d3d3");

    QColor colors[5] = {primary, secondary, success, error, border};
    for (int i = 0; i < 5; ++i) {
        themeSwatchLabels[i]->setStyleSheet( QString("background: %1; border-radius: 10px; border: 1px solid rgba(255,255,255,0.1);").arg(colors[i].name()));
    }
}

void DialogSettings::loadSettings()
{
    themeCombo->setCurrentText(settings->data.MainTheme);
    updateThemeSwatches();
    fontFamilyCombo->setCurrentText(settings->data.FontFamily);
    fontSizeSpin->setValue(settings->data.FontSize);
    graphCombo1->setCurrentText(settings->data.GraphVersion);
    graphAutoHideInactiveSwitch->setChecked(settings->data.GraphAutoHideInactive);
    graphAutoHideNoChildsSwitch->setChecked(settings->data.GraphAutoHideNoChilds);
    terminalSizeSpin->setValue(settings->data.RemoteTerminalBufferSize);

    int tbPos = qBound(0, settings->data.ToolbarPosition, 3);
    for (int i = 0; i < 4; ++i)
        toolbarPosBtn[i]->setChecked(i == tbPos);

    consoleSizeSpin->setValue(settings->data.ConsoleBufferSize);
    consoleTimeCheckbox->setChecked(settings->data.ConsoleTime);
    consoleNoWrapCheckbox->setChecked(settings->data.ConsoleNoWrap);
    consoleAutoScrollCheckbox->setChecked(settings->data.ConsoleAutoScroll);
    consoleAutoLoadEarlierCheckbox->setChecked(settings->data.ConsoleAutoLoadEarlier);
    consolePageSizeSpin->setValue(qBound(10, settings->data.ConsolePageSize, 2000));
    consoleShowBackgroundCheckbox->setChecked(settings->data.ConsoleShowBackground);
    consoleUseAppThemeCheckbox->setChecked(settings->data.ConsoleUseAppTheme);

    bool useApp    = settings->data.ConsoleUseAppTheme;
    bool hasBg     = !settings->data.ConsoleBgImagePath.isEmpty();
    bool isDefault = (settings->data.ConsoleBgImagePath == ":/Back");

    consoleBgImagePathEdit->setText(isDefault ? ":/Back (Default)" : settings->data.ConsoleBgImagePath);
    consoleBgDimmingSlider->setValue(settings->data.ConsoleBgDimming);
    consoleBgDimmingValueLabel->setText(QString("%1%").arg(settings->data.ConsoleBgDimming));
    consoleThemeCombo->setCurrentText(settings->data.ConsoleTheme);
    consoleThemeLabel->setEnabled(!useApp);
    consoleThemeCombo->setEnabled(!useApp);
    consoleThemeImportBtn->setEnabled(!useApp);
    consoleThemeDeleteBtn->setEnabled(!useApp);
    bool showBg = settings->data.ConsoleShowBackground;
    consoleBgDimmingLabel->setEnabled(showBg);
    consoleBgDimmingSlider->setEnabled(showBg);
    consoleBgDimmingValueLabel->setEnabled(showBg);

    if (hasBg) {
        QPixmap pix(settings->data.ConsoleBgImagePath);
        if (!pix.isNull()) {
            consoleBgPreviewLabel->setPixmap(pix);
            consoleBgImageClearBtn->setEnabled(!isDefault);
        } else {
            consoleBgPreviewLabel->clear();
            consoleBgPreviewLabel->setText("Image not found");
        }
    } else {
        consoleBgPreviewLabel->clear();
        consoleBgPreviewLabel->setText("No background image");
    }

    for (int i = 0; i < sessionsCheckCount; i++)
        sessionsCheck[i]->setChecked(settings->data.SessionsTableColumns[i]);

    sessionsHealthCheck->setChecked(settings->data.CheckHealth);
    sessionsAutoHideInactiveSwitch->setChecked(settings->data.SessionsAutoHideInactive);
    sessionsCompactSwitch->setChecked(settings->data.SessionsCompactMode);
    sessionsCoafSpin->setValue(settings->data.HealthCoaf);
    sessionsOffsetSpin->setValue(settings->data.HealthOffset);
    sessionsDeadShiftSpin->setValue(settings->data.DeadLightnessShift);

    for (int i = 0; i < tasksCheckCount; i++)
        tasksCheck[i]->setChecked(settings->data.TasksTableColumns[i]);
    tasksInProcessSwitch->setChecked(settings->data.TasksInProcessOnly);
    tasksCompactSwitch->setChecked(settings->data.TasksCompactMode);

    for (int i = 0; i < targetsCheckCount; i++)
        targetsCheck[i]->setChecked(settings->data.TargetsTableColumns[i]);
    targetsCompactSwitch->setChecked(settings->data.TargetsCompactMode);

    for (int i = 0; i < credsCheckCount; i++)
        credsCheck[i]->setChecked(settings->data.CredentialsTableColumns[i]);
    credsCompactSwitch->setChecked(settings->data.CredentialsCompactMode);

    for (int i = 0; i < filesCheckCount; i++)
        filesCheck[i]->setChecked(settings->data.FilesTableColumns[i]);
    filesCompactSwitch->setChecked(settings->data.FilesCompactMode);

    tabblinkEnabledCheckbox->setChecked(settings->data.TabBlinkEnabled);

    for (auto it = m_tabblinkChecks.begin(); it != m_tabblinkChecks.end(); ++it) {
        if ( settings->data.BlinkWidgets.contains(it.key()) ) {
            bool enabled = settings->data.BlinkWidgets[it.key()];
            it.value()->setChecked(enabled);
        }
    }

    buttonApply->setEnabled(false);
}

void DialogSettings::showEvent(QShowEvent* event)
{
    loadSettings();
    QWidget::showEvent(event);
}

QString DialogSettings::userAppThemeDir()
{
    QString dir = QDir(QDir::homePath()).filePath(".adaptix/themes/app");
    QDir().mkpath(dir);
    return dir;
}

bool DialogSettings::importAppTheme(const QString& filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || fi.suffix().toLower() != "json")
        return false;

    QString destDir = userAppThemeDir();
    QString destPath = destDir + "/" + fi.fileName();
    if (QFile::exists(destPath))
        QFile::remove(destPath);

    return QFile::copy(filePath, destPath);
}

bool DialogSettings::deleteAppTheme(const QString& name)
{
    QString userPath = userAppThemeDir() + "/" + name + ".json";
    if (!QFile::exists(userPath))
        return false;
    return QFile::remove(userPath);
}

void DialogSettings::refreshAppThemeCombo()
{
    QString current = themeCombo->currentText();
    themeCombo->clear();

    QStringList builtIn = { "Adaptix_Dark_Emerald", "Adaptix_Light_Emerald", "Adaptix_Dracula" };
    themeCombo->addItems(builtIn);

    QDir userDir(userAppThemeDir());
    for (const auto& entry : userDir.entryList({"*.json"}, QDir::Files)) {
        QString name = QFileInfo(entry).baseName();
        if (!builtIn.contains(name))
            themeCombo->addItem(name);
    }

    if (!current.isEmpty())
        themeCombo->setCurrentText(current);
}

void DialogSettings::refreshConsoleThemeCombo()
{
    QString current = consoleThemeCombo->currentText();
    consoleThemeCombo->clear();
    consoleThemeCombo->addItems(ConsoleThemeManager::instance().availableThemes());
    if (!current.isEmpty())
        consoleThemeCombo->setCurrentText(current);
}
