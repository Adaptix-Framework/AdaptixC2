#ifndef AXCHECKER_WIDGETBUILDER_H
#define AXCHECKER_WIDGETBUILDER_H

#include <main.h>

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
};

#endif //AXCHECKER_WIDGETBUILDER_H