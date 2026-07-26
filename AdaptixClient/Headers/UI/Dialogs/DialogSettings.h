#ifndef ADAPTIXCLIENT_DIALOGSETTINGS_H
#define ADAPTIXCLIENT_DIALOGSETTINGS_H

#include <main.h>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QSlider>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QStackedWidget>
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
    QLabel*      sessionsViewLabel    = nullptr;
    QComboBox*   sessionsViewCombo    = nullptr;
    oclero::qlementine::Switch* sessionsAutoHideInactiveSwitch = nullptr;
    oclero::qlementine::Switch* sessionsCompactSwitch = nullptr;
    QGroupBox*   sessionsGroup        = nullptr;
    QVBoxLayout* sessionsGroupLayout  = nullptr;
    int          sessionsCheckCount   = 17;
    QCheckBox*   sessionsCheck[17];
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
    QLabel*      targetsViewLabel  = nullptr;
    QComboBox*   targetsViewCombo  = nullptr;
    oclero::qlementine::Switch* targetsCompactSwitch = nullptr;
    QGroupBox*   targetsGroup      = nullptr;
    int          targetsCheckCount = 10;
    QCheckBox*   targetsCheck[10];

    QWidget*     credsWidget     = nullptr;
    QGridLayout* credsLayout     = nullptr;
    QLabel*      credsViewLabel  = nullptr;
    QComboBox*   credsViewCombo  = nullptr;
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

    QWidget*     scriptSecWidget = nullptr;
    QVBoxLayout* scriptSecLayout = nullptr;
    oclero::qlementine::Switch* scriptServerRead = nullptr;
    oclero::qlementine::Switch* scriptServerWrite = nullptr;
    oclero::qlementine::Switch* scriptServerProcess = nullptr;
    oclero::qlementine::Switch* scriptServerSandbox = nullptr;
    oclero::qlementine::Switch* scriptLocalRead = nullptr;
    oclero::qlementine::Switch* scriptLocalWrite = nullptr;
    oclero::qlementine::Switch* scriptLocalProcess = nullptr;
    oclero::qlementine::Switch* scriptLocalSandbox = nullptr;
    oclero::qlementine::Switch* scriptEditorRead = nullptr;
    oclero::qlementine::Switch* scriptEditorWrite = nullptr;
    oclero::qlementine::Switch* scriptEditorProcess = nullptr;
    oclero::qlementine::Switch* scriptEditorSandbox = nullptr;
    oclero::qlementine::Switch* scriptActionRead = nullptr;
    oclero::qlementine::Switch* scriptActionWrite = nullptr;
    oclero::qlementine::Switch* scriptActionProcess = nullptr;
    oclero::qlementine::Switch* scriptActionSandbox = nullptr;
    QLineEdit*   scriptSandboxDirEdit = nullptr;

    QPushButton*  codeEditorResetDefaultsBtn = nullptr;
    QWidget*      codeEditorWidget       = nullptr;
    QHBoxLayout*  codeEditorLayout       = nullptr;
    QListWidget*  codeEditorProfileList  = nullptr;
    QPushButton*  codeEditorAddBtn       = nullptr;
    QPushButton*  codeEditorRemoveBtn    = nullptr;
    QPushButton*  codeEditorForkBtn      = nullptr;
    QPushButton*  codeEditorExportBtn    = nullptr;
    QPushButton*  codeEditorImportBtn    = nullptr;
    QString       codeEditorEditingId;
    QTabWidget*   codeEditorTabs         = nullptr;
    QLineEdit*    codeEditorNameEdit     = nullptr;
    QComboBox*    codeEditorLanguageCombo = nullptr;
    QLineEdit*    codeEditorBuildEdit    = nullptr;
    QLineEdit*    codeEditorRunEdit      = nullptr;
    QLineEdit*    codeEditorDefinesEdit  = nullptr;
    oclero::qlementine::Switch* codeEditorMainEngineSwitch = nullptr;

    QWidget*      codeEditorBuildFields  = nullptr;
    QCheckBox* codeEditorTbNewFile = nullptr;
    QCheckBox* codeEditorTbOpenFile = nullptr;
    QCheckBox* codeEditorTbOpenFolder = nullptr;
    QCheckBox* codeEditorTbSave = nullptr;
    QCheckBox* codeEditorTbExplorer = nullptr;
    QCheckBox* codeEditorTbBuildLog = nullptr;
    QCheckBox* codeEditorTbMinimap = nullptr;
    QCheckBox* codeEditorTbWordWrap = nullptr;

    oclero::qlementine::Switch* codeEditorPanelEnabledSwitch = nullptr;
    QPlainTextEdit*  codeEditorPanelScriptEdit  = nullptr;
    QPushButton*     codeEditorPanelScriptTplBtn = nullptr;
    QLabel*          codeEditorPanelScriptHint  = nullptr;
    int              codeEditorActionEditRow    = -1;
    QTableWidget*    codeEditorActionsTable     = nullptr;
    QPlainTextEdit*  codeEditorActionScriptEdit = nullptr;
    QLabel*          codeEditorActionBodyLabel  = nullptr;
    QLineEdit*       codeEditorActionLabelEdit  = nullptr;
    QPushButton*     codeEditorActionIconBtn    = nullptr;
    QString          codeEditorActionIconPath;
    QPushButton*     codeEditorActionAddBtn     = nullptr;
    QPushButton*     codeEditorActionRemoveBtn  = nullptr;
    QStackedWidget*  codeEditorCustomStack      = nullptr;
    QWidget*         codeEditorCustomEditorPage = nullptr;
    QLabel*          codeEditorCustomEmptyLabel = nullptr;
    QWidget*         codeEditorActionScriptHost = nullptr;
    bool          codeEditorLoading      = false;
    bool          codeEditorActionScriptLoading = false;

    void createUI();
    void loadSettings();
    void refreshAppThemeCombo();
    void refreshConsoleThemeCombo();
    void updateThemeSwatches();
    void refreshCodeEditorProfilesList();
    void loadCodeEditorProfileToForm(const QString& name);
    void saveCodeEditorProfileFromForm();
    void updateCodeEditorPanelScriptVisibility();
    void setCodeEditorFormEditable(bool editable);
    void syncCodeEditorActionScriptToTable(int row = -1);
    void loadCodeEditorActionScriptFromRow(int row);
    void updateCodeEditorActionTypeUi();
    void pickCodeEditorActionIcon();
    void exportCodeEditorProfile();
    void importCodeEditorProfile();

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