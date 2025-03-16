#ifndef ADAPTIXCLIENT_DIALOGSETTINGS_H
#define ADAPTIXCLIENT_DIALOGSETTINGS_H

#include <main.h>
#include <Client/Settings.h>

class Settings;

class DialogSettings : public QWidget
{
Q_OBJECT

    Settings* settings = nullptr;

    QWidget*        mainWidget    = nullptr;
    QGridLayout*    layoutMain    = nullptr;
    QListWidget*    listSettings  = nullptr;
    QVBoxLayout*    headerLayout  = nullptr;
    QLabel*         labelHeader   = nullptr;
    QFrame*         lineFrame     = nullptr;
    QStackedWidget* stackSettings = nullptr;
    QSpacerItem*    hSpacer       = nullptr;
    QPushButton*    buttonApply   = nullptr;
    QPushButton*    buttonClose   = nullptr;

    QWidget*     mainSettingWidget   = nullptr;
    QGridLayout* mainSettingLayout   = nullptr;
    QLabel*      themeLabel          = nullptr;
    QComboBox*   themeCombo          = nullptr;
    QLabel*      fontSizeLabel       = nullptr;
    QSpinBox*    fontSizeSpin        = nullptr;
    QLabel*      fontFamilyLabel     = nullptr;
    QComboBox*   fontFamilyCombo     = nullptr;
    QCheckBox*   consoleTimeCheckbox = nullptr;

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
