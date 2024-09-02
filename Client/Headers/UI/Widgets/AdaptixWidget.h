#ifndef ADAPTIXCLIENT_ADAPTIXWIDGET_H
#define ADAPTIXCLIENT_ADAPTIXWIDGET_H

#include <main.h>

class AdaptixWidget : public QWidget {

    QGridLayout* gridLayout_Main   = nullptr;
    QHBoxLayout* topLayout         = nullptr;
    QPushButton* listenersButton   = nullptr;
    QPushButton* logsButton        = nullptr;
    QPushButton* sessionsButton    = nullptr;
    QPushButton* graphButton       = nullptr;
    QPushButton* targetsButton     = nullptr;
    QPushButton* jobsButton        = nullptr;
    QPushButton* proxyButton       = nullptr;
    QPushButton* downloadsButton   = nullptr;
    QPushButton* credsButton       = nullptr;
    QPushButton* screensButton     = nullptr;
    QPushButton* keysButton        = nullptr;
    QPushButton* reconnectButton   = nullptr;
    QFrame*      line_1            = nullptr;
    QFrame*      line_2            = nullptr;
    QFrame*      line_3            = nullptr;
    QFrame*      line_4            = nullptr;
    QSplitter*   mainVSplitter     = nullptr;
    QTabWidget*  mainTabWidget     = nullptr;
    QSpacerItem* horizontalSpacer2 = nullptr;

    void createUI();

public:

    explicit AdaptixWidget();

};

#endif //ADAPTIXCLIENT_ADAPTIXWIDGET_H
