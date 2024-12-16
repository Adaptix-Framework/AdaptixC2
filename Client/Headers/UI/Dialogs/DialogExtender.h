#ifndef ADAPTIXCLIENT_DIALOGEXTENDER_H
#define ADAPTIXCLIENT_DIALOGEXTENDER_H

#include <main.h>

class DialogExtender : public QWidget
{
Q_OBJECT

    QGridLayout*  layout      = nullptr;
    QTableWidget* table       = nullptr;
    QSpacerItem*  hSpacer     = nullptr;
    QPushButton*  buttonClose = nullptr;


    void createUI();

public:
    DialogExtender();
    ~DialogExtender();

public slots:
    void handleMenu(const QPoint &pos ) const;
    void onActionAdd();
    void onActionEnable();
    void onActionDisable();
    void onActionRemove();
};

#endif //ADAPTIXCLIENT_DIALOGEXTENDER_H
