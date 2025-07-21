#ifndef ADAPTIXCLIENT_DIALOGEXTENDER_H
#define ADAPTIXCLIENT_DIALOGEXTENDER_H

#include <main.h>

class Extender;

class DialogExtender : public QWidget
{
Q_OBJECT

    Extender*     extender    = nullptr;
    QGridLayout*  layout      = nullptr;
    QTableWidget* tableWidget = nullptr;
    QTextEdit*    textComment = nullptr;
    QPushButton*  buttonClose = nullptr;
    QSpacerItem*  spacer1     = nullptr;
    QSpacerItem*  spacer2     = nullptr;

    void createUI();

public:
    DialogExtender(Extender* e);
    ~DialogExtender() override;

    void AddExtenderItem(const ExtensionFile &extenderItem) const;
    void UpdateExtenderItem(const ExtensionFile &extenderItem) const;
    void RemoveExtenderItem(const ExtensionFile &extenderItem) const;

public slots:
    void handleMenu(const QPoint &pos ) const;
    void onActionLoad() const;
    void onActionReload() const;
    void onActionEnable() const;
    void onActionDisable() const;
    void onActionRemove() const;
    void onRowSelect(int row, int column) const;
};

#endif
