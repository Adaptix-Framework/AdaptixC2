#ifndef ADAPTIXCLIENT_WIDGETBUILDER_H
#define ADAPTIXCLIENT_WIDGETBUILDER_H

#include <main.h>
#include <Utils/CustomElements.h>

class WidgetBuilder
{
    QWidget* widget = nullptr;

    QMap<QString, QWidget*> widgetMap;
    QVector<void*>          lpVector;
    QJsonObject             qJsonObject;
    QString                 error;

public:
    bool valid = false;

    explicit WidgetBuilder(const QByteArray& jsonData);
    ~WidgetBuilder();

    void     BuildWidget(bool editable);
    QLayout* BuildLayout(QString layoutType, QJsonObject rootObj, bool editable);
    QString  GetError();
    QWidget* GetWidget();
    void     ClearWidget();
    void     FillData(QString jsonString);
    QString  CollectData();
};

#endif //ADAPTIXCLIENT_WIDGETBUILDER_H