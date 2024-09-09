#ifndef ADAPTIXCLIENT_DIALOGLISTENER_H
#define ADAPTIXCLIENT_DIALOGLISTENER_H

#include <main.h>

class DialogListener : public QDialog {

    QGridLayout*    mainGridLayout;
    QGridLayout*    stackGridLayout;
    QSpacerItem*    horizontalSpacer;
    QSpacerItem*    horizontalSpacer_2;
    QSpacerItem*    horizontalSpacer_3;
    QSpacerItem*    horizontalSpacer_4;
    QSpacerItem*    horizontalSpacer_5;
    QLabel*         listenerNameLabel;
    QLineEdit*      listenerNameInput;
    QLabel*         listenerTypeLabel;
    QComboBox*      listenerTypeCombobox;
    QPushButton*    buttonClose;
    QPushButton*    buttonSave;
    QGroupBox*      listenerConfigGroupbox;
    QStackedWidget* configStackWidget;

    void createUI();

public:
    explicit DialogListener();
    ~DialogListener();

    void Start();

};

#endif //ADAPTIXCLIENT_DIALOGLISTENER_H