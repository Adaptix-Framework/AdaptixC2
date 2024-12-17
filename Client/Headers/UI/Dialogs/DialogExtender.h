#ifndef ADAPTIXCLIENT_DIALOGEXTENDER_H
#define ADAPTIXCLIENT_DIALOGEXTENDER_H

#include <main.h>
#include <Client/Extender.h>

class Extender;

class DialogExtender : public QWidget
{
Q_OBJECT

    Extender*     extender    = nullptr;
    QGridLayout*  layout      = nullptr;
    QTableWidget* table       = nullptr;
    QPushButton*  buttonClose = nullptr;

    void createUI();

public:
    DialogExtender(Extender* e);
    ~DialogExtender();

public slots:
    void handleMenu(const QPoint &pos ) const;
    void onActionAdd();
    void onActionEnable();
    void onActionDisable();
    void onActionRemove();
};

#endif //ADAPTIXCLIENT_DIALOGEXTENDER_H
