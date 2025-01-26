#ifndef ADAPTIXCLIENT_DIALOGSETTINGS_H
#define ADAPTIXCLIENT_DIALOGSETTINGS_H

#include <main.h>

class DialogSettings : public QWidget
{
Q_OBJECT
    QWidget*        mainWidget       = nullptr;
    QGridLayout*    layoutMain       = nullptr;
    QListWidget*    listSettings     = nullptr;
    QVBoxLayout*    headerLayout     = nullptr;
    QLabel*         labelHeader      = nullptr;
    QFrame*         lineFrame        = nullptr;
    QStackedWidget* stackSettings    = nullptr;
    QSpacerItem*    hSpacer          = nullptr;
    QPushButton*    buttonOk         = nullptr;
    QPushButton*    buttonCancel     = nullptr;

    void createUI();

public:
    DialogSettings(QWidget* w);

public slots:
    void onStackChange(int index);
};

#endif //ADAPTIXCLIENT_DIALOGSETTINGS_H
