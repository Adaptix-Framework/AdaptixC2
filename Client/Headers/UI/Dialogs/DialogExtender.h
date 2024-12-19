#ifndef ADAPTIXCLIENT_DIALOGEXTENDER_H
#define ADAPTIXCLIENT_DIALOGEXTENDER_H

#include <main.h>
#include <Client/Extender.h>
#include <Utils/CustomElements.h>

class Extender;

class DialogExtender : public QWidget
{
Q_OBJECT

    Extender*     extender    = nullptr;
    QGridLayout*  layout      = nullptr;
    QTableWidget* tableWidget = nullptr;
    QTextEdit*    textComment = nullptr;
    QPushButton*  buttonClose = nullptr;

    void createUI();

public:
    DialogExtender(Extender* e);
    ~DialogExtender();

    void AddExtenderItem(ExtensionFile extenderItem);
    void UpdateExtenderItem(ExtensionFile extenderItem);

public slots:
    void handleMenu(const QPoint &pos ) const;
    void onActionAdd();
    void onActionEnable();
    void onActionDisable();
    void onActionRemove();
    void onRowSelect(int row, int column);
};

#endif //ADAPTIXCLIENT_DIALOGEXTENDER_H
