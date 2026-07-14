#ifndef ADAPTIXCLIENT_SPINTABLE_H
#define ADAPTIXCLIENT_SPINTABLE_H

#include <main.h>

class SpinTable : public QWidget {
Q_OBJECT
public:
    QGridLayout*        layout      = nullptr;
    QTableView*         table       = nullptr;
    QStandardItemModel* tableModel  = nullptr;
    QPushButton*        buttonAdd   = nullptr;
    QPushButton*        buttonClear = nullptr;

    SpinTable(int rows, int clomuns, QWidget* parent);

    explicit SpinTable(QWidget* parent = nullptr) : SpinTable(0, 0, parent) {}
    ~SpinTable() override = default;
};

#endif
