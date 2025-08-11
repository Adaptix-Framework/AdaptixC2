#ifndef TARGETSWIDGET_H
#define TARGETSWIDGET_H

#include <main.h>

class AdaptixWidget;
class ClickableLabel;

class TargetsWidget : public QWidget
{
    AdaptixWidget* adaptixWidget  = nullptr;
    QGridLayout*   mainGridLayout = nullptr;
    QTableWidget*  tableWidget    = nullptr;
    QShortcut*     shortcutSearch = nullptr;

    QWidget*        searchWidget    = nullptr;
    QHBoxLayout*    searchLayout    = nullptr;
    QLineEdit*      inputFilter     = nullptr;
    ClickableLabel* hideButton      = nullptr;

    int ColumnId       = 0;
    int ColumnComputer = 1;
    int ColumnDomain   = 2;
    int ColumnAddress  = 3;
    int ColumnTag      = 4;
    int ColumnOs       = 5;
    int ColumnDate     = 6;
    int ColumnInfo     = 7;

    void createUI();
    bool filterItem(const CredentialData &credentials) const;
    void addTableItem(const CredentialData &newCredentials) const;

public:
    explicit TargetsWidget(AdaptixWidget* w);
    ~TargetsWidget() override;

    void Clear() const;

    void SetData() const;
    void ClearTableContent() const;

    void TargetsAdd(QList<TargetData> targetList);

public slots:
    void toggleSearchPanel() const;
    void onFilterUpdate() const;
    void handleTargetsMenu( const QPoint &pos ) const;
    void onCreateTarget();
    void onEditTarget() const;
    void onRemoveTarget() const;
    void onExportTarget() const;
};

#endif //TARGETSWIDGET_H
