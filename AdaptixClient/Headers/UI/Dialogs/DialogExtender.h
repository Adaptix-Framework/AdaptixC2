#ifndef ADAPTIXCLIENT_DIALOGEXTENDER_H
#define ADAPTIXCLIENT_DIALOGEXTENDER_H

#include <main.h>

class Extender;
class AdaptixWidget;
class MainUI;

class DialogExtender : public QWidget
{
Q_OBJECT

    Extender*     extender    = nullptr;
    QGridLayout*  layout      = nullptr;
    QTabWidget*   tabWidget   = nullptr;

    QTableWidget* tableWidget       = nullptr;
    QSplitter*    splitter          = nullptr;
    QTextEdit*    textComment       = nullptr;

    QWidget*      serverTab            = nullptr;
    QComboBox*    serverProjectCombo   = nullptr;
    QTableWidget* serverTableWidget    = nullptr;
    QSplitter*    serverSplitter       = nullptr;
    QTextEdit*    serverTextComment    = nullptr;
    MainUI*       mainUI               = nullptr;

    QPushButton*  buttonClose = nullptr;
    QSpacerItem*  spacer1     = nullptr;
    QSpacerItem*  spacer2     = nullptr;

    AdaptixWidget* currentAdaptixWidget = nullptr;

    void createUI();

public:
    DialogExtender(Extender* e);
    ~DialogExtender() override;

    void AddExtenderItem(const ExtensionFile &extenderItem) const;
    void UpdateExtenderItem(const ExtensionFile &extenderItem) const;
    void RemoveExtenderItem(const ExtensionFile &extenderItem) const;

    void SetMainUI(MainUI* ui);
    void RefreshProjectsList();
    void RefreshServerScripts();

public Q_SLOTS:
    void handleMenu(const QPoint &pos ) const;
    void onActionLoad() const;
    void onActionReload() const;
    void onActionEnable() const;
    void onActionDisable() const;
    void onActionRemove() const;
    void onRowSelect(int row, int column) const;

    void handleServerMenu(const QPoint &pos);
    void onServerActionEnable();
    void onServerActionDisable();
    void onServerRowSelect(int row, int column) const;
    void onProjectChanged(int index);
};

#endif
