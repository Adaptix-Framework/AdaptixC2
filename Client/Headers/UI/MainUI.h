#ifndef ADAPTIXCLIENT_MAINUI_H
#define ADAPTIXCLIENT_MAINUI_H

#include <main.h>
#include <UI/Widgets/AdaptixWidget.h>

class MainUI : public QMainWindow {

    QWidget* mainWidget = nullptr;

public:

    explicit MainUI();

};

#endif //ADAPTIXCLIENT_MAINUI_H
