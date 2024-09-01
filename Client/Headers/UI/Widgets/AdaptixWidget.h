#ifndef ADAPTIXCLIENT_ADAPTIXWIDGET_H
#define ADAPTIXCLIENT_ADAPTIXWIDGET_H

#include <main.h>

class AdaptixWidget : public QWidget {

    QWidget*     mainWidget      = nullptr;
    QGridLayout* gridLayout_Main = nullptr;
    QWidget*     topWidget       = nullptr;
    QHBoxLayout* topLayout       = nullptr;
    QPushButton* listenersButton = nullptr;
    QPushButton* sessionsButton  = nullptr;
    QPushButton* logsButton      = nullptr;

public:

    explicit AdaptixWidget();

};

#endif //ADAPTIXCLIENT_ADAPTIXWIDGET_H
