#ifndef ADAPTIXCLIENT_DIALOGSETTINGS_H
#define ADAPTIXCLIENT_DIALOGSETTINGS_H

#include <main.h>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QSlider>
#include <oclero/qlementine/widgets/Switch.hpp>

class Settings;

class DialogSettings : public QWidget
{
Q_OBJECT

    Settings* settings = nullptr;

    QGridLayout*    layoutMain    = nullptr;
    QListWidget*    listSettings  = nullptr;
    QVBoxLayout*    headerLayout  = nullptr;
    QLabel*         labelHeader   = nullptr;
    QFrame*         lineFrame     = nullptr;
    QStackedWidget* stackSettings = nullptr;
    QSpacerItem*    hSpacer       = nullptr;
    QPushButton*    buttonApply   = nullptr;
    QPushButton*    buttonClose   = nullptr;

    QWidget*     appearanceWidget = nullptr;
    QGridLayout* appearanceLayout = nullptr;
    QWidget*     consolePageWidget = nullptr;
    QGridLayout* consolePageLayout = nullptr;
    QLabel*      themeLabel        = nullptr;
    QComboBox*   themeCombo        = nullptr;
    QPushButton* themeImportBtn    = nullptr;
    QPushButton* themeDeleteBtn    = nullptr;
    QFrame*      themeSwatchesFrame = nullptr;
    QHBoxLayout* themeSwatchesLayout = nullptr;
    QLabel*      themeSwatchLabels[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
    QLabel*      fontSizeLabel     = nullptr;
    QSpinBox*    fontSizeSpin      = nullptr;
    QLabel*      fontFamilyLabel   = nullptr;
    QComboBox*   fontFamilyCombo   = nullptr;
    QLabel*      graphLabel1       = nullptr;
    QComboBox*   graphCombo1       = nullptr;
    oclero::qlementine::Switch* graphAutoHideInactiveSwitch = nullptr;
    oclero::qlementine::Switch* graphAutoHideNoChildsSwitch = nullptr;
    QLabel*      terminalSizeLabel = nullptr;
    QSpinBox*    terminalSizeSpin  = nullptr;
    QLabel*      toolbarPosLabel   = nullptr;
    QFrame*      toolbarPosFrame   = nullptr;
    QGridLayout* toolbarPosGrid    = nullptr;
    QPushButton* toolbarPosBtn[4]  = {nullptr, nullptr, nullptr, nullptr};

    QGroupBox*   consoleThemeGroup         = nullptr;
    QGridLayout* consoleThemeGroupLayout   = nullptr;
    QGroupBox*   consoleBehaviorGroup      = nullptr;
    QGridLayout* consoleBehaviorGroupLayout = nullptr;
    QLabel*      consoleSizeLabel          = nullptr;
    QSpinBox*    consoleSizeSpin           = nullptr;
    oclero::qlementine::Switch* consoleTimeCheckbox           = nullptr;
    oclero::qlementine::Switch* consoleNoWrapCheckbox         = nullptr;
    oclero::qlementine::Switch* consoleAutoScrollCheckbox     = nullptr;
    oclero::qlementine::Switch* consoleAutoLoadEarlierCheckbox = nullptr;
    QLabel*      consolePageSizeLabel       = nullptr;
    QSpinBox*    consolePageSizeSpin        = nullptr;
    oclero::qlementine::Switch* consoleShowBackgroundCheckbox = nullptr;
    oclero::qlementine::Switch* consoleUseAppThemeCheckbox   = nullptr;
    QLabel*      consoleBgImageLabel        = nullptr;
    QLineEdit*   consoleBgImagePathEdit     = nullptr;
    QPushButton* consoleBgImageBrowseBtn    = nullptr;
    QPushButton* consoleBgImageClearBtn     = nullptr;
    QLabel*      consoleBgDimmingLabel      = nullptr;
    QSlider*     consoleBgDimmingSlider     = nullptr;
    QLabel*      consoleBgDimmingValueLabel = nullptr;
    QFrame*      consoleBgPreviewFrame      = nullptr;
    QLabel*      consoleBgPreviewLabel      = nullptr;
    QLabel*      consoleThemeLabel         = nullptr;
    QComboBox*   consoleThemeCombo         = nullptr;
    QPushButton* consoleThemeImportBtn     = nullptr;
    QPushButton* consoleThemeDeleteBtn     = nullptr;

    QWidget*     sessionsWidget       = nullptr;
    QGridLayout* sessionsLayout       = nullptr;
    oclero::qlementine::Switch* sessionsAutoHideInactiveSwitch = nullptr;
    oclero::qlementine::Switch* sessionsCompactSwitch = nullptr;
    QGroupBox*   sessionsGroup        = nullptr;
    QVBoxLayout* sessionsGroupLayout  = nullptr;
    int          sessionsCheckCount   = 16;
    QCheckBox*   sessionsCheck[16];
    oclero::qlementine::Switch* sessionsHealthCheck = nullptr;
    QLabel*      sessionsLabel1       = nullptr;
    QLabel*      sessionsLabel2       = nullptr;
    QLabel*      sessionsLabel3       = nullptr;
    QDoubleSpinBox* sessionsCoafSpin  = nullptr;
    QSpinBox*    sessionsOffsetSpin   = nullptr;
    QDoubleSpinBox* sessionsDeadShiftSpin = nullptr;

    QWidget*     tasksWidget      = nullptr;
    QGridLayout* tasksLayout      = nullptr;
    oclero::qlementine::Switch* tasksInProcessSwitch = nullptr;
    oclero::qlementine::Switch* tasksCompactSwitch   = nullptr;
    QGroupBox*   tasksGroup       = nullptr;
    QVBoxLayout* tasksGroupLayout = nullptr;
    int          tasksCheckCount  = 10;
    QCheckBox*   tasksCheck[10];

    QWidget*     targetsWidget     = nullptr;
    QGridLayout* targetsLayout     = nullptr;
    oclero::qlementine::Switch* targetsCompactSwitch = nullptr;
    QGroupBox*   targetsGroup      = nullptr;
    int          targetsCheckCount = 10;
    QCheckBox*   targetsCheck[10];

    QWidget*     credsWidget     = nullptr;
    QGridLayout* credsLayout     = nullptr;
    oclero::qlementine::Switch* credsCompactSwitch = nullptr;
    QGroupBox*   credsGroup      = nullptr;
    int          credsCheckCount = 10;
    QCheckBox*   credsCheck[10];

    QWidget*     filesWidget     = nullptr;
    QGridLayout* filesLayout     = nullptr;
    oclero::qlementine::Switch* filesCompactSwitch = nullptr;
    QGroupBox*   filesGroup      = nullptr;
    int          filesCheckCount = 11;
    QCheckBox*   filesCheck[11];

    QWidget*     tabblinkWidget          = nullptr;
    QGridLayout* tabblinkLayout          = nullptr;
    oclero::qlementine::Switch* tabblinkEnabledCheckbox = nullptr;
    QGroupBox*   tabblinkGroup           = nullptr;
    QGridLayout* tabblinkGroupLayout     = nullptr;
    QMap<QString, QCheckBox*> m_tabblinkChecks;  // className -> checkbox

    QWidget*     shortcutsWidget      = nullptr;
    QGridLayout* shortcutsLayout      = nullptr;
    QTableWidget* shortcutsTable      = nullptr;

    void createUI();
    void loadSettings();
    void refreshAppThemeCombo();
    void refreshConsoleThemeCombo();
    void updateThemeSwatches();

    static QString userAppThemeDir();
    static bool importAppTheme(const QString& filePath);
    static bool deleteAppTheme(const QString& name);

protected:
    void showEvent(QShowEvent* event) override;

public:
    DialogSettings(Settings* s);

public Q_SLOTS:
    void onStackChange(int index) const;
    void onHealthChange() const;
    void onBlinkChange() const;
    void onUseAppThemeChange() const;
    void onApply() const;
    void onClose();
};

#endif