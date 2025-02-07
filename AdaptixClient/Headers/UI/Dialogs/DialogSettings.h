#ifndef ADAPTIXCLIENT_DIALOGSETTINGS_H
#define ADAPTIXCLIENT_DIALOGSETTINGS_H

#include <main.h>
#include <Client/Settings.h>
#include <Utils/CustomElements.h>

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

    QWidget*     sessionsWidget      = nullptr;
    QGridLayout* sessionsLayout      = nullptr;
    QGroupBox*   sessionsGroup       = nullptr;
    QGridLayout* groupLayout         = nullptr;
    QCheckBox*   sessionsCheck[15];

void createUI();

public:
    DialogSettings(Settings* s);

public slots:
    void onStackChange(int index);
    void onApply();
    void onClose();
};

#endif //ADAPTIXCLIENT_DIALOGSETTINGS_H
