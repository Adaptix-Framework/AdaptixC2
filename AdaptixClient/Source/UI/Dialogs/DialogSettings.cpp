#include <UI/MainUI.h>
#include <UI/Dialogs/DialogSettings.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <UI/Widgets/CodeEditorWidget.h>
#include <MainAdaptix.h>
#include <Client/Settings.h>
#include <Client/DockLayoutEngine.h>
#include <Client/ConsoleTheme.h>
#include <Client/CodeEditorProfileManager.h>
#include <Utils/TitleBarStyle.h>
#include <Utils/FontManager.h>

#include <oclero/qlementine.hpp>

#include <QInputDialog>
#include <QSignalBlocker>
#include <QMessageBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QCheckBox>
#include <QFrame>
#include <QShowEvent>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPlainTextEdit>
#include <QHeaderView>
#include <QFontDatabase>
#include <QRegularExpression>
#include <QAbstractItemView>
#include <QTabWidget>
#include <QButtonGroup>
#include <QAbstractButton>
#include <QPainter>
#include <QPainterPath>
#include <QListWidget>
#include <QTabBar>
#include <QStackedWidget>
#include <QSizePolicy>
#include <QGridLayout>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QListView>
#include <QIcon>
#include <algorithm>

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

    for ( int i = 0; i < payloadsCheckCount; i++)
        connect(payloadsCheck[i], &QCheckBox::checkStateChanged, buttonApply, [this](int){buttonApply->setEnabled(true);} );

    connect(sessionsViewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){buttonApply->setEnabled(true);});
    connect(sessionsAutoHideInactiveSwitch, &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(sessionsCompactSwitch,          &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(tasksInProcessSwitch,           &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(tasksCompactSwitch,             &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(targetsViewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){buttonApply->setEnabled(true);});
    connect(credsViewCombo,   QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int){buttonApply->setEnabled(true);});
    connect(targetsCompactSwitch,           &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(credsCompactSwitch,             &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(filesCompactSwitch,             &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(graphAutoHideInactiveSwitch,    &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});
    connect(graphAutoHideNoChildsSwitch,    &oclero::qlementine::Switch::toggled, buttonApply, [this](bool){buttonApply->setEnabled(true);});

    connect(listSettings, &QListWidget::currentRowChanged, this, &DialogSettings::onStackChange);
    connect(buttonApply,  &QPushButton::clicked,           this, &DialogSettings::onApply);
    connect(buttonClose,  &QPushButton::clicked,           this, &DialogSettings::onClose);

    if (toolbarPosCombo) {
        connect(toolbarPosCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { if (buttonApply) buttonApply->setEnabled(true); });
    }
}

namespace {

constexpr int kSettingsLabelColW = 140;
constexpr int kSettingsFieldMinW = 260;
constexpr int kSettingsFieldMaxW = 420;

class LayoutSchemeCard : public QAbstractButton
{
public:
    explicit LayoutSchemeCard(const QString& layoutId, const QString& title, QWidget* parent = nullptr) : QAbstractButton(parent), m_layoutId(layoutId), m_title(title)
    {
        setCheckable(true);
        setAutoExclusive(true);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(128, 108);
        setToolTip(title);
    }

    QString layoutId() const { return m_layoutId; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QPalette pal = palette();
        const QColor bg = isChecked() ? pal.color(QPalette::Highlight).darker(160) : pal.color(QPalette::Base);
        const QColor border = isChecked() ? pal.color(QPalette::Highlight) : pal.color(QPalette::Mid);
        const QColor zoneFill = isChecked() ? pal.color(QPalette::Highlight).lighter(130) : pal.color(QPalette::Window);
        const QColor zoneLine = isChecked() ? pal.color(QPalette::HighlightedText).darker(120) : pal.color(QPalette::Mid);
        const QColor textCol = pal.color(QPalette::WindowText);

        QRectF outer = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        p.setPen(QPen(border, isChecked() ? 2.0 : 1.0));
        p.setBrush(bg);
        p.drawRoundedRect(outer, 8, 8);

        QRectF diagram(14, 12, width() - 28, 58);
        p.setPen(QPen(zoneLine, 1.0));
        p.setBrush(zoneFill);

        auto drawZone = [&](const QRectF& r) {
            p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
        };

        if (m_layoutId == QLatin1String("split_v2")) {
            const qreal gap = 3;
            const qreal h1 = diagram.height() * 0.55;
            drawZone(QRectF(diagram.left(), diagram.top(), diagram.width(), h1));
            drawZone(QRectF(diagram.left(), diagram.top() + h1 + gap, diagram.width(), diagram.height() - h1 - gap));
        } else if (m_layoutId == QLatin1String("main_left")) {
            const qreal gap = 3;
            const qreal hTop = diagram.height() * 0.52;
            const qreal wL = (diagram.width() - gap) / 2.0;
            drawZone(QRectF(diagram.left(), diagram.top(), wL, hTop));
            drawZone(QRectF(diagram.left() + wL + gap, diagram.top(), wL, hTop));
            drawZone(QRectF(diagram.left(), diagram.top() + hTop + gap, diagram.width(), diagram.height() - hTop - gap));
        } else if (m_layoutId == QLatin1String("main_right")) {
            const qreal gap = 3;
            const qreal wR = diagram.width() * 0.48;
            const qreal xR = diagram.right() - wR;
            drawZone(QRectF(xR, diagram.top(), wR, diagram.height()));
            const qreal wL = xR - diagram.left() - gap;
            const qreal h1 = diagram.height() * 0.48;
            drawZone(QRectF(diagram.left(), diagram.top(), wL, h1));
            drawZone(QRectF(diagram.left(), diagram.top() + h1 + gap, wL, diagram.height() - h1 - gap));
        } else { // quad
            const qreal gap = 3;
            const qreal w = (diagram.width() - gap) / 2.0;
            const qreal h = (diagram.height() - gap) / 2.0;
            drawZone(QRectF(diagram.left(), diagram.top(), w, h));
            drawZone(QRectF(diagram.left() + w + gap, diagram.top(), w, h));
            drawZone(QRectF(diagram.left(), diagram.top() + h + gap, w, h));
            drawZone(QRectF(diagram.left() + w + gap, diagram.top() + h + gap, w, h));
        }

        p.setPen(textCol);
        QFont f = font();
        f.setPointSize(qMax(9, f.pointSize() - 1));
        f.setBold(isChecked());
        p.setFont(f);
        p.drawText(QRect(6, height() - 28, width() - 12, 22), Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap, m_title);
    }

private:
    QString m_layoutId;
    QString m_title;
};

void applySettingsFormGrid(QGridLayout* grid, int labelCol = 0, int fieldCol = 1)
{
    if (!grid)
        return;
    grid->setContentsMargins(12, 12, 12, 12);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    grid->setColumnMinimumWidth(labelCol, kSettingsLabelColW);
    grid->setColumnStretch(labelCol, 0);
    grid->setColumnStretch(fieldCol, 0);
    if (grid->columnCount() <= fieldCol + 1)
        grid->setColumnStretch(fieldCol + 1, 1);
    else
        grid->setColumnStretch(fieldCol + 1, 1);
}

void limitFieldWidth(QWidget* w, int minW = kSettingsFieldMinW, int maxW = kSettingsFieldMaxW)
{
    if (!w)
        return;
    w->setMinimumWidth(minW);
    w->setMaximumWidth(maxW);
    w->setSizePolicy(QSizePolicy::Preferred, w->sizePolicy().verticalPolicy());
}

void styleSettingsLabel(QLabel* lab)
{
    if (!lab)
        return;
    lab->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lab->setMinimumWidth(kSettingsLabelColW);
}

const char* kSettingsCardCss =
    "QGroupBox {"
    "  font-weight: 600;"
    "  margin-top: 10px;"
    "  padding-top: 12px;"
    "}"
    "QGroupBox::title {"
    "  subcontrol-origin: margin;"
    "  left: 10px;"
    "  padding: 0 4px;"
    "}";

} // namespace

void DialogSettings::createUI()
{
    this->setWindowTitle("Adaptix Settings");
    this->resize(900, 600);
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
    QStringList swatchLabels = {"Background", "Primary", "Text", "Success", "Error"};
    for (int i = 0; i < 5; ++i) {
        auto* swatch = new QLabel(themeSwatchesFrame);
        swatch->setFixedSize(20, 20);
        swatch->setStyleSheet(QStringLiteral("background: #555555; border-radius: 10px; border: 1px solid rgba(255,255,255,0.15);"));
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
    toolbarPosCombo = new QComboBox(appearanceWidget);
    toolbarPosCombo->addItem(QStringLiteral("Top"), 0);
    toolbarPosCombo->addItem(QStringLiteral("Bottom"), 1);
    toolbarPosCombo->addItem(QStringLiteral("Left"), 2);
    toolbarPosCombo->addItem(QStringLiteral("Right"), 3);
    toolbarPosCombo->setToolTip(QStringLiteral("Place the toolbar on the selected edge. Takes effect on next project open."));

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
    consoleBgPreviewFrame->setMinimumHeight(180);
    consoleBgPreviewFrame->setMaximumHeight(240);
    auto* previewLayout = new QVBoxLayout(consoleBgPreviewFrame);
    previewLayout->setContentsMargins(4, 4, 4, 4);
    consoleBgPreviewLabel = new QLabel(consoleBgPreviewFrame);
    consoleBgPreviewLabel->setAlignment(Qt::AlignCenter);
    consoleBgPreviewLabel->setMinimumSize(200, 160);
    consoleBgPreviewLabel->setStyleSheet(QStringLiteral("color: palette(placeholderText); font-size: %1px;").arg(FontManager::instance().typography().chromeFontPx));
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
    themeButtons->setSpacing(6);
    themeButtons->addWidget(themeImportBtn);
    themeButtons->addWidget(themeDeleteBtn);
    themeButtons->addStretch(1);

    auto* appearanceThemeGroup = new QGroupBox(QStringLiteral("Theme"), appearanceWidget);
    appearanceThemeGroup->setStyleSheet(kSettingsCardCss);
    auto* appearanceThemeLay = new QGridLayout(appearanceThemeGroup);
    applySettingsFormGrid(appearanceThemeLay);
    styleSettingsLabel(themeLabel);
    limitFieldWidth(themeCombo, 220, 320);
    auto* themeComboRow = new QWidget(appearanceThemeGroup);
    auto* themeComboLay = new QHBoxLayout(themeComboRow);
    themeComboLay->setContentsMargins(0, 0, 0, 0);
    themeComboLay->setSpacing(8);
    themeComboLay->addWidget(themeCombo, 0);
    themeComboLay->addLayout(themeButtons, 1);
    appearanceThemeLay->addWidget(themeLabel, 0, 0);
    appearanceThemeLay->addWidget(themeComboRow, 0, 1);
    appearanceThemeLay->addWidget(themeSwatchesFrame, 1, 0, 1, 2);
    appearanceThemeLay->setColumnStretch(2, 1);

    auto* appearanceTypeGroup = new QGroupBox(QStringLiteral("Font"), appearanceWidget);
    appearanceTypeGroup->setStyleSheet(kSettingsCardCss);
    auto* appearanceTypeLay = new QGridLayout(appearanceTypeGroup);
    applySettingsFormGrid(appearanceTypeLay);
    styleSettingsLabel(fontFamilyLabel);
    styleSettingsLabel(fontSizeLabel);
    limitFieldWidth(fontFamilyCombo, 220, 360);
    limitFieldWidth(fontSizeSpin, 100, 140);
    appearanceTypeLay->addWidget(fontFamilyLabel, 0, 0);
    appearanceTypeLay->addWidget(fontFamilyCombo, 0, 1, Qt::AlignLeft);
    appearanceTypeLay->addWidget(fontSizeLabel, 1, 0);
    appearanceTypeLay->addWidget(fontSizeSpin, 1, 1, Qt::AlignLeft);
    appearanceTypeLay->setColumnStretch(2, 1);

    auto* dockPresetGroup = new QGroupBox(QStringLiteral("Dock layout"), appearanceWidget);
    dockPresetGroup->setStyleSheet(kSettingsCardCss);
    auto* dockPresetLay = new QVBoxLayout(dockPresetGroup);
    dockPresetLay->setContentsMargins(12, 12, 12, 12);
    dockPresetLay->setSpacing(10);

    auto* toolbarRow = new QWidget(dockPresetGroup);
    auto* toolbarRowLay = new QHBoxLayout(toolbarRow);
    toolbarRowLay->setContentsMargins(0, 0, 0, 0);
    toolbarRowLay->setSpacing(12);
    styleSettingsLabel(toolbarPosLabel);
    limitFieldWidth(toolbarPosCombo, 140, 200);
    toolbarRowLay->addWidget(toolbarPosLabel, 0);
    toolbarRowLay->addWidget(toolbarPosCombo, 0);
    toolbarRowLay->addStretch(1);

    dockLayoutHint = new QLabel(QStringLiteral("Choose a zone scheme, then drag widgets into columns. Takes effect on next project open."), dockPresetGroup);
    dockLayoutHint->setWordWrap(true);
    dockLayoutHint->setStyleSheet(QStringLiteral("color: palette(placeholderText); font-size: 11px;"));

    dockLayoutSchemes = new QWidget(dockPresetGroup);
    auto* schemesLay = new QHBoxLayout(dockLayoutSchemes);
    schemesLay->setContentsMargins(0, 0, 0, 0);
    schemesLay->setSpacing(10);

    dockLayoutGroup = new QButtonGroup(this);
    dockLayoutGroup->setExclusive(true);

    struct SchemeSpec { const char* id; const char* shortTitle; };
    const SchemeSpec schemes[] = {
        { "main_right", "Big right" },
        { "main_left",  "Top split" },
        { "split_v2",   "2 zones" },
        { "quad",       "4 zones" },
    };
    for (const auto& s : schemes) {
        auto* card = new LayoutSchemeCard(QString::fromLatin1(s.id), QString::fromLatin1(s.shortTitle), dockLayoutSchemes);
        card->setProperty("layoutId", QString::fromLatin1(s.id));
        dockLayoutGroup->addButton(card);
        schemesLay->addWidget(card, 0, Qt::AlignLeft);
    }
    schemesLay->addStretch(1);

    connect(dockLayoutGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton*) {
        refreshDockZoneColumns();
        if (buttonApply)
            buttonApply->setEnabled(true);
    });

    auto* dockMapLabel = new QLabel(QStringLiteral("Drag widgets between zones. Check ☐ to open at project start."), dockPresetGroup);
    dockMapLabel->setStyleSheet(QStringLiteral("font-weight: 600;"));
    dockMapLabel->setWordWrap(true);

    dockZoneColumnsHost = new QWidget(dockPresetGroup);
    auto* zoneColsLay = new QHBoxLayout(dockZoneColumnsHost);
    zoneColsLay->setContentsMargins(0, 0, 0, 0);
    zoneColsLay->setSpacing(8);
    dockZoneColumnsHost->setMinimumHeight(280);

    dockPresetLay->addWidget(toolbarRow);
    dockPresetLay->addWidget(dockLayoutHint);
    dockPresetLay->addWidget(dockLayoutSchemes);
    dockPresetLay->addWidget(dockMapLabel);
    dockPresetLay->addWidget(dockZoneColumnsHost, 1);

    auto* appearanceInner = new QWidget(appearanceWidget);
    auto* appearanceInnerLay = new QVBoxLayout(appearanceInner);
    appearanceInnerLay->setContentsMargins(0, 0, 0, 0);
    appearanceInnerLay->setSpacing(12);
    appearanceInnerLay->addWidget(appearanceThemeGroup);
    appearanceInnerLay->addWidget(appearanceTypeGroup);
    appearanceInnerLay->addWidget(dockPresetGroup);
    appearanceInnerLay->addStretch(1);

    auto* appearanceScroll = new QScrollArea(appearanceWidget);
    appearanceScroll->setWidgetResizable(true);
    appearanceScroll->setFrameShape(QFrame::NoFrame);
    appearanceScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    appearanceScroll->setWidget(appearanceInner);

    appearanceLayout->setContentsMargins(0, 0, 0, 0);
    appearanceLayout->setHorizontalSpacing(0);
    appearanceLayout->setVerticalSpacing(0);
    appearanceLayout->addWidget(appearanceScroll, 0, 0);
    appearanceLayout->setRowStretch(0, 1);
    appearanceLayout->setColumnStretch(0, 1);
    appearanceWidget->setLayout(appearanceLayout);

    applySettingsFormGrid(consoleThemeGroupLayout);
    styleSettingsLabel(consoleThemeLabel);
    styleSettingsLabel(consoleBgImageLabel);
    styleSettingsLabel(consoleBgDimmingLabel);
    limitFieldWidth(consoleThemeCombo, 200, 300);
    limitFieldWidth(consoleBgImagePathEdit, 220, 400);
    consoleThemeGroup->setStyleSheet(kSettingsCardCss);
    consoleThemeGroupLayout->setColumnStretch(2, 0);
    consoleThemeGroupLayout->setColumnStretch(3, 1);

    applySettingsFormGrid(consoleBehaviorGroupLayout);
    styleSettingsLabel(consoleSizeLabel);
    styleSettingsLabel(consolePageSizeLabel);
    limitFieldWidth(consoleSizeSpin, 120, 180);
    limitFieldWidth(consolePageSizeSpin, 120, 180);
    consoleBehaviorGroup->setStyleSheet(kSettingsCardCss);
    consoleBehaviorGroupLayout->setColumnStretch(2, 1);

    consolePageLayout->setContentsMargins(0, 0, 0, 0);
    consolePageLayout->setVerticalSpacing(12);
    consolePageLayout->addWidget(consoleThemeGroup,    0, 0, 1, 1);
    consolePageLayout->addWidget(consoleBehaviorGroup, 1, 0, 1, 1);

    auto* terminalGroup = new QGroupBox(QStringLiteral("Remote Terminal"), consolePageWidget);
    terminalGroup->setStyleSheet(kSettingsCardCss);
    auto* terminalLay = new QGridLayout(terminalGroup);
    applySettingsFormGrid(terminalLay);
    styleSettingsLabel(terminalSizeLabel);
    limitFieldWidth(terminalSizeSpin, 120, 180);
    terminalLay->addWidget(terminalSizeLabel, 0, 0);
    terminalLay->addWidget(terminalSizeSpin, 0, 1, Qt::AlignLeft);
    terminalLay->setColumnStretch(2, 1);
    consolePageLayout->addWidget(terminalGroup, 2, 0, 1, 1);
    consolePageLayout->setRowStretch(3, 1);
    consolePageLayout->setColumnStretch(0, 1);
    consolePageWidget->setLayout(consolePageLayout);

    codeEditorWidget = new QWidget(this);
    codeEditorLayout = new QHBoxLayout(codeEditorWidget);
    codeEditorLayout->setContentsMargins(0, 0, 0, 0);
    codeEditorLayout->setSpacing(12);

    const QColor textColor = codeEditorWidget->palette().color(QPalette::WindowText);
    QColor mutedColor = textColor;
    mutedColor.setAlphaF(codeEditorWidget->palette().color(QPalette::Base).lightnessF() < 0.5 ? 0.58 : 0.55);
    const QString mutedCss = QStringLiteral("color: %1; font-size: 11px;").arg(mutedColor.name(QColor::HexArgb));
    const QString sectionTitleCss = QStringLiteral("font-weight: 600; font-size: 12px; color: palette(window-text);");
    const QString cardCss = QStringLiteral(
        "QFrame#CeCard {"
        "  background: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 10px;"
        "}");
    const QString innerCss = QStringLiteral(
        "QFrame#CeInner {"
        "  background: palette(alternate-base);"
        "  border: 1px solid transparent;"
        "  border-radius: 8px;"
        "}");

    auto makeCard = [&](QWidget* parent) {
        auto* f = new QFrame(parent);
        f->setObjectName(QStringLiteral("CeCard"));
        f->setStyleSheet(cardCss);
        return f;
    };
    auto makeInner = [&](QWidget* parent) {
        auto* f = new QFrame(parent);
        f->setObjectName(QStringLiteral("CeInner"));
        f->setStyleSheet(innerCss);
        return f;
    };

    auto* leftCard = makeCard(codeEditorWidget);
    leftCard->setMinimumWidth(188);
    leftCard->setMaximumWidth(220);
    auto* leftLay = new QVBoxLayout(leftCard);
    leftLay->setContentsMargins(10, 10, 10, 10);
    leftLay->setSpacing(8);

    auto* listLabel = new QLabel(QStringLiteral("Profiles"), leftCard);
    listLabel->setStyleSheet(sectionTitleCss);
    leftLay->addWidget(listLabel);

    codeEditorProfileList = new QListWidget(leftCard);
    codeEditorProfileList->setFrameShape(QFrame::NoFrame);
    codeEditorProfileList->setSpacing(1);
    codeEditorProfileList->setUniformItemSizes(true);
    codeEditorProfileList->setStyleSheet(QStringLiteral(
        "QListWidget { background: transparent; outline: none; border: none; }"
        "QListWidget::item {"
        "  padding: 6px 10px;"
        "  border-radius: 6px;"
        "  margin: 0;"
        "  color: palette(window-text);"
        "}"
        "QListWidget::item:selected {"
        "  background: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"
        "QListWidget::item:hover:!selected {"
        "  background: palette(alternate-base);"
        "}"));
    leftLay->addWidget(codeEditorProfileList, 1);

    auto* listBtns = new QHBoxLayout();
    listBtns->setContentsMargins(0, 0, 0, 0);
    listBtns->setSpacing(4);

    auto makeIconBtn = [&](const QString& iconPath, const QString& tip) {
        auto* b = new QPushButton(leftCard);
        b->setIcon(QIcon(iconPath));
        b->setIconSize(QSize(18, 18));
        b->setFixedSize(QSize(28, 28));
        b->setToolTip(tip);
        b->setFocusPolicy(Qt::NoFocus);
        return b;
    };

    codeEditorAddBtn = makeIconBtn(QStringLiteral(":/icons/plus"), QStringLiteral("Add profile"));
    codeEditorRemoveBtn = makeIconBtn(QStringLiteral(":/icons/delete"), QStringLiteral("Remove profile"));
    codeEditorForkBtn = makeIconBtn(QStringLiteral(":/icons/extension"), QStringLiteral("Fork profile to a user copy"));
    codeEditorImportBtn = makeIconBtn(QStringLiteral(":/icons/file_open"), QStringLiteral("Import profile from JSON"));
    codeEditorExportBtn = makeIconBtn(QStringLiteral(":/icons/save_as"), QStringLiteral("Export profile to JSON"));

    auto* sep = new QFrame(leftCard);
    sep->setFrameShape(QFrame::VLine);
    sep->setFrameShadow(QFrame::Sunken);
    sep->setFixedHeight(22);

    listBtns->addWidget(codeEditorAddBtn, 1);
    listBtns->addWidget(codeEditorForkBtn, 1);
    listBtns->addWidget(codeEditorRemoveBtn, 1);
    listBtns->addWidget(sep, 0);
    listBtns->addWidget(codeEditorImportBtn, 1);
    listBtns->addWidget(codeEditorExportBtn, 1);
    leftLay->addLayout(listBtns);
    codeEditorLayout->addWidget(leftCard, 0);

    auto* rightCard = makeCard(codeEditorWidget);
    auto* rightLay = new QVBoxLayout(rightCard);
    rightLay->setContentsMargins(14, 12, 14, 12);
    rightLay->setSpacing(10);

    codeEditorNameEdit = new QLineEdit(rightCard);
    codeEditorNameEdit->setPlaceholderText(QStringLiteral("Profile name"));
    codeEditorNameEdit->setFixedHeight(32);
    QFont nameFont = codeEditorNameEdit->font();
    nameFont.setPointSizeF(nameFont.pointSizeF() > 0 ? nameFont.pointSizeF() + 0.5 : 11.5);
    nameFont.setWeight(QFont::DemiBold);
    codeEditorNameEdit->setFont(nameFont);
    rightLay->addWidget(codeEditorNameEdit);

    codeEditorTabs = new QTabWidget(rightCard);
    codeEditorTabs->setDocumentMode(true);
    codeEditorTabs->tabBar()->setExpanding(false);
    codeEditorTabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane {"
        "  border: none;"
        "  top: 4px;"
        "  background: transparent;"
        "}"
        "QTabBar::tab {"
        "  background: transparent;"
        "  color: palette(window-text);"
        "  padding: 6px 14px 8px 14px;"
        "  margin-right: 2px;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "  font-weight: 500;"
        "}"
        "QTabBar::tab:selected {"
        "  background: transparent;"
        "  color: palette(window-text);"
        "  font-weight: 600;"
        "  border-bottom: 2px solid palette(highlight);"
        "}"
        "QTabBar::tab:!selected {"
        "  color: %1;"
        "}"
        "QTabBar::tab:!selected:hover {"
        "  color: palette(window-text);"
        "  background: palette(alternate-base);"
        "  border-radius: 6px 6px 0 0;"
        "}").arg(mutedColor.name(QColor::HexArgb)));

    QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    if (monoFont.pointSizeF() <= 0)
        monoFont.setPointSize(10);

    auto* generalPage = new QWidget(codeEditorTabs);
    auto* generalLay = new QVBoxLayout(generalPage);
    generalLay->setContentsMargins(0, 12, 0, 4);
    generalLay->setSpacing(12);

    auto* langCard = makeInner(generalPage);
    auto* langLay = new QVBoxLayout(langCard);
    langLay->setContentsMargins(12, 12, 12, 12);
    langLay->setSpacing(8);
    auto* langTitle = new QLabel(QStringLiteral("Language"), langCard);
    langTitle->setStyleSheet(sectionTitleCss);
    langLay->addWidget(langTitle);
    codeEditorLanguageCombo = new QComboBox(langCard);
    codeEditorLanguageCombo->addItem(QStringLiteral("AxScript"), QStringLiteral("axscript"));
    codeEditorLanguageCombo->addItem(QStringLiteral("C"), QStringLiteral("c"));
    codeEditorLanguageCombo->addItem(QStringLiteral("C++"), QStringLiteral("cpp"));
    codeEditorLanguageCombo->addItem(QStringLiteral("Plain text"), QStringLiteral("plain"));
    codeEditorLanguageCombo->setMinimumHeight(32);
    langLay->addWidget(codeEditorLanguageCombo);
    auto* langHint = new QLabel(QStringLiteral("Syntax highlighting for the editor. Handlers & panel live on the other tabs."), langCard);
    langHint->setWordWrap(true);
    langHint->setStyleSheet(mutedCss);
    langLay->addWidget(langHint);
    generalLay->addWidget(langCard);

    auto* panelCard = makeInner(generalPage);
    auto* panelCardLay = new QVBoxLayout(panelCard);
    panelCardLay->setContentsMargins(12, 12, 12, 12);
    panelCardLay->setSpacing(8);
    auto* panelTitle = new QLabel(QStringLiteral("Bottom panel"), panelCard);
    panelTitle->setStyleSheet(sectionTitleCss);
    panelCardLay->addWidget(panelTitle);
    codeEditorPanelEnabledSwitch = new oclero::qlementine::Switch(panelCard);
    codeEditorPanelEnabledSwitch->setText(QStringLiteral("Show GeneratePanel under the editor"));
    codeEditorPanelEnabledSwitch->setToolTip(
        QStringLiteral("On — AxScript GeneratePanel() in the editor bottom strip.\n"
                       "Off — no config panel."));
    panelCardLay->addWidget(codeEditorPanelEnabledSwitch);
    auto* panelHint = new QLabel(QStringLiteral("Edit the script on the Panel tab. Values are free-form keys in the container."), panelCard);
    panelHint->setWordWrap(true);
    panelHint->setStyleSheet(mutedCss);
    panelCardLay->addWidget(panelHint);
    generalLay->addWidget(panelCard);

    auto* flowCard = makeInner(generalPage);
    auto* flowLay = new QVBoxLayout(flowCard);
    flowLay->setContentsMargins(12, 12, 12, 12);
    flowLay->setSpacing(6);
    auto* flowTitle = new QLabel(QStringLiteral("How it works"), flowCard);
    flowTitle->setStyleSheet(sectionTitleCss);
    flowLay->addWidget(flowTitle);
    auto* flowBody = new QLabel(
        QStringLiteral(
            "1. Panel — GeneratePanel() builds fields (cmd, switches, …)\n"
            "2. Toolbar — buttons with AxScript handlers\n"
            "3. Handler — get_panel_data + content + eval / job_start\n"
            "4. Log — editor.log / job stdout / ax.log"),
        flowCard);
    flowBody->setWordWrap(true);
    flowBody->setStyleSheet(mutedCss);
    flowLay->addWidget(flowBody);
    generalLay->addWidget(flowCard);

    auto* resetRow = new QHBoxLayout();
    resetRow->addStretch(1);
    auto* resetBtn = new QPushButton(QStringLiteral("Reset panel & actions to defaults"), generalPage);
    resetBtn->setFixedHeight(30);
    resetBtn->setToolTip(QStringLiteral("Replace GeneratePanel script and toolbar handlers with current defaults for this profile type."));
    resetRow->addWidget(resetBtn);
    generalLay->addLayout(resetRow);
    generalLay->addStretch(1);

    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        if (codeEditorEditingId.isEmpty())
            return;
        auto* mgr = CodeEditorProfileManager::instance();
        const BuildProfile* cur = mgr->profile(codeEditorEditingId);
        if (!cur || cur->isSystem())
            return;
        const auto reply = QMessageBox::question(this, QStringLiteral("Reset profile?"),
            QStringLiteral("Reset panel script and toolbar actions for “%1” to defaults?\n"
                           "Panel field values (panelState) are kept when possible.")
                .arg(cur->name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
        BuildProfile p = *cur;
        p.applyCurrentDefaults(true);
        mgr->updateProfile(p);
        loadCodeEditorProfileToForm(p.name);
        buttonApply->setEnabled(true);
    });
    codeEditorResetDefaultsBtn = resetBtn;

    codeEditorBuildFields = nullptr;
    codeEditorBuildEdit = nullptr;
    codeEditorRunEdit = nullptr;
    codeEditorDefinesEdit = nullptr;
    codeEditorMainEngineSwitch = nullptr;

    codeEditorTabs->addTab(generalPage, QStringLiteral("General"));

    auto* toolbarPage = new QWidget(codeEditorTabs);
    auto* toolbarLay = new QVBoxLayout(toolbarPage);
    toolbarLay->setContentsMargins(0, 12, 0, 4);
    toolbarLay->setSpacing(12);

    auto* builtInCard = makeInner(toolbarPage);
    auto* builtInLay = new QVBoxLayout(builtInCard);
    builtInLay->setContentsMargins(12, 10, 12, 10);
    builtInLay->setSpacing(6);
    auto* builtInTitle = new QLabel(QStringLiteral("Editor chrome"), builtInCard);
    builtInTitle->setStyleSheet(sectionTitleCss);
    builtInLay->addWidget(builtInTitle);

    auto makeCb = [builtInCard](const QString& text) {
        auto* cb = new QCheckBox(text, builtInCard);
        cb->setMinimumHeight(24);
        return cb;
    };
    codeEditorTbNewFile = makeCb(QStringLiteral("New"));
    codeEditorTbOpenFile = makeCb(QStringLiteral("Open"));
    codeEditorTbOpenFolder = makeCb(QStringLiteral("Folder"));
    codeEditorTbSave = makeCb(QStringLiteral("Save"));
    codeEditorTbExplorer = makeCb(QStringLiteral("Explorer"));
    codeEditorTbBuildLog = makeCb(QStringLiteral("Log"));
    codeEditorTbMinimap = makeCb(QStringLiteral("Minimap"));
    codeEditorTbWordWrap = makeCb(QStringLiteral("Wrap"));

    auto* tbGrid = new QGridLayout();
    tbGrid->setContentsMargins(0, 4, 0, 0);
    tbGrid->setHorizontalSpacing(14);
    tbGrid->setVerticalSpacing(6);
    tbGrid->setColumnMinimumWidth(0, 44);
    auto placeRow = [&](int row, const QString& title, std::initializer_list<QCheckBox*> cbs) {
        auto* t = new QLabel(title, builtInCard);
        t->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        t->setStyleSheet(mutedCss + QStringLiteral(" font-weight: 600;"));
        tbGrid->addWidget(t, row, 0, Qt::AlignVCenter);
        int col = 1;
        for (QCheckBox* cb : cbs)
            tbGrid->addWidget(cb, row, col++, Qt::AlignVCenter | Qt::AlignLeft);
        tbGrid->setColumnStretch(col, 1);
    };
    placeRow(0, QStringLiteral("Files"), {codeEditorTbNewFile, codeEditorTbOpenFile, codeEditorTbOpenFolder, codeEditorTbSave});
    placeRow(1, QStringLiteral("View"), {codeEditorTbExplorer, codeEditorTbBuildLog, codeEditorTbMinimap, codeEditorTbWordWrap});
    builtInLay->addLayout(tbGrid);
    toolbarLay->addWidget(builtInCard);

    auto* customHead = new QHBoxLayout();
    customHead->setSpacing(8);
    customHead->setAlignment(Qt::AlignVCenter);
    auto* customLabel = new QLabel(QStringLiteral("Toolbar actions"), toolbarPage);
    customLabel->setStyleSheet(sectionTitleCss);
    codeEditorActionAddBtn = new QPushButton(QStringLiteral("Add button"), toolbarPage);
    codeEditorActionRemoveBtn = new QPushButton(QStringLiteral("Remove"), toolbarPage);
    codeEditorActionAddBtn->setFixedHeight(28);
    codeEditorActionRemoveBtn->setFixedHeight(28);
    customHead->addWidget(customLabel, 0, Qt::AlignVCenter);
    customHead->addStretch(1);
    customHead->addWidget(codeEditorActionAddBtn, 0, Qt::AlignVCenter);
    customHead->addWidget(codeEditorActionRemoveBtn, 0, Qt::AlignVCenter);
    toolbarLay->addLayout(customHead);

    codeEditorCustomStack = new QStackedWidget(toolbarPage);

    auto* emptyPage = makeInner(codeEditorCustomStack);
    auto* emptyLay = new QVBoxLayout(emptyPage);
    emptyLay->setContentsMargins(20, 28, 20, 28);
    emptyLay->setSpacing(12);
    emptyLay->setAlignment(Qt::AlignCenter);
    codeEditorCustomEmptyLabel = new QLabel(
        QStringLiteral("No toolbar actions yet.\n"
                       "Add toolbar actions — each runs an AxScript handler with global editor."),
        emptyPage);
    codeEditorCustomEmptyLabel->setAlignment(Qt::AlignCenter);
    codeEditorCustomEmptyLabel->setWordWrap(true);
    codeEditorCustomEmptyLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(mutedColor.name(QColor::HexArgb)));
    emptyLay->addWidget(codeEditorCustomEmptyLabel, 0, Qt::AlignCenter);
    auto* emptyAdd = new QPushButton(QStringLiteral("Add first button"), emptyPage);
    emptyAdd->setFixedHeight(30);
    emptyAdd->setMinimumWidth(140);
    emptyLay->addWidget(emptyAdd, 0, Qt::AlignCenter);
    connect(emptyAdd, &QPushButton::clicked, codeEditorActionAddBtn, &QPushButton::click);
    codeEditorCustomStack->addWidget(emptyPage);

    codeEditorCustomEditorPage = new QWidget(codeEditorCustomStack);
    auto* customSplit = new QHBoxLayout(codeEditorCustomEditorPage);
    customSplit->setContentsMargins(0, 0, 0, 0);
    customSplit->setSpacing(10);

    codeEditorActionsTable = new QTableWidget(0, 4, codeEditorCustomEditorPage);
    codeEditorActionsTable->setHorizontalHeaderLabels({
        QStringLiteral(""), QStringLiteral("Action"), QStringLiteral("Type"), QStringLiteral("Body")
    });
    codeEditorActionsTable->setColumnHidden(2, true);
    codeEditorActionsTable->setColumnHidden(3, true);
    codeEditorActionsTable->horizontalHeader()->setStretchLastSection(false);
    codeEditorActionsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    codeEditorActionsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    codeEditorActionsTable->setColumnWidth(0, 40);
    codeEditorActionsTable->verticalHeader()->setVisible(false);
    codeEditorActionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    codeEditorActionsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    codeEditorActionsTable->setShowGrid(false);
    codeEditorActionsTable->setAlternatingRowColors(false);
    codeEditorActionsTable->setIconSize(QSize(20, 20));
    codeEditorActionsTable->setMaximumWidth(200);
    codeEditorActionsTable->setMinimumWidth(168);
    codeEditorActionsTable->setStyleSheet(QStringLiteral(
        "QTableWidget {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "  background: palette(base);"
        "  gridline-color: transparent;"
        "  outline: none;"
        "}"
        "QTableWidget::item {"
        "  padding: 6px 8px;"
        "}"
        "QTableWidget::item:selected {"
        "  background: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"
        "QHeaderView::section {"
        "  background: palette(alternate-base);"
        "  color: %1;"
        "  border: none;"
        "  border-bottom: 1px solid palette(mid);"
        "  padding: 6px 8px;"
        "  font-weight: 600;"
        "  font-size: 11px;"
        "}")
        .arg(mutedColor.name(QColor::HexArgb)));
    customSplit->addWidget(codeEditorActionsTable, 0);

    auto* detailCard = makeInner(codeEditorCustomEditorPage);
    auto* scriptCol = new QVBoxLayout(detailCard);
    scriptCol->setContentsMargins(12, 10, 12, 10);
    scriptCol->setSpacing(8);

    auto* metaRow = new QHBoxLayout();
    metaRow->setSpacing(10);
    codeEditorActionIconBtn = new QPushButton(detailCard);
    codeEditorActionIconBtn->setFixedSize(40, 40);
    codeEditorActionIconBtn->setToolTip(QStringLiteral("Choose icon from client resources (:/icons)"));
    codeEditorActionIconBtn->setEnabled(false);
    codeEditorActionIconBtn->setIconSize(QSize(24, 24));
    codeEditorActionIconBtn->setCursor(Qt::PointingHandCursor);
    auto* metaRight = new QVBoxLayout();
    metaRight->setSpacing(2);
    metaRight->setContentsMargins(0, 0, 0, 0);
    auto* labelCaption = new QLabel(QStringLiteral("Label"), detailCard);
    labelCaption->setStyleSheet(mutedCss);
    codeEditorActionLabelEdit = new QLineEdit(detailCard);
    codeEditorActionLabelEdit->setPlaceholderText(QStringLiteral("Button label"));
    codeEditorActionLabelEdit->setEnabled(false);
    codeEditorActionLabelEdit->setFixedHeight(32);
    metaRight->addWidget(labelCaption);
    metaRight->addWidget(codeEditorActionLabelEdit);
    metaRow->addWidget(codeEditorActionIconBtn, 0, Qt::AlignBottom);
    metaRow->addLayout(metaRight, 1);
    scriptCol->addLayout(metaRow);

    codeEditorActionScriptHost = new QWidget(detailCard);
    auto* scriptHostLay = new QVBoxLayout(codeEditorActionScriptHost);
    scriptHostLay->setContentsMargins(0, 0, 0, 0);
    scriptHostLay->setSpacing(4);
    codeEditorActionBodyLabel = new QLabel(QStringLiteral("Handler (AxScript)"), codeEditorActionScriptHost);
    codeEditorActionBodyLabel->setStyleSheet(mutedCss);
    codeEditorActionScriptEdit = new QPlainTextEdit(codeEditorActionScriptHost);
    codeEditorActionScriptEdit->setPlaceholderText(QStringLiteral(
        "let p = editor.get_panel_data();\n"
        "editor.eval(editor.content(), { main: !!p.mainEngine });\n"));
    codeEditorActionScriptEdit->setFont(monoFont);
    codeEditorActionScriptEdit->setTabStopDistance(28);
    codeEditorActionScriptEdit->setEnabled(false);
    codeEditorActionScriptEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        "  background: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 6px;"
        "  padding: 6px;"
        "}"));
    scriptHostLay->addWidget(codeEditorActionBodyLabel);
    scriptHostLay->addWidget(codeEditorActionScriptEdit, 1);
    scriptCol->addWidget(codeEditorActionScriptHost, 1);

    auto* axHint = new QLabel(
        QStringLiteral(
            "editor: get_panel_data · content · file · save · expand · log · eval · job_*\n"
            "save() = write current tab to disk · file() path · content() text"),
        detailCard);
    axHint->setStyleSheet(mutedCss);
    axHint->setWordWrap(true);
    scriptCol->addWidget(axHint);

    customSplit->addWidget(detailCard, 1);
    codeEditorCustomStack->addWidget(codeEditorCustomEditorPage);
    codeEditorCustomStack->setCurrentIndex(0);

    toolbarLay->addWidget(codeEditorCustomStack, 1);
    codeEditorTabs->addTab(toolbarPage, QStringLiteral("Toolbar"));

    auto* panelPage = new QWidget(codeEditorTabs);
    auto* panelLay = new QVBoxLayout(panelPage);
    panelLay->setContentsMargins(0, 12, 0, 4);
    panelLay->setSpacing(10);

    auto* panelToggleRow = new QHBoxLayout();
    panelToggleRow->setSpacing(10);
    codeEditorPanelScriptHint = new QLabel(panelPage);
    codeEditorPanelScriptHint->setStyleSheet(mutedCss);
    codeEditorPanelScriptHint->setWordWrap(true);
    panelToggleRow->addWidget(codeEditorPanelScriptHint, 1, Qt::AlignVCenter);
    codeEditorPanelScriptTplBtn = new QPushButton(QStringLiteral("Insert template"), panelPage);
    codeEditorPanelScriptTplBtn->setToolTip(QStringLiteral("Insert GeneratePanel() skeleton"));
    codeEditorPanelScriptTplBtn->setFixedHeight(28);
    panelToggleRow->addWidget(codeEditorPanelScriptTplBtn, 0, Qt::AlignVCenter);
    panelLay->addLayout(panelToggleRow);

    codeEditorPanelScriptEdit = new QPlainTextEdit(panelPage);
    codeEditorPanelScriptEdit->setPlaceholderText(QStringLiteral("function GeneratePanel() { … }"));
    codeEditorPanelScriptEdit->setFont(monoFont);
    codeEditorPanelScriptEdit->setTabStopDistance(28);
    codeEditorPanelScriptEdit->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        "  background: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "  padding: 10px;"
        "  selection-background-color: palette(highlight);"
        "}"
        "QPlainTextEdit:disabled {"
        "  color: %1;"
        "  background: palette(alternate-base);"
        "}")
        .arg(mutedColor.name(QColor::HexArgb)));
    panelLay->addWidget(codeEditorPanelScriptEdit, 1);

    auto* panelGlobals = new QLabel(QStringLiteral("Panel context: __state  ·  __profileName  ·  __language  (values restored into container after GeneratePanel)"), panelPage);
    panelGlobals->setObjectName(QStringLiteral("CePanelGlobals"));
    panelGlobals->setStyleSheet(mutedCss);
    panelLay->addWidget(panelGlobals);

    codeEditorTabs->addTab(panelPage, QStringLiteral("Panel"));

    rightLay->addWidget(codeEditorTabs, 1);
    codeEditorLayout->addWidget(rightCard, 1);
    codeEditorWidget->setLayout(codeEditorLayout);

    auto listItemKey = [](QListWidgetItem* item) -> QString {
        if (!item)
            return {};
        const QString id = item->data(Qt::UserRole).toString();
        return id.isEmpty() ? item->text() : id;
    };

    connect(codeEditorProfileList, &QListWidget::currentItemChanged, this, [this, listItemKey](QListWidgetItem* current, QListWidgetItem* /*previous*/) {
        if (!codeEditorLoading && !codeEditorEditingId.isEmpty())
            saveCodeEditorProfileFromForm();
        if (current)
            loadCodeEditorProfileToForm(listItemKey(current));
        else if (!codeEditorLoading)
            codeEditorEditingId.clear();
    });
    connect(codeEditorAddBtn, &QPushButton::clicked, this, [this]() {
        if (!codeEditorLoading && !codeEditorEditingId.isEmpty())
            saveCodeEditorProfileFromForm();
        auto* mgr = CodeEditorProfileManager::instance();
        const QString created = mgr->addProfile(QStringLiteral("Profile"));
        refreshCodeEditorProfilesList();
        const auto items = codeEditorProfileList->findItems(created, Qt::MatchExactly);
        if (!items.isEmpty())
            codeEditorProfileList->setCurrentItem(items.first());
        if (codeEditorNameEdit) {
            codeEditorNameEdit->setFocus(Qt::OtherFocusReason);
            codeEditorNameEdit->selectAll();
        }
        buttonApply->setEnabled(true);
    });
    connect(codeEditorForkBtn, &QPushButton::clicked, this, [this, listItemKey]() {
        auto* item = codeEditorProfileList->currentItem();
        if (!item)
            return;
        if (!codeEditorLoading && !codeEditorEditingId.isEmpty())
            saveCodeEditorProfileFromForm();
        auto* mgr = CodeEditorProfileManager::instance();
        const QString key = listItemKey(item);
        const BuildProfile* src = mgr->profile(key);
        if (!src)
            return;
        const QString newId = mgr->forkProfile(key);
        if (newId.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Fork Profile"), QStringLiteral("Could not fork profile."));
            return;
        }
        refreshCodeEditorProfilesList();
        for (int i = 0; i < codeEditorProfileList->count(); ++i) {
            auto* it = codeEditorProfileList->item(i);
            if (it && it->data(Qt::UserRole).toString() == newId) {
                codeEditorProfileList->setCurrentItem(it);
                break;
            }
        }
        buttonApply->setEnabled(true);
    });
    connect(codeEditorRemoveBtn, &QPushButton::clicked, this, [this, listItemKey]() {
        auto* item = codeEditorProfileList->currentItem();
        if (!item)
            return;
        auto* mgr = CodeEditorProfileManager::instance();
        const BuildProfile* p = mgr->profile(listItemKey(item));
        if (!p || !p->isDeletable()) {
            QMessageBox::information(this, QStringLiteral("Remove Profile"), QStringLiteral("System profiles (AxScript, BOF, Event Handler) cannot be removed."));
            return;
        }
        const auto reply = QMessageBox::question(this, QStringLiteral("Remove Profile"),
            QStringLiteral("Remove profile '%1'?").arg(p->name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
        codeEditorEditingId.clear();
        if (mgr->removeProfile(p->id))
            refreshCodeEditorProfilesList();
    });
    connect(codeEditorExportBtn, &QPushButton::clicked, this, [this]() { exportCodeEditorProfile(); });
    connect(codeEditorImportBtn, &QPushButton::clicked, this, [this]() { importCodeEditorProfile(); });

    auto markDirty = [this]() {
        if (!codeEditorLoading)
            buttonApply->setEnabled(true);
    };
    connect(codeEditorNameEdit,    &QLineEdit::textEdited, this, [markDirty](const QString&) { markDirty(); });
    connect(codeEditorLanguageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, markDirty](int) {
        markDirty();
        updateCodeEditorPanelScriptVisibility();
    });
    if (codeEditorPanelEnabledSwitch) {
        connect(codeEditorPanelEnabledSwitch, &oclero::qlementine::Switch::toggled, this, [this, markDirty](bool) {
                    updateCodeEditorPanelScriptVisibility();
                    markDirty();
                });
    }
    if (codeEditorPanelScriptEdit)
        connect(codeEditorPanelScriptEdit, &QPlainTextEdit::textChanged, this, [markDirty]() { markDirty(); });
    if (codeEditorPanelScriptTplBtn) {
        connect(codeEditorPanelScriptTplBtn, &QPushButton::clicked, this, [this, markDirty]() {
            if (!codeEditorPanelScriptEdit)
                return;
            if (!codeEditorPanelScriptEdit->toPlainText().trimmed().isEmpty()) {
                const auto reply = QMessageBox::question(this, QStringLiteral("Insert template"),
                    QStringLiteral("Replace current panel script with the template?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (reply != QMessageBox::Yes)
                    return;
            }
            QString tpl = CodeEditorWidget::defaultPanelScriptTemplate();
            if (codeEditorLanguageCombo
                && codeEditorLanguageCombo->currentData().toString() == QLatin1String("axscript"))
                tpl = BuildProfile::defaultAxScriptPanelScript();
            else if (codeEditorLanguageCombo) {
                const QString lang = codeEditorLanguageCombo->currentData().toString();
                if (lang == QLatin1String("c") || lang == QLatin1String("cpp"))
                    tpl = BuildProfile::defaultBofPanelScript();
            }
            codeEditorPanelScriptEdit->setPlainText(tpl);
            markDirty();
        });
    }
    if (codeEditorActionAddBtn) {
        connect(codeEditorActionAddBtn, &QPushButton::clicked, this, [this, markDirty]() {
            if (!codeEditorActionsTable)
                return;
            syncCodeEditorActionScriptToTable(codeEditorActionEditRow);
            if (codeEditorCustomStack)
                codeEditorCustomStack->setCurrentIndex(1);
            const int row = codeEditorActionsTable->rowCount();
            codeEditorActionsTable->insertRow(row);
            auto* iconItem = new QTableWidgetItem();
            iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEditable);
            iconItem->setTextAlignment(Qt::AlignCenter);
            codeEditorActionsTable->setItem(row, 0, iconItem);
            codeEditorActionsTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("Action")));
            codeEditorActionsTable->setItem(row, 2, new QTableWidgetItem(QStringLiteral("axscript")));
            codeEditorActionsTable->setItem(row, 3, new QTableWidgetItem(QStringLiteral(
                "let p = editor.get_panel_data();\n"
                "editor.eval(editor.content(), { main: !!p.mainEngine });\n")));
            codeEditorActionsTable->selectRow(row);
            loadCodeEditorActionScriptFromRow(row);
            markDirty();
        });
    }
    if (codeEditorActionRemoveBtn) {
        connect(codeEditorActionRemoveBtn, &QPushButton::clicked, this, [this, markDirty]() {
            if (!codeEditorActionsTable)
                return;
            const int row = codeEditorActionsTable->currentRow();
            if (row < 0)
                return;
            codeEditorActionsTable->removeRow(row);
            const int next = qMin(row, codeEditorActionsTable->rowCount() - 1);
            if (next >= 0) {
                codeEditorActionsTable->selectRow(next);
                loadCodeEditorActionScriptFromRow(next);
            } else {
                if (codeEditorCustomStack)
                    codeEditorCustomStack->setCurrentIndex(0);
                loadCodeEditorActionScriptFromRow(-1);
            }
            markDirty();
        });
    }
    if (codeEditorActionsTable) {
        connect(codeEditorActionsTable, &QTableWidget::itemChanged, this, [markDirty](QTableWidgetItem*) {
            markDirty();
        });
        connect(codeEditorActionsTable, &QTableWidget::currentCellChanged, this,
                [this](int row, int, int prevRow, int) {
                    if (row == prevRow)
                        return;
                    if (prevRow >= 0)
                        syncCodeEditorActionScriptToTable(prevRow);
                    loadCodeEditorActionScriptFromRow(row);
                });
    }
    if (codeEditorActionScriptEdit) {
        connect(codeEditorActionScriptEdit, &QPlainTextEdit::textChanged, this, [this, markDirty]() {
            if (codeEditorActionScriptLoading || codeEditorLoading)
                return;
            syncCodeEditorActionScriptToTable(codeEditorActionEditRow);
            markDirty();
        });
    }
    if (codeEditorActionLabelEdit) {
        connect(codeEditorActionLabelEdit, &QLineEdit::textEdited, this, [this, markDirty](const QString&) {
            if (codeEditorActionScriptLoading || codeEditorLoading)
                return;
            syncCodeEditorActionScriptToTable(codeEditorActionEditRow);
            markDirty();
        });
    }
    for (QCheckBox* cb : {
             codeEditorTbNewFile, codeEditorTbOpenFile, codeEditorTbOpenFolder, codeEditorTbSave,
             codeEditorTbExplorer, codeEditorTbBuildLog, codeEditorTbMinimap, codeEditorTbWordWrap
         }) {
        if (cb)
            connect(cb, &QCheckBox::toggled, this, [markDirty](bool) { markDirty(); });
    }
    if (codeEditorActionIconBtn)
        connect(codeEditorActionIconBtn, &QPushButton::clicked, this, [this, markDirty]() {
            pickCodeEditorActionIcon();
            markDirty();
        });

    sessionsWidget = new QWidget(this);
    sessionsLayout = new QGridLayout(sessionsWidget);

    sessionsViewLabel = new QLabel("View mode:", sessionsWidget);
    sessionsViewLabel->setStyleSheet("font-weight: 500;");
    sessionsViewCombo = new QComboBox(sessionsWidget);
    sessionsViewCombo->addItem("Table (classic)");
    sessionsViewCombo->addItem("Feed (activity stream)");
    sessionsViewCombo->setToolTip("Choose how sessions are displayed. Requires reconnect/reopen of the project.");
    sessionsViewCombo->setMinimumWidth(180);
    connect(sessionsViewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (sessionsCompactSwitch)
            sessionsCompactSwitch->setEnabled(index == 1);
        buttonApply->setEnabled(true);
    });

    sessionsAutoHideInactiveSwitch = new oclero::qlementine::Switch(sessionsWidget);
    sessionsAutoHideInactiveSwitch->setText("Auto hide inactive");
    sessionsAutoHideInactiveSwitch->setToolTip("When enabled, sessions with status Terminated/Inactive/Disconnect are hidden by default.");
    sessionsCompactSwitch = new oclero::qlementine::Switch(sessionsWidget);
    sessionsCompactSwitch->setText("Compact mode");
    sessionsCompactSwitch->setToolTip("Single-line feed rows for Sessions (Feed mode only).");

    auto* sessionsViewRow = new QHBoxLayout();
    sessionsViewRow->setContentsMargins(0, 0, 0, 0);
    sessionsViewRow->setSpacing(16);
    sessionsViewRow->addWidget(sessionsViewLabel);
    sessionsViewRow->addWidget(sessionsViewCombo);
    sessionsViewRow->addStretch();
    sessionsLayout->addLayout(sessionsViewRow, 0, 0, 1, 1);

    auto* sessionsPrefsRow = new QHBoxLayout();
    sessionsPrefsRow->setContentsMargins(0, 0, 0, 0);
    sessionsPrefsRow->setSpacing(16);
    sessionsPrefsRow->addWidget(sessionsAutoHideInactiveSwitch);
    sessionsPrefsRow->addWidget(sessionsCompactSwitch);
    sessionsPrefsRow->addStretch();
    sessionsLayout->addLayout(sessionsPrefsRow, 1, 0, 1, 1);

    sessionsGroup = new QGroupBox("Visible fields / columns", sessionsWidget);
    sessionsGroup->setToolTip("Controls table columns and feed card blocks.");

    QStringList sessionsCheckboxLabels = {
        "Icon", "Agent ID", "Agent Type", "External", "Listener", "Internal",
        "Domain", "Computer", "User", "OS", "Process",
        "PID", "TID", "Tags", "Created", "Last", "Sleep"
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
    identityLayout->addWidget(sessionsCheck[0]);  // Icon
    identityLayout->addWidget(sessionsCheck[1]);  // Agent ID
    identityLayout->addWidget(sessionsCheck[2]);  // Agent Type
    identityLayout->addWidget(sessionsCheck[14]); // Created
    identityLayout->addStretch();
    topRow->addWidget(identityGroup, 1);

    auto* mainGroup = new QGroupBox("Main", sessionsGroup);
    auto* mainLayout = new QVBoxLayout(mainGroup);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->addWidget(sessionsCheck[8]); // User
    mainLayout->addWidget(sessionsCheck[7]); // Computer
    mainLayout->addWidget(sessionsCheck[6]); // Domain
    mainLayout->addWidget(sessionsCheck[5]); // Internal
    mainLayout->addStretch();
    topRow->addWidget(mainGroup, 1);

    auto* statusGroup = new QGroupBox("Status", sessionsGroup);
    auto* statusLayout = new QVBoxLayout(statusGroup);
    statusLayout->setContentsMargins(8, 8, 8, 8);
    statusLayout->addWidget(sessionsCheck[15]); // Last
    statusLayout->addWidget(sessionsCheck[16]); // Sleep
    statusLayout->addStretch();
    topRow->addWidget(statusGroup, 1);

    sessionsGroupLayout->addLayout(topRow);

    auto* detailsGroup = new QGroupBox("Details", sessionsGroup);
    auto* detailsLayout = new QGridLayout(detailsGroup);
    detailsLayout->setContentsMargins(8, 8, 8, 8);
    detailsLayout->addWidget(sessionsCheck[4], 0, 0); // Listener
    detailsLayout->addWidget(sessionsCheck[3], 0, 1); // External
    detailsLayout->addWidget(sessionsCheck[9], 0, 2); // OS
    detailsLayout->addWidget(sessionsCheck[10], 1, 0); // Process
    detailsLayout->addWidget(sessionsCheck[11], 1, 1); // PID
    detailsLayout->addWidget(sessionsCheck[12], 1, 2); // TID
    sessionsGroupLayout->addWidget(detailsGroup);

    auto* tagsGroup = new QGroupBox("Tags", sessionsGroup);
    auto* tagsLayout = new QHBoxLayout(tagsGroup);
    tagsLayout->setContentsMargins(8, 8, 8, 8);
    tagsLayout->addWidget(sessionsCheck[13]); // Tags
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

    healthGroupLayout->setContentsMargins(12, 12, 12, 12);
    healthGroupLayout->setHorizontalSpacing(12);
    healthGroupLayout->setVerticalSpacing(10);
    healthGroupLayout->addWidget(sessionsHealthCheck,      0, 0, 1, 4);
    healthGroupLayout->addWidget(sessionsLabel1,           1, 0, 1, 1);
    healthGroupLayout->addWidget(sessionsCoafSpin,        1, 1, 1, 1, Qt::AlignLeft);
    healthGroupLayout->addWidget(sessionsLabel2,          1, 2, 1, 1, Qt::AlignCenter);
    healthGroupLayout->addWidget(sessionsOffsetSpin,      1, 3, 1, 1, Qt::AlignLeft);
    healthGroupLayout->addWidget(sessionsDeadShiftLabel,  2, 0, 1, 1);
    healthGroupLayout->addWidget(sessionsDeadShiftSpin,   2, 1, 1, 1, Qt::AlignLeft);
    healthGroupLayout->setColumnStretch(4, 1);
    healthGroup->setLayout(healthGroupLayout);
    healthGroup->setStyleSheet(kSettingsCardCss);

    auto* graphGroup = new QGroupBox("Session Graph", sessionsWidget);
    graphGroup->setStyleSheet(kSettingsCardCss);
    auto* graphGroupLayout = new QGridLayout(graphGroup);
    applySettingsFormGrid(graphGroupLayout);
    styleSettingsLabel(graphLabel1);
    limitFieldWidth(graphCombo1, 200, 320);
    graphGroupLayout->addWidget(graphLabel1, 0, 0, 1, 1);
    graphGroupLayout->addWidget(graphCombo1, 0, 1, 1, 1, Qt::AlignLeft);

    graphAutoHideInactiveSwitch = new oclero::qlementine::Switch(graphGroup);
    graphAutoHideInactiveSwitch->setText("Auto hide inactive");
    graphAutoHideInactiveSwitch->setToolTip("Hide inactive/terminated sessions from the graph by default.");

    graphAutoHideNoChildsSwitch = new oclero::qlementine::Switch(graphGroup);
    graphAutoHideNoChildsSwitch->setText("Auto hide without children");
    graphAutoHideNoChildsSwitch->setToolTip("Hide sessions that have no child sessions in the graph by default.");

    graphGroupLayout->addWidget(graphAutoHideInactiveSwitch, 1, 0, 1, 2);
    graphGroupLayout->addWidget(graphAutoHideNoChildsSwitch, 2, 0, 1, 2);
    graphGroup->setLayout(graphGroupLayout);

    sessionsLayout->addWidget(sessionsGroup,  2, 0, 1, 1);
    sessionsLayout->addWidget(healthGroup,    3, 0, 1, 1);
    sessionsLayout->addWidget(graphGroup,     4, 0, 1, 1);
    sessionsLayout->setRowStretch(5, 1);

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
    targetsViewLabel = new QLabel("View mode:", targetsWidget);
    targetsViewLabel->setStyleSheet("font-weight: 500;");
    targetsViewCombo = new QComboBox(targetsWidget);
    targetsViewCombo->addItem("Table (classic)");
    targetsViewCombo->addItem("Feed (activity stream)");
    targetsViewCombo->setToolTip("Choose how targets are displayed. Requires reconnect/reopen of the project.");
    targetsViewCombo->setMinimumWidth(180);
    connect(targetsViewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (targetsCompactSwitch)
            targetsCompactSwitch->setEnabled(index == 1);
        buttonApply->setEnabled(true);
    });
    targetsCompactSwitch = new oclero::qlementine::Switch(targetsWidget);
    targetsCompactSwitch->setText("Compact mode");
    targetsCompactSwitch->setToolTip("Single-line feed rows for Targets (Feed mode only).");
    auto* targetsPrefsRow = new QHBoxLayout();
    targetsPrefsRow->setContentsMargins(0, 0, 0, 0);
    targetsPrefsRow->setSpacing(16);
    targetsPrefsRow->addWidget(targetsViewLabel);
    targetsPrefsRow->addWidget(targetsViewCombo);
    targetsPrefsRow->addSpacing(16);
    targetsPrefsRow->addWidget(targetsCompactSwitch);
    targetsPrefsRow->addStretch();
    targetsLayout->addLayout(targetsPrefsRow, 0, 0, 1, 1);

    targetsGroup = new QGroupBox("Visible fields / columns", targetsWidget);
    targetsGroup->setToolTip("Controls table columns and feed card blocks.");

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
    credsViewLabel = new QLabel("View mode:", credsWidget);
    credsViewLabel->setStyleSheet("font-weight: 500;");
    credsViewCombo = new QComboBox(credsWidget);
    credsViewCombo->addItem("Table (classic)");
    credsViewCombo->addItem("Feed (activity stream)");
    credsViewCombo->setToolTip("Choose how credentials are displayed. Requires reconnect/reopen of the project.");
    credsViewCombo->setMinimumWidth(180);
    connect(credsViewCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (credsCompactSwitch)
            credsCompactSwitch->setEnabled(index == 1);
        buttonApply->setEnabled(true);
    });
    credsCompactSwitch = new oclero::qlementine::Switch(credsWidget);
    credsCompactSwitch->setText("Compact mode");
    credsCompactSwitch->setToolTip("Single-line feed rows for Credentials (Feed mode only).");
    auto* credsPrefsRow = new QHBoxLayout();
    credsPrefsRow->setContentsMargins(0, 0, 0, 0);
    credsPrefsRow->setSpacing(16);
    credsPrefsRow->addWidget(credsViewLabel);
    credsPrefsRow->addWidget(credsViewCombo);
    credsPrefsRow->addSpacing(16);
    credsPrefsRow->addWidget(credsCompactSwitch);
    credsPrefsRow->addStretch();
    credsLayout->addLayout(credsPrefsRow, 0, 0, 1, 1);

    credsGroup = new QGroupBox("Visible fields / columns", credsWidget);
    credsGroup->setToolTip("Controls table columns and feed card blocks.");

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

    payloadsWidget = new QWidget(this);
    payloadsLayout = new QGridLayout(payloadsWidget);
    payloadsGroup = new QGroupBox("Visible columns", payloadsWidget);
    payloadsGroup->setToolTip("Columns shown in the Payload Store table.");

    QStringList payloadsCheckboxLabels = {
        "ID", "Name", "Description", "Type", "Artifact", "Listener(s)",
        "Size", "Creator", "Created", "UID", "Tag", "MD5", "SHA1", "SHA256"
    };
    for (int i = 0; i < payloadsCheckCount; ++i)
        payloadsCheck[i] = new QCheckBox(payloadsCheckboxLabels[i], payloadsGroup);

    auto* payloadsGroupLayout = new QVBoxLayout(payloadsGroup);
    payloadsGroupLayout->setSpacing(8);
    auto* payloadsTopRow = new QHBoxLayout();
    payloadsTopRow->setSpacing(8);

    auto* payloadsIdentityGroup = new QGroupBox("Identity", payloadsGroup);
    auto* payloadsIdentityLayout = new QVBoxLayout(payloadsIdentityGroup);
    payloadsIdentityLayout->setContentsMargins(8, 8, 8, 8);
    payloadsIdentityLayout->addWidget(payloadsCheck[0]);  // ID
    payloadsIdentityLayout->addWidget(payloadsCheck[1]);  // Name
    payloadsIdentityLayout->addWidget(payloadsCheck[2]);  // Description
    payloadsIdentityLayout->addWidget(payloadsCheck[10]); // Tag
    payloadsIdentityLayout->addWidget(payloadsCheck[9]);  // UID (hidden by default)
    payloadsIdentityLayout->addStretch();
    payloadsTopRow->addWidget(payloadsIdentityGroup);

    auto* payloadsMainGroup = new QGroupBox("Payload", payloadsGroup);
    auto* payloadsMainLayout = new QVBoxLayout(payloadsMainGroup);
    payloadsMainLayout->setContentsMargins(8, 8, 8, 8);
    payloadsMainLayout->addWidget(payloadsCheck[3]);  // Type
    payloadsMainLayout->addWidget(payloadsCheck[4]);  // Artifact
    payloadsMainLayout->addWidget(payloadsCheck[5]);  // Listener(s)
    payloadsMainLayout->addWidget(payloadsCheck[6]);  // Size
    payloadsMainLayout->addWidget(payloadsCheck[7]);  // Creator
    payloadsMainLayout->addWidget(payloadsCheck[8]);  // Created
    payloadsMainLayout->addStretch();
    payloadsTopRow->addWidget(payloadsMainGroup);

    auto* payloadsHashGroup = new QGroupBox("Hashes", payloadsGroup);
    auto* payloadsHashLayout = new QVBoxLayout(payloadsHashGroup);
    payloadsHashLayout->setContentsMargins(8, 8, 8, 8);
    payloadsHashLayout->addWidget(payloadsCheck[11]); // MD5
    payloadsHashLayout->addWidget(payloadsCheck[12]); // SHA1
    payloadsHashLayout->addWidget(payloadsCheck[13]); // SHA256
    payloadsHashLayout->addStretch();
    payloadsTopRow->addWidget(payloadsHashGroup);

    payloadsGroupLayout->addLayout(payloadsTopRow);
    payloadsGroup->setLayout(payloadsGroupLayout);
    payloadsLayout->addWidget(payloadsGroup, 0, 0, 1, 1);
    payloadsLayout->setRowStretch(1, 1);
    payloadsWidget->setLayout(payloadsLayout);

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
        if (!isDockContentBlinkAllowed(info.className))
            continue;
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
    addRow("Ctrl+PgUp",          "Main UI",    "Navigate to previous dock");
    addRow("Ctrl+PgDown",        "Main UI",    "Navigate to next dock");
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

    scriptSecWidget = new QWidget(this);
    scriptSecLayout = new QVBoxLayout(scriptSecWidget);
    scriptSecLayout->setContentsMargins(0, 0, 0, 0);
    scriptSecLayout->setSpacing(12);

    auto makePolicyGroup = [&](const QString& title, const QString& hint, oclero::qlementine::Switch** readSw, oclero::qlementine::Switch** writeSw, oclero::qlementine::Switch** processSw, oclero::qlementine::Switch** sandboxSw, bool processEnabled) {
        auto* box = new QGroupBox(title, scriptSecWidget);
        auto* lay = new QVBoxLayout(box);
        lay->setSpacing(6);
        if (!hint.isEmpty()) {
            auto* h = new QLabel(hint, box);
            h->setWordWrap(true);
            h->setStyleSheet(QStringLiteral("color: palette(placeholderText); font-size: 11px;"));
            lay->addWidget(h);
        }
        auto addSw = [&](oclero::qlementine::Switch** out, const QString& text, bool enabled = true) {
            auto* sw = new oclero::qlementine::Switch(box);
            sw->setText(text);
            sw->setEnabled(enabled);
            if (!enabled)
                sw->setChecked(false);
            lay->addWidget(sw);
            *out = sw;
        };
        addSw(readSw, QStringLiteral("File read (ax.file_read)"));
        addSw(writeSw, QStringLiteral("File write (ax.file_write)"));
        addSw(processSw, QStringLiteral("Process exec (process.exec)"), processEnabled);
        if (!processEnabled && processSw && *processSw)
            (*processSw)->setToolTip(QStringLiteral("Not available for this context (hard-limited)"));
        addSw(sandboxSw, QStringLiteral("Sandbox filesystem (restrict paths)"));
        return box;
    };

    scriptSecLayout->addWidget(makePolicyGroup(
        QStringLiteral("Server scripts"),
        QStringLiteral("Delivered from the teamserver."),
        &scriptServerRead, &scriptServerWrite, &scriptServerProcess, &scriptServerSandbox, false));
    scriptSecLayout->addWidget(makePolicyGroup(
        QStringLiteral("Local scripts"),
        QStringLiteral("Main engine, user extensions, extender ax_config."),
        &scriptLocalRead, &scriptLocalWrite, &scriptLocalProcess, &scriptLocalSandbox, false));
    scriptSecLayout->addWidget(makePolicyGroup(
        QStringLiteral("Code Editor — Run / Panel"),
        QStringLiteral("GeneratePanel and Run (isolated or main engine)."),
        &scriptEditorRead, &scriptEditorWrite, &scriptEditorProcess, &scriptEditorSandbox, false));
    scriptSecLayout->addWidget(makePolicyGroup(
        QStringLiteral("Code Editor — Toolbar actions"),
        QStringLiteral("AxScript handlers on profile toolbar buttons. Only this context may run process.exec."),
        &scriptActionRead, &scriptActionWrite, &scriptActionProcess, &scriptActionSandbox, true));

    auto* sandBox = new QGroupBox(QStringLiteral("Sandbox directory"), scriptSecWidget);
    sandBox->setStyleSheet(kSettingsCardCss);
    auto* sandLay = new QGridLayout(sandBox);
    applySettingsFormGrid(sandLay);
    auto* sandLabel = new QLabel(QStringLiteral("Root:"), sandBox);
    styleSettingsLabel(sandLabel);
    scriptSandboxDirEdit = new QLineEdit(sandBox);
    scriptSandboxDirEdit->setPlaceholderText(QStringLiteral("~/.adaptix/script_sandbox"));
    scriptSandboxDirEdit->setToolTip(QStringLiteral(
        "Absolute path or ~/… / ~\\… (home expansion works on Windows too).\n"
        "Relative file paths are mapped under this root.\n"
        "Local scripts may also read/write their own script directory when sandbox is on."));
    limitFieldWidth(scriptSandboxDirEdit, 280, 480);
    sandLay->addWidget(sandLabel, 0, 0);
    sandLay->addWidget(scriptSandboxDirEdit, 0, 1, Qt::AlignLeft);
    sandLay->setColumnStretch(2, 1);
    scriptSecLayout->addWidget(sandBox);

    auto* note = new QLabel(
        QStringLiteral("Policy changes apply to newly created script engines (reload scripts / restart action). "
                       "process.exec is hard-limited to Code Editor toolbar actions even if other toggles appear."),
        scriptSecWidget);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: palette(placeholderText); font-size: 11px;"));
    scriptSecLayout->addWidget(note);
    scriptSecLayout->addStretch(1);

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
        label->setStyleSheet(QStringLiteral(
            "QLabel#SidebarSectionLabel {"
            "  color: palette(highlight);"
            "  font-size: %1px;"
            "  font-weight: 700;"
            "  letter-spacing: 1px;"
            "  padding: 6px 8px 2px 8px;"
            "  border-bottom: 1px solid palette(mid);"
            "}"
        ).arg(FontManager::instance().typography().captionFontPx));
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
    addNavItem("Code Editor");
    addSectionHeader("SECURITY");
    addNavItem("AxScript");
    addSectionHeader("DATA");
    addNavItem("Sessions");
    addNavItem("Tasks");
    addNavItem("Targets");
    addNavItem("Credentials");
    addNavItem("Files");
    addNavItem("Payloads");
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
    QFont headerFont = FontManager::instance().typography().primary;
    headerFont.setPointSize(FontManager::instance().typography().baseSize + 4);
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

    auto markScriptDirty = [this](bool) { buttonApply->setEnabled(true); };
    for (auto* sw : {scriptServerRead, scriptServerWrite, scriptServerProcess, scriptServerSandbox,
                     scriptLocalRead, scriptLocalWrite, scriptLocalProcess, scriptLocalSandbox,
                     scriptEditorRead, scriptEditorWrite, scriptEditorProcess, scriptEditorSandbox,
                     scriptActionRead, scriptActionWrite, scriptActionProcess, scriptActionSandbox}) {
        if (sw)
            connect(sw, &oclero::qlementine::Switch::toggled, buttonApply, markScriptDirty);
    }
    if (scriptSandboxDirEdit)
        connect(scriptSandboxDirEdit, &QLineEdit::textEdited, buttonApply, [this](const QString&){ buttonApply->setEnabled(true); });

    stackSettings = new QStackedWidget(this);
    stackSettings->addWidget(appearanceWidget);
    stackSettings->addWidget(consolePageWidget);
    stackSettings->addWidget(codeEditorWidget);
    stackSettings->addWidget(scriptSecWidget);
    stackSettings->addWidget(sessionsWidget);
    stackSettings->addWidget(tasksWidget);
    stackSettings->addWidget(targetsWidget);
    stackSettings->addWidget(credsWidget);
    stackSettings->addWidget(filesWidget);
    stackSettings->addWidget(payloadsWidget);
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

void DialogSettings::refreshCodeEditorProfilesList()
{
    if (!codeEditorProfileList)
        return;
    codeEditorLoading = true;
    const QString prevId = codeEditorProfileList->currentItem()
        ? codeEditorProfileList->currentItem()->data(Qt::UserRole).toString()
        : codeEditorEditingId;
    codeEditorProfileList->clear();
    auto* mgr = CodeEditorProfileManager::instance();
    for (const auto& p : mgr->profiles()) {
        auto* item = new QListWidgetItem(p.name, codeEditorProfileList);
        item->setData(Qt::UserRole, p.id);
        if (p.isSystem())
            item->setToolTip(QStringLiteral("System profile · %1").arg(p.id));
        else if (p.isManaged())
            item->setToolTip(QStringLiteral("Managed (AxScript) · %1").arg(p.id));
        else
            item->setToolTip(QStringLiteral("User profile · %1").arg(p.id));
    }
    QListWidgetItem* select = nullptr;
    if (!prevId.isEmpty()) {
        for (int i = 0; i < codeEditorProfileList->count(); ++i) {
            auto* it = codeEditorProfileList->item(i);
            if (it && (it->data(Qt::UserRole).toString() == prevId || it->text() == prevId)) {
                select = it;
                break;
            }
        }
    }
    if (!select && codeEditorProfileList->count() > 0)
        select = codeEditorProfileList->item(0);
    if (select)
        codeEditorProfileList->setCurrentItem(select);
    codeEditorLoading = false;
    if (select) {
        const QString key = select->data(Qt::UserRole).toString();
        loadCodeEditorProfileToForm(key.isEmpty() ? select->text() : key);
    }
}

void DialogSettings::loadCodeEditorProfileToForm(const QString& name)
{
    if (!codeEditorNameEdit)
        return;
    codeEditorLoading = true;
    auto* mgr = CodeEditorProfileManager::instance();
    const BuildProfile* p = mgr->profile(name);
    if (!p) {
        codeEditorEditingId.clear();
        codeEditorLoading = false;
        return;
    }

    codeEditorEditingId = p->id;
    codeEditorNameEdit->setText(p->name);
    codeEditorNameEdit->setReadOnly(!p->isRenameable());
    codeEditorNameEdit->setToolTip(p->isRenameable()
        ? QStringLiteral("Profile display name (id: %1)").arg(p->id)
        : (p->isSystem()
               ? QStringLiteral("System profile name cannot be changed")
               : QStringLiteral("Managed profile name is set by AxScript (id: %1)").arg(p->id)));

    if (codeEditorLanguageCombo) {
        const int li = codeEditorLanguageCombo->findData(p->language);
        codeEditorLanguageCombo->setCurrentIndex(li >= 0 ? li : 0);
    }
    if (codeEditorPanelEnabledSwitch) {
        const QString panel = p->toolbar.panel;
        const bool on = (panel != QLatin1String("none"));
        QSignalBlocker b(codeEditorPanelEnabledSwitch);
        codeEditorPanelEnabledSwitch->setChecked(on);
    }

    if (codeEditorPanelScriptEdit)
        codeEditorPanelScriptEdit->setPlainText(p->panelScript);

    if (codeEditorActionsTable) {
        codeEditorActionsTable->blockSignals(true);
        codeEditorActionsTable->setRowCount(0);
        for (const BuildProfileAction& a : p->customActions) {
            const int row = codeEditorActionsTable->rowCount();
            codeEditorActionsTable->insertRow(row);
            auto* iconItem = new QTableWidgetItem();
            iconItem->setData(Qt::UserRole, a.icon);
            iconItem->setToolTip(a.icon);
            if (!a.icon.isEmpty())
                iconItem->setIcon(QIcon(a.icon));
            iconItem->setTextAlignment(Qt::AlignCenter);
            iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEditable);
            codeEditorActionsTable->setItem(row, 0, iconItem);
            codeEditorActionsTable->setItem(row, 1, new QTableWidgetItem(a.label));
            codeEditorActionsTable->setItem(row, 2, new QTableWidgetItem(QStringLiteral("axscript")));
            codeEditorActionsTable->setItem(row, 3, new QTableWidgetItem(a.script));
        }
        codeEditorActionsTable->blockSignals(false);
        if (codeEditorCustomStack)
            codeEditorCustomStack->setCurrentIndex(codeEditorActionsTable->rowCount() > 0 ? 1 : 0);
        if (codeEditorActionsTable->rowCount() > 0) {
            codeEditorActionsTable->selectRow(0);
            loadCodeEditorActionScriptFromRow(0);
        } else {
            loadCodeEditorActionScriptFromRow(-1);
        }
    }

    const BuildProfileToolbar& t = p->toolbar;
    auto setCb = [](QCheckBox* cb, bool on) {
        if (cb) cb->setChecked(on);
    };
    setCb(codeEditorTbNewFile, t.newFile);
    setCb(codeEditorTbOpenFile, t.openFile);
    setCb(codeEditorTbOpenFolder, t.openFolder);
    setCb(codeEditorTbSave, t.save);
    setCb(codeEditorTbExplorer, t.explorer);
    setCb(codeEditorTbBuildLog, t.buildLog);
    setCb(codeEditorTbMinimap, t.minimap);
    setCb(codeEditorTbWordWrap, t.wordWrap);

    if (codeEditorRemoveBtn)
        codeEditorRemoveBtn->setEnabled(p->isDeletable());

    setCodeEditorFormEditable(p->isEditable());
    updateCodeEditorPanelScriptVisibility();
    codeEditorLoading = false;
}

void DialogSettings::setCodeEditorFormEditable(bool editable)
{
    if (codeEditorNameEdit)
        codeEditorNameEdit->setReadOnly(!editable);
    if (codeEditorLanguageCombo)
        codeEditorLanguageCombo->setEnabled(editable);
    if (codeEditorPanelEnabledSwitch)
        codeEditorPanelEnabledSwitch->setEnabled(editable);
    if (codeEditorPanelScriptEdit)
        codeEditorPanelScriptEdit->setReadOnly(!editable);
    if (codeEditorPanelScriptTplBtn)
        codeEditorPanelScriptTplBtn->setEnabled(editable && codeEditorPanelEnabledSwitch && codeEditorPanelEnabledSwitch->isChecked());
    if (codeEditorResetDefaultsBtn)
        codeEditorResetDefaultsBtn->setEnabled(editable);
    if (codeEditorActionAddBtn)
        codeEditorActionAddBtn->setEnabled(editable);
    if (codeEditorActionRemoveBtn)
        codeEditorActionRemoveBtn->setEnabled(editable);
    if (codeEditorActionScriptEdit)
        codeEditorActionScriptEdit->setReadOnly(!editable);
    const bool hasRow = codeEditorActionsTable && codeEditorActionsTable->currentRow() >= 0;
    if (codeEditorActionLabelEdit) {
        codeEditorActionLabelEdit->setEnabled(hasRow);
        codeEditorActionLabelEdit->setReadOnly(!editable);
    }
    if (codeEditorActionIconBtn) {
        codeEditorActionIconBtn->setEnabled(editable && hasRow);
        codeEditorActionIconBtn->setToolTip(
            editable
                ? QStringLiteral("Choose icon from client resources (:/icons)")
                : QStringLiteral("System profile — icons are locked"));
    }
    if (codeEditorActionsTable)
        codeEditorActionsTable->setEditTriggers(
            editable ? (QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
                        | QAbstractItemView::AnyKeyPressed)
                     : QAbstractItemView::NoEditTriggers);

    for (QCheckBox* cb : {
             codeEditorTbNewFile, codeEditorTbOpenFile, codeEditorTbOpenFolder, codeEditorTbSave,
             codeEditorTbExplorer, codeEditorTbBuildLog, codeEditorTbMinimap, codeEditorTbWordWrap
         }) {
        if (cb)
            cb->setEnabled(editable);
    }

}

void DialogSettings::saveCodeEditorProfileFromForm()
{
    if (codeEditorEditingId.isEmpty())
        return;
    syncCodeEditorActionScriptToTable(codeEditorActionEditRow);
    auto* mgr = CodeEditorProfileManager::instance();
    const BuildProfile* cur = mgr->profile(codeEditorEditingId);
    if (!cur)
        return;
    if (cur->isSystem())
        return;

    QString newName = codeEditorNameEdit ? codeEditorNameEdit->text().trimmed() : cur->name;
    if (newName.isEmpty())
        newName = cur->name;

    if (newName != cur->name) {
        if (!cur->isRenameable()) {
            if (codeEditorNameEdit)
                codeEditorNameEdit->setText(cur->name);
        } else {
            const QString stableId = codeEditorEditingId;
            QString finalName;
            if (mgr->renameProfile(stableId, newName, &finalName) && !finalName.isEmpty()) {
                if (codeEditorProfileList) {
                    QSignalBlocker block(codeEditorProfileList);
                    for (int i = 0; i < codeEditorProfileList->count(); ++i) {
                        if (auto* item = codeEditorProfileList->item(i)) {
                            if (item->data(Qt::UserRole).toString() == stableId) {
                                item->setText(finalName);
                                break;
                            }
                        }
                    }
                }
                if (codeEditorNameEdit && codeEditorNameEdit->text().trimmed() != finalName)
                    codeEditorNameEdit->setText(finalName);
                cur = mgr->profile(stableId);
                if (!cur)
                    return;
            } else {
                if (codeEditorNameEdit)
                    codeEditorNameEdit->setText(cur->name);
                cur = mgr->profile(codeEditorEditingId);
                if (!cur)
                    return;
            }
        }
    }

    if (cur->isSystem())
        return;

    BuildProfile p = *cur;
    if (codeEditorLanguageCombo)
        p.language = codeEditorLanguageCombo->currentData().toString();

    BuildProfileToolbar& t = p.toolbar;
    auto getCb = [](QCheckBox* cb, bool def) { return cb ? cb->isChecked() : def; };
    t.newFile = getCb(codeEditorTbNewFile, t.newFile);
    t.openFile = getCb(codeEditorTbOpenFile, t.openFile);
    t.openFolder = getCb(codeEditorTbOpenFolder, t.openFolder);
    t.save = getCb(codeEditorTbSave, t.save);
    t.explorer = getCb(codeEditorTbExplorer, t.explorer);
    t.buildLog = getCb(codeEditorTbBuildLog, t.buildLog);
    t.minimap = getCb(codeEditorTbMinimap, t.minimap);
    t.wordWrap = getCb(codeEditorTbWordWrap, t.wordWrap);
    if (codeEditorPanelEnabledSwitch)
        t.panel = codeEditorPanelEnabledSwitch->isChecked()
            ? QStringLiteral("axscript")
            : QStringLiteral("none");
    else
        t.panel = QStringLiteral("axscript");

    if (codeEditorPanelScriptEdit)
        p.panelScript = codeEditorPanelScriptEdit->toPlainText();

    p.customActions.clear();
    if (codeEditorActionsTable) {
        for (int row = 0; row < codeEditorActionsTable->rowCount(); ++row) {
            BuildProfileAction a;
            auto cell = [&](int col) -> QString {
                if (auto* it = codeEditorActionsTable->item(row, col))
                    return it->text();
                return {};
            };
            a.label  = cell(1).trimmed();
            if (auto* iconIt = codeEditorActionsTable->item(row, 0)) {
                a.icon = iconIt->data(Qt::UserRole).toString();
                if (a.icon.isEmpty())
                    a.icon = iconIt->text().trimmed();
            }
            a.script = cell(3);
            a.id = a.label.isEmpty()
                ? QStringLiteral("action_%1").arg(row + 1)
                : a.label.toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9_]+")), QStringLiteral("_"));
            if (a.label.isEmpty() && a.script.trimmed().isEmpty())
                continue;
            p.customActions.append(a);
        }
    }

    mgr->updateProfile(p);
}

void DialogSettings::updateCodeEditorPanelScriptVisibility()
{
    const bool axPanel = codeEditorPanelEnabledSwitch
        ? codeEditorPanelEnabledSwitch->isChecked()
        : true;

    if (codeEditorPanelScriptHint) {
        codeEditorPanelScriptHint->setText(axPanel
            ? QStringLiteral("Return { ui_panel, ui_container }. Toolbar handlers use global editor.")
            : QStringLiteral("Panel is off — enable to edit GeneratePanel()."));
    }
    bool editable = true;
    if (auto* mgr = CodeEditorProfileManager::instance()) {
        if (const BuildProfile* p = mgr->profile(codeEditorEditingId))
            editable = p->isEditable();
    }
    if (codeEditorPanelScriptEdit) {
        codeEditorPanelScriptEdit->setEnabled(axPanel);
        codeEditorPanelScriptEdit->setReadOnly(!editable);
    }
    if (codeEditorPanelScriptTplBtn)
        codeEditorPanelScriptTplBtn->setEnabled(editable && axPanel);
    if (auto* globals = codeEditorTabs
            ? codeEditorTabs->findChild<QLabel*>(QStringLiteral("CePanelGlobals"))
            : nullptr) {
        globals->setVisible(axPanel);
    }
}

void DialogSettings::syncCodeEditorActionScriptToTable(int row)
{
    if (!codeEditorActionsTable || codeEditorActionScriptLoading)
        return;
    if (row < 0)
        row = codeEditorActionEditRow;
    if (row < 0 || row >= codeEditorActionsTable->rowCount())
        return;

    QSignalBlocker blockTable(codeEditorActionsTable);

    auto ensureItem = [&](int col) -> QTableWidgetItem* {
        auto* it = codeEditorActionsTable->item(row, col);
        if (!it) {
            it = new QTableWidgetItem();
            codeEditorActionsTable->setItem(row, col, it);
        }
        return it;
    };

    ensureItem(2)->setText(QStringLiteral("axscript"));

    if (auto* iconItem = ensureItem(0)) {
        iconItem->setText(QString());
        iconItem->setData(Qt::UserRole, codeEditorActionIconPath);
        iconItem->setToolTip(codeEditorActionIconPath);
        iconItem->setIcon(codeEditorActionIconPath.isEmpty() ? QIcon() : QIcon(codeEditorActionIconPath));
        iconItem->setTextAlignment(Qt::AlignCenter);
        iconItem->setFlags(iconItem->flags() & ~Qt::ItemIsEditable);
    }
    if (codeEditorActionLabelEdit)
        ensureItem(1)->setText(codeEditorActionLabelEdit->text());

    if (codeEditorActionScriptEdit)
        ensureItem(3)->setText(codeEditorActionScriptEdit->toPlainText());
}

void DialogSettings::updateCodeEditorActionTypeUi()
{
    if (codeEditorActionScriptHost)
        codeEditorActionScriptHost->setVisible(true);
    if (codeEditorActionBodyLabel)
        codeEditorActionBodyLabel->setText(QStringLiteral("AxScript handler"));
}

void DialogSettings::pickCodeEditorActionIcon()
{
    if (!codeEditorActionIconBtn)
        return;
    if (auto* mgr = CodeEditorProfileManager::instance()) {
        if (const BuildProfile* p = mgr->profile(codeEditorEditingId)) {
            if (!p->isEditable())
                return;
        }
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Choose icon — client resources"));
    dlg.resize(520, 500);
    auto* lay = new QVBoxLayout(&dlg);
    lay->setSpacing(8);

    auto* hint = new QLabel(QStringLiteral("Icons from AdaptixClient resources (:/icons)"), &dlg);
    hint->setStyleSheet(QStringLiteral("color: palette(placeholderText);"));
    lay->addWidget(hint);

    auto* filterEdit = new QLineEdit(&dlg);
    filterEdit->setPlaceholderText(QStringLiteral("Filter by name…"));
    filterEdit->setClearButtonEnabled(true);
    lay->addWidget(filterEdit);

    auto* list = new QListWidget(&dlg);
    list->setViewMode(QListView::IconMode);
    list->setFlow(QListView::LeftToRight);
    list->setWrapping(true);
    list->setResizeMode(QListWidget::Adjust);
    list->setMovement(QListView::Static);
    list->setUniformItemSizes(true);
    list->setSpacing(2);
    list->setIconSize(QSize(36, 36));
    list->setGridSize(QSize(100, 86));
    list->setWordWrap(true);
    list->setTextElideMode(Qt::ElideRight);
    list->setStyleSheet(QStringLiteral(
        "QListWidget {"
        "  outline: none;"
        "  background: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "  padding: 6px;"
        "}"
        "QListWidget::item {"
        "  color: palette(window-text);"
        "  padding: 4px 2px 2px 2px;"
        "  border-radius: 6px;"
        "}"
        "QListWidget::item:selected {"
        "  background: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"));

    auto makeItem = [](const QString& path, const QString& label) {
        auto* it = new QListWidgetItem(path.isEmpty() ? QIcon() : QIcon(path), label);
        it->setData(Qt::UserRole, path);
        it->setData(Qt::UserRole + 1, label);
        it->setToolTip(path.isEmpty() ? label : QStringLiteral("%1\n%2").arg(label, path));
        it->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        it->setFlags(it->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        return it;
    };

    list->addItem(makeItem(QString(), QStringLiteral("(none)")));
    for (const QString& path : BuildProfileAction::toolbarIconPaths()) {
        const QString label = path.section(QLatin1Char('/'), -1);
        list->addItem(makeItem(path, label));
    }
    lay->addWidget(list, 1);

    auto applyFilter = [list](const QString& text) {
        const QString q = text.trimmed();
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* it = list->item(i);
            if (!it)
                continue;
            if (q.isEmpty()) {
                it->setHidden(false);
                continue;
            }
            const QString label = it->data(Qt::UserRole + 1).toString();
            const QString path  = it->data(Qt::UserRole).toString();
            it->setHidden(!(label.contains(q, Qt::CaseInsensitive) || path.contains(q, Qt::CaseInsensitive)));
        }
    };
    connect(filterEdit, &QLineEdit::textChanged, &dlg, applyFilter);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    lay->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);

    filterEdit->setFocus(Qt::OtherFocusReason);
    if (dlg.exec() != QDialog::Accepted)
        return;
    auto* cur = list->currentItem();
    if (!cur || cur->isHidden())
        return;
    codeEditorActionIconPath = cur->data(Qt::UserRole).toString();
    codeEditorActionIconBtn->setIcon(codeEditorActionIconPath.isEmpty() ? QIcon() : QIcon(codeEditorActionIconPath));
    syncCodeEditorActionScriptToTable(codeEditorActionEditRow);
}

void DialogSettings::loadCodeEditorActionScriptFromRow(int row)
{
    codeEditorActionScriptLoading = true;
    codeEditorActionEditRow = row;

    bool editable = true;
    if (auto* mgr = CodeEditorProfileManager::instance()) {
        if (const BuildProfile* p = mgr->profile(codeEditorEditingId))
            editable = p->isEditable();
    }

    if (row < 0 || !codeEditorActionsTable || row >= codeEditorActionsTable->rowCount()) {
        if (codeEditorActionScriptEdit) {
            codeEditorActionScriptEdit->clear();
            codeEditorActionScriptEdit->setEnabled(false);
        }
        if (codeEditorActionLabelEdit) {
            codeEditorActionLabelEdit->clear();
            codeEditorActionLabelEdit->setEnabled(false);
        }
        if (codeEditorActionIconBtn) {
            codeEditorActionIconBtn->setEnabled(false);
            codeEditorActionIconBtn->setIcon(QIcon());
            codeEditorActionIconBtn->setToolTip(QStringLiteral("Choose icon from client resources (:/icons)"));
        }
        codeEditorActionIconPath.clear();
        if (codeEditorActionScriptHost)
            codeEditorActionScriptHost->setVisible(true);
    } else {
        codeEditorActionIconPath.clear();
        if (auto* it = codeEditorActionsTable->item(row, 0)) {
            codeEditorActionIconPath = it->data(Qt::UserRole).toString();
            if (codeEditorActionIconPath.isEmpty())
                codeEditorActionIconPath = it->text();
        }
        if (codeEditorActionIconBtn) {
            codeEditorActionIconBtn->setEnabled(editable);
            codeEditorActionIconBtn->setIcon(codeEditorActionIconPath.isEmpty()
                                                 ? QIcon()
                                                 : QIcon(codeEditorActionIconPath));
            codeEditorActionIconBtn->setToolTip(
                editable
                    ? QStringLiteral("Choose icon from client resources (:/icons)")
                    : QStringLiteral("System profile — icons are locked"));
        }

        QString label;
        if (auto* it = codeEditorActionsTable->item(row, 1))
            label = it->text();
        if (codeEditorActionLabelEdit) {
            codeEditorActionLabelEdit->setText(label);
            codeEditorActionLabelEdit->setEnabled(true);
            codeEditorActionLabelEdit->setReadOnly(!editable);
        }

        QString body;
        if (auto* it = codeEditorActionsTable->item(row, 3))
            body = it->text();
        if (codeEditorActionScriptEdit) {
            codeEditorActionScriptEdit->setPlainText(body);
            codeEditorActionScriptEdit->setEnabled(true);
            codeEditorActionScriptEdit->setReadOnly(!editable);
        }
        updateCodeEditorActionTypeUi();
    }
    codeEditorActionScriptLoading = false;
}

void DialogSettings::exportCodeEditorProfile()
{
    if (!codeEditorEditingId.isEmpty())
        saveCodeEditorProfileFromForm();

    auto* mgr = CodeEditorProfileManager::instance();
    const QString name = codeEditorEditingId.isEmpty()
        ? (codeEditorProfileList && codeEditorProfileList->currentItem()
               ? codeEditorProfileList->currentItem()->text()
               : QString())
        : codeEditorEditingId;
    const BuildProfile* p = mgr->profile(name);
    if (!p) {
        QMessageBox::information(this, QStringLiteral("Export Profile"),
            QStringLiteral("Select a profile to export."));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Export Code Editor Profile"),
        p->name + QStringLiteral(".json"),
        QStringLiteral("JSON files (*.json)"));
    if (path.isEmpty())
        return;

    QJsonObject root = p->toJson();
    root[QStringLiteral("format")] = QStringLiteral("adaptix.code_editor_profile");
    root[QStringLiteral("version")] = 1;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("Export Profile"),
            QStringLiteral("Cannot write file:\n%1").arg(path));
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
}

void DialogSettings::importCodeEditorProfile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Import Code Editor Profile"),
        QString(),
        QStringLiteral("JSON files (*.json)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("Import Profile"),
            QStringLiteral("Cannot read file:\n%1").arg(path));
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) {
        QMessageBox::warning(this, QStringLiteral("Import Profile"),
            QStringLiteral("Invalid profile JSON."));
        return;
    }

    BuildProfile imported = BuildProfile::fromJson(doc.object());
    if (imported.name.trimmed().isEmpty())
        imported.name = QFileInfo(path).completeBaseName();
    if (imported.name.trimmed().isEmpty())
        imported.name = QStringLiteral("Imported");

    if (imported.isSystem())
        imported.name = imported.name + QStringLiteral("_imported");

    if (!codeEditorLoading && !codeEditorEditingId.isEmpty())
        saveCodeEditorProfileFromForm();

    auto* mgr = CodeEditorProfileManager::instance();
    QString created = mgr->addProfile(imported.name);
    imported.name = created;
    mgr->updateProfile(imported);

    refreshCodeEditorProfilesList();
    const auto items = codeEditorProfileList->findItems(created, Qt::MatchExactly);
    if (!items.isEmpty())
        codeEditorProfileList->setCurrentItem(items.first());
    buttonApply->setEnabled(true);
}

void DialogSettings::onApply() const
{
    buttonApply->setEnabled(false);

    const_cast<DialogSettings*>(this)->saveCodeEditorProfileFromForm();

    bool themeChanged = settings->data.MainTheme != themeCombo->currentText();
    bool fontChanged  = settings->data.FontSize != fontSizeSpin->value() || settings->data.FontFamily != fontFamilyCombo->currentText();

    if (themeChanged) {
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

    if (fontChanged) {
        settings->data.FontSize   = fontSizeSpin->value();
        settings->data.FontFamily = fontFamilyCombo->currentText();
    }

    if (themeChanged || fontChanged) {
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
    settings->data.ToolbarPosition = toolbarPosCombo->currentData().toInt();
    if (settings->data.ToolbarPosition != prevToolbarPos) {
        settings->getMainAdaptix()->mainUI->RebuildToolbars();
    }

    {
        DockLayoutSettings dock;
        dock.layout = currentDockLayoutId();
        dock.openIn = collectDockOpenIn();
        dock.startup = collectDockStartup();
        DockLayoutEngine::ensureValid(dock);
        settings->data.DockLayout = dock;
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
    settings->data.SessionsViewMode = sessionsViewCombo->currentIndex();
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

    settings->data.TargetsViewMode = targetsViewCombo->currentIndex();
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

    settings->data.CredentialsViewMode = credsViewCombo->currentIndex();
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

    updateTable = false;
    for (int i = 0; i < payloadsCheckCount; i++) {
        if (settings->data.PayloadsTableColumns[i] != payloadsCheck[i]->isChecked()) {
            settings->data.PayloadsTableColumns[i] = payloadsCheck[i]->isChecked();
            updateTable = true;
        }
    }
    if (updateTable)
        settings->getMainAdaptix()->mainUI->UpdatePayloadsColumns();

    for (auto it = m_tabblinkChecks.begin(); it != m_tabblinkChecks.end(); ++it)
        settings->data.BlinkWidgets[it.key()] = it.value()->isChecked();

    auto savePol = [](oclero::qlementine::Switch* r, oclero::qlementine::Switch* w, oclero::qlementine::Switch* p, oclero::qlementine::Switch* s, AxScriptPolicy* pol) {
        if (!pol) return;
        if (r) pol->fileRead  = r->isChecked();
        if (w) pol->fileWrite = w->isChecked();
        if (p) pol->process   = p->isChecked();
        if (s) pol->sandboxFs = s->isChecked();
    };
    savePol(scriptServerRead, scriptServerWrite, scriptServerProcess, scriptServerSandbox, &settings->data.ScriptServer);
    savePol(scriptLocalRead, scriptLocalWrite, scriptLocalProcess, scriptLocalSandbox, &settings->data.ScriptLocal);
    savePol(scriptEditorRead, scriptEditorWrite, scriptEditorProcess, scriptEditorSandbox, &settings->data.ScriptEditor);
    savePol(scriptActionRead, scriptActionWrite, scriptActionProcess, scriptActionSandbox, &settings->data.ScriptEditorAction);
    settings->data.ScriptServer.process = false;
    settings->data.ScriptLocal.process = false;
    settings->data.ScriptEditor.process = false;
    if (scriptSandboxDirEdit)
        settings->data.ScriptSandboxDir = scriptSandboxDirEdit->text().trimmed();

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

    const QString userPath = userAppThemeDir() + "/" + name + ".json";
    const QString resAlias = QStringLiteral(":/qlementine-themes/%1").arg(name);
    const QString resJson  = QStringLiteral(":/qlementine-themes/%1.json").arg(name);
    QString jsonPath;
    if (QFile::exists(userPath))
        jsonPath = userPath;
    else if (QFile::exists(resAlias))
        jsonPath = resAlias;
    else if (QFile::exists(resJson))
        jsonPath = resJson;
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

    QColor bg       = readColor(QStringLiteral("backgroundColorMain1"), QStringLiteral("#1E2220"));
    QColor primary  = readColor(QStringLiteral("primaryColor"),         QStringLiteral("#1890ff"));
    QColor text     = readColor(QStringLiteral("secondaryColor"),       QStringLiteral("#BEBEBE"));
    QColor success  = readColor(QStringLiteral("statusColorSuccess"),   QStringLiteral("#2bb5a0"));
    QColor error    = readColor(QStringLiteral("statusColorError"),     QStringLiteral("#e96b72"));

    QColor colors[5] = {bg, primary, text, success, error};
    for (int i = 0; i < 5; ++i) {
        if (!themeSwatchLabels[i])
            continue;
        themeSwatchLabels[i]->setStyleSheet(QStringLiteral("background: %1; border-radius: 10px; border: 1px solid rgba(255,255,255,0.18);").arg(colors[i].name(QColor::HexRgb)));
        themeSwatchLabels[i]->setToolTip( QStringList{QStringLiteral("Background"), QStringLiteral("Primary"), QStringLiteral("Text"), QStringLiteral("Success"), QStringLiteral("Error")}[i] + QStringLiteral(": ") + colors[i].name(QColor::HexRgb));
    }
}

QString DialogSettings::currentDockLayoutId() const
{
    if (!dockLayoutGroup)
        return QStringLiteral("split_v2");
    if (auto* b = dockLayoutGroup->checkedButton()) {
        const QString id = b->property("layoutId").toString();
        if (!id.isEmpty())
            return id;
    }
    return QStringLiteral("split_v2");
}

void DialogSettings::setDockLayoutId(const QString& layoutId)
{
    if (!dockLayoutGroup)
        return;
    const QList<QAbstractButton*> buttons = dockLayoutGroup->buttons();
    for (QAbstractButton* b : buttons) {
        if (b->property("layoutId").toString() == layoutId) {
            b->setChecked(true);
            return;
        }
    }
    if (!buttons.isEmpty())
        buttons.first()->setChecked(true);
}

QMap<QString, QString> DialogSettings::collectDockOpenIn() const
{
    QMap<QString, QString> openIn;
    if (!dockZoneColumnsHost)
        return openIn;
    const auto lists = dockZoneColumnsHost->findChildren<QListWidget*>();
    for (QListWidget* list : lists) {
        const QString zoneId = list->property("zoneId").toString();
        if (zoneId.isEmpty())
            continue;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* item = list->item(i);
            if (!item)
                continue;
            const QString wid = item->data(Qt::UserRole).toString();
            if (!wid.isEmpty())
                openIn.insert(wid, zoneId);
        }
    }
    return openIn;
}

QStringList DialogSettings::collectDockStartup() const
{
    QStringList startup;
    if (!dockZoneColumnsHost)
        return startup;
    const QStringList candidates = DockLayoutEngine::startupCandidateIds();
    for (QListWidget* list : dockZoneColumnsHost->findChildren<QListWidget*>()) {
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem* item = list->item(i);
            if (!item || !(item->flags() & Qt::ItemIsUserCheckable))
                continue;
            if (item->checkState() != Qt::Checked)
                continue;
            const QString wid = item->data(Qt::UserRole).toString();
            if (!wid.isEmpty() && candidates.contains(wid) && !startup.contains(wid))
                startup.append(wid);
        }
    }
    return startup;
}

void DialogSettings::refreshDockZoneColumns(const QMap<QString, QString>* preferredOpenIn, const QStringList* preferredStartup)
{
    if (!dockZoneColumnsHost)
        return;

    const QMap<QString, QString> prev = preferredOpenIn ? *preferredOpenIn : collectDockOpenIn();
    const QStringList prevStartup = preferredStartup ? *preferredStartup : collectDockStartup();

    if (auto* lay = qobject_cast<QHBoxLayout*>(dockZoneColumnsHost->layout())) {
        while (QLayoutItem* child = lay->takeAt(0)) {
            if (child->widget())
                child->widget()->deleteLater();
            delete child;
        }
    } else {
        auto* layNew = new QHBoxLayout(dockZoneColumnsHost);
        layNew->setContentsMargins(0, 0, 0, 0);
        layNew->setSpacing(8);
    }

    auto* lay = qobject_cast<QHBoxLayout*>(dockZoneColumnsHost->layout());
    if (!lay)
        return;

    const QString layoutId = currentDockLayoutId();
    const QStringList zones = DockLayoutEngine::zoneIdsForLayout(layoutId);
    const DockLayoutSettings defs = DockLayoutEngine::defaultsForLayout(layoutId);
    const QStringList startupOk = DockLayoutEngine::startupCandidateIds();

    QMap<QString, QListWidget*> zoneLists;
    for (const QString& zoneId : zones) {
        auto* col = new QWidget(dockZoneColumnsHost);
        auto* colLay = new QVBoxLayout(col);
        colLay->setContentsMargins(0, 0, 0, 0);
        colLay->setSpacing(4);

        auto* header = new QLabel(DockLayoutEngine::zoneLabel(zoneId), col);
        header->setAlignment(Qt::AlignCenter);
        header->setStyleSheet(QStringLiteral(
            "font-weight: 600; padding: 4px 6px;"
            "background: palette(mid); border-radius: 4px;"));

        auto* list = new QListWidget(col);
        list->setProperty("zoneId", zoneId);
        list->setDragEnabled(true);
        list->setAcceptDrops(true);
        list->setDropIndicatorShown(true);
        list->setDefaultDropAction(Qt::MoveAction);
        list->setDragDropMode(QAbstractItemView::DragDrop);
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setIconSize(QSize(18, 18));
        list->setMinimumWidth(150);
        list->setMinimumHeight(220);
        list->setSpacing(2);
        list->setStyleSheet(QStringLiteral(
            "QListWidget { border: 1px solid palette(mid); border-radius: 4px; padding: 2px; }"
            "QListWidget::item { padding: 3px 4px; }"
            "QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text); }"));

        colLay->addWidget(header);
        colLay->addWidget(list, 1);
        lay->addWidget(col, 1);
        zoneLists.insert(zoneId, list);

        auto markDirty = [this]() {
            if (buttonApply)
                buttonApply->setEnabled(true);
        };
        connect(list->model(), &QAbstractItemModel::rowsInserted, this, [markDirty]() { markDirty(); });
        connect(list->model(), &QAbstractItemModel::rowsRemoved, this, [markDirty]() { markDirty(); });
        connect(list->model(), &QAbstractItemModel::dataChanged, this, [this](const QModelIndex&, const QModelIndex&, const QList<int>& roles) {
                    if (!buttonApply)
                        return;
                    if (roles.isEmpty() || roles.contains(Qt::CheckStateRole))
                        buttonApply->setEnabled(true);
                });
        connect(list, &QListWidget::itemChanged, this, [this](QListWidgetItem*) {
            if (buttonApply)
                buttonApply->setEnabled(true);
        });
    }

    const QString fallbackZone = zones.value(0);
    for (const QString& wid : DockLayoutEngine::widgetIds()) {
        QString zone = prev.value(wid);
        if (zone.isEmpty() || !zoneLists.contains(zone))
            zone = defs.openIn.value(wid, fallbackZone);
        if (!zoneLists.contains(zone))
            zone = fallbackZone;
        QListWidget* list = zoneLists.value(zone);
        if (!list)
            continue;

        {
            const QSignalBlocker listBlocker(list);

            auto* item = new QListWidgetItem(DockLayoutEngine::widgetLabel(wid));
            item->setData(Qt::UserRole, wid);
            const QString iconPath = DockLayoutEngine::widgetIconPath(wid);
            if (!iconPath.isEmpty())
                item->setIcon(QIcon(iconPath));

            Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
            if (startupOk.contains(wid)) {
                flags |= Qt::ItemIsUserCheckable;
                item->setFlags(flags);
                item->setCheckState(prevStartup.contains(wid) ? Qt::Checked : Qt::Unchecked);
                item->setToolTip(QStringLiteral("Drag to change zone. Check to open at project start."));
            } else {
                item->setFlags(flags);
                item->setToolTip(QStringLiteral("Drag to change zone. (Agent panels open on demand.)"));
            }
            list->addItem(item);
        }
    }
}

void DialogSettings::loadSettings()
{
    refreshCodeEditorProfilesList();

    themeCombo->setCurrentText(settings->data.MainTheme);
    updateThemeSwatches();
    fontFamilyCombo->setCurrentText(settings->data.FontFamily);
    fontSizeSpin->setValue(settings->data.FontSize);
    graphCombo1->setCurrentText(settings->data.GraphVersion);
    graphAutoHideInactiveSwitch->setChecked(settings->data.GraphAutoHideInactive);
    graphAutoHideNoChildsSwitch->setChecked(settings->data.GraphAutoHideNoChilds);
    terminalSizeSpin->setValue(settings->data.RemoteTerminalBufferSize);

    {
        const int tbPos = qBound(0, settings->data.ToolbarPosition, 3);
        const int idx = toolbarPosCombo->findData(tbPos);
        toolbarPosCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    {
        DockLayoutSettings dock = settings->data.DockLayout;
        DockLayoutEngine::ensureValid(dock);
        setDockLayoutId(dock.layout);
        refreshDockZoneColumns(&dock.openIn, &dock.startup);
    }

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

    sessionsViewCombo->setCurrentIndex(settings->data.SessionsViewMode);
    sessionsHealthCheck->setChecked(settings->data.CheckHealth);
    sessionsAutoHideInactiveSwitch->setChecked(settings->data.SessionsAutoHideInactive);
    sessionsCompactSwitch->setChecked(settings->data.SessionsCompactMode);
    sessionsCompactSwitch->setEnabled(settings->data.SessionsViewMode == 1);
    sessionsCoafSpin->setValue(settings->data.HealthCoaf);
    sessionsOffsetSpin->setValue(settings->data.HealthOffset);
    sessionsDeadShiftSpin->setValue(settings->data.DeadLightnessShift);

    for (int i = 0; i < tasksCheckCount; i++)
        tasksCheck[i]->setChecked(settings->data.TasksTableColumns[i]);
    tasksInProcessSwitch->setChecked(settings->data.TasksInProcessOnly);
    tasksCompactSwitch->setChecked(settings->data.TasksCompactMode);

    for (int i = 0; i < targetsCheckCount; i++)
        targetsCheck[i]->setChecked(settings->data.TargetsTableColumns[i]);
    targetsViewCombo->setCurrentIndex(settings->data.TargetsViewMode);
    targetsCompactSwitch->setChecked(settings->data.TargetsCompactMode);
    targetsCompactSwitch->setEnabled(settings->data.TargetsViewMode == 1);

    for (int i = 0; i < credsCheckCount; i++)
        credsCheck[i]->setChecked(settings->data.CredentialsTableColumns[i]);
    credsViewCombo->setCurrentIndex(settings->data.CredentialsViewMode);
    credsCompactSwitch->setChecked(settings->data.CredentialsCompactMode);
    credsCompactSwitch->setEnabled(settings->data.CredentialsViewMode == 1);

    for (int i = 0; i < filesCheckCount; i++)
        filesCheck[i]->setChecked(settings->data.FilesTableColumns[i]);
    filesCompactSwitch->setChecked(settings->data.FilesCompactMode);

    for (int i = 0; i < payloadsCheckCount; i++)
        payloadsCheck[i]->setChecked(settings->data.PayloadsTableColumns[i]);

    tabblinkEnabledCheckbox->setChecked(settings->data.TabBlinkEnabled);

    for (auto it = m_tabblinkChecks.begin(); it != m_tabblinkChecks.end(); ++it) {
        if ( settings->data.BlinkWidgets.contains(it.key()) ) {
            bool enabled = settings->data.BlinkWidgets[it.key()];
            it.value()->setChecked(enabled);
        }
    }

    auto loadPol = [](oclero::qlementine::Switch* r, oclero::qlementine::Switch* w, oclero::qlementine::Switch* p, oclero::qlementine::Switch* s, const AxScriptPolicy& pol) {
        if (r) r->setChecked(pol.fileRead);
        if (w) w->setChecked(pol.fileWrite);
        if (p) p->setChecked(pol.process);
        if (s) s->setChecked(pol.sandboxFs);
    };
    loadPol(scriptServerRead, scriptServerWrite, scriptServerProcess, scriptServerSandbox, settings->data.ScriptServer);
    loadPol(scriptLocalRead, scriptLocalWrite, scriptLocalProcess, scriptLocalSandbox, settings->data.ScriptLocal);
    loadPol(scriptEditorRead, scriptEditorWrite, scriptEditorProcess, scriptEditorSandbox, settings->data.ScriptEditor);
    loadPol(scriptActionRead, scriptActionWrite, scriptActionProcess, scriptActionSandbox, settings->data.ScriptEditorAction);
    if (scriptServerProcess) { scriptServerProcess->setChecked(false); scriptServerProcess->setEnabled(false); }
    if (scriptLocalProcess)  { scriptLocalProcess->setChecked(false);  scriptLocalProcess->setEnabled(false); }
    if (scriptEditorProcess) { scriptEditorProcess->setChecked(false); scriptEditorProcess->setEnabled(false); }
    if (scriptSandboxDirEdit)
        scriptSandboxDirEdit->setText(settings->data.ScriptSandboxDir);

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

    QStringList builtIn = {
        QStringLiteral("Adaptix_Dark_Emerald"),
        QStringLiteral("Adaptix_Light_Emerald"),
        QStringLiteral("Adaptix_Dracula"),
        QStringLiteral("Black"),
        QStringLiteral("Solarized"),
        QStringLiteral("Monokai"),
    };
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
