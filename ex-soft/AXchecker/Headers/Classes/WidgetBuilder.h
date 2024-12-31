#ifndef AXCHECKER_WIDGETBUILDER_H
#define AXCHECKER_WIDGETBUILDER_H

#include <main.h>

class SpinTable : public QWidget {
Q_OBJECT
public:
    QGridLayout*  layout      = nullptr;
    QTableWidget* table       = nullptr;
    QPushButton*  buttonAdd   = nullptr;
    QPushButton*  buttonClear = nullptr;

    SpinTable(int rows, int clomuns, QWidget* parent);
    SpinTable(QWidget* parent = nullptr) { SpinTable(0,0,parent); }
    ~SpinTable() = default;
};

class WidgetBuilder {

    QWidget* widget = nullptr;

    QMap<QString, QWidget*> widgetMap;
    QVector<void*>          lpVector;
    QJsonObject             qJsonObject;
    QString                 error;

public:
    bool valid = false;

    explicit WidgetBuilder(const QByteArray& jsonData);
    ~WidgetBuilder();

    void     BuildWidget();
    QLayout* BuildLayout(QString layoutType, QJsonObject rootObj);
    QString  GetError();
    QWidget* GetWidget();
    QString  CollectData();
    void     FillData(QString jsonString);
};

#endif //AXCHECKER_WIDGETBUILDER_H