#ifndef ADAPTIXCLIENT_DIALOGSETTINGS_H
#define ADAPTIXCLIENT_DIALOGSETTINGS_H

#include <main.h>

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

    QWidget*     mainSettingWidget = nullptr;
    QGridLayout* mainSettingLayout = nullptr;
    QLabel*      themeLabel        = nullptr;
    QComboBox*   themeCombo        = nullptr;
    QLabel*      fontSizeLabel     = nullptr;
    QSpinBox*    fontSizeSpin      = nullptr;
    QLabel*      fontFamilyLabel   = nullptr;
    QComboBox*   fontFamilyCombo   = nullptr;
    QLabel*      graphLabel1       = nullptr;
    QComboBox*   graphCombo1       = nullptr;
    QLabel*      terminalSizeLabel = nullptr;
    QSpinBox*    terminalSizeSpin  = nullptr;

    QGroupBox*   consoleGroup              = nullptr;
    QGridLayout* consoleGroupLayout        = nullptr;
    QLabel*      consoleSizeLabel          = nullptr;
    QSpinBox*    consoleSizeSpin           = nullptr;
    QCheckBox*   consoleTimeCheckbox       = nullptr;
    QCheckBox*   consoleNoWrapCheckbox     = nullptr;
    QCheckBox*   consoleAutoScrollCheckbox = nullptr;

    QWidget*     sessionsWidget       = nullptr;
    QGridLayout* sessionsLayout       = nullptr;
    QGroupBox*   sessionsGroup        = nullptr;
    QGridLayout* sessionsGroupLayout  = nullptr;
    QCheckBox*   sessionsCheck[15];
    QCheckBox*   sessionsHealthCheck  = nullptr;
    QLabel*      sessionsLabel1       = nullptr;
    QLabel*      sessionsLabel2       = nullptr;
    QLabel*      sessionsLabel3       = nullptr;
    QDoubleSpinBox* sessionsCoafSpin  = nullptr;
    QSpinBox*    sessionsOffsetSpin   = nullptr;

    QWidget*     tasksWidget      = nullptr;
    QGridLayout* tasksLayout      = nullptr;
    QGroupBox*   tasksGroup       = nullptr;
    QGridLayout* tasksGroupLayout = nullptr;
    QCheckBox*   tasksCheck[11];

void createUI();

public:
    DialogSettings(Settings* s);

public slots:
    void onStackChange(int index) const;
    void onHealthChange() const;
    void onApply() const;
    void onClose();
};

#endif //ADAPTIXCLIENT_DIALOGSETTINGS_H
