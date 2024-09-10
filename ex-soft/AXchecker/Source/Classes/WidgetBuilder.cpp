#include <Classes/WidgetBuilder.h>

WidgetBuilder::WidgetBuilder(const QByteArray& jsonData) {
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        qJsonObject = document.object();
        widget = new QWidget;
        this->BuildWidget();
    } else {
        error = QString("JSON parse error: %1").arg(parseError.errorString());
    }
}

WidgetBuilder::~WidgetBuilder() {
    for ( void* lp : lpVector )
        delete lp;
}

QString WidgetBuilder::GetError() {
    return error;
}

QLayout* WidgetBuilder::BuildLayout(QString layoutType, QJsonObject rootObj) {
    QLayout* layout = nullptr;

    if (layoutType.isEmpty())
        layoutType = rootObj["layout"].toString();

    if (layoutType == "vlayout") {
        layout = new QVBoxLayout;
    } else if (layoutType == "hlayout") {
        layout = new QHBoxLayout;
    } else if (layoutType == "glayout") {
        layout = new QGridLayout;
    }
    else {
        error = "Required base layout";
        return nullptr;
    }

    lpVector.append(layout);

    QJsonArray elementsArray = rootObj["elements"].toArray();

    for (const QJsonValue& elementValue : elementsArray) {
        QJsonObject elementObj = elementValue.toObject();
        QString type = elementObj["type"].toString();
        QString id = elementObj["id"].toString();

        QJsonArray positionArray = elementObj["position"].toArray();
        int row = positionArray.size() > 0 ? positionArray[0].toInt() : 0;
        int col = positionArray.size() > 1 ? positionArray[1].toInt() : 0;
        int rowSpan = positionArray.size() > 2 ? positionArray[2].toInt() : 1;
        int colSpan = positionArray.size() > 3 ? positionArray[3].toInt() : 1;

        if (type == "label") {
            auto label = new QLabel(elementObj["text"].toString(), widget);

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addWidget(label, row, col, rowSpan, colSpan);
            } else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addWidget(label);
            }
        }
        else if (type == "input") {
            auto lineEdit = new QLineEdit(widget);
            lineEdit->setPlaceholderText(elementObj["placeholder"].toString());

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addWidget(lineEdit, row, col, rowSpan, colSpan);
            } else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addWidget(lineEdit);
            }

            if (!id.isEmpty()) {
                widgetMap[id] = lineEdit;
            }
        }
        else if (type == "vlayout" || type == "hlayout" || type == "glayout") {
            QLayout* nestedLayout = BuildLayout(type, elementObj);

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addLayout(nestedLayout, row, col, rowSpan, colSpan);
            } else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addLayout(nestedLayout);
            }
        }
    }

    return layout;
}


void WidgetBuilder::BuildWidget() {
    if (qJsonObject.isEmpty())
        return;

    auto layout = BuildLayout("", qJsonObject);
    widget->setLayout(layout);
    valid = true;
}

QWidget *WidgetBuilder::GetWidget() {
    return widget;
}

QString WidgetBuilder::CollectData() {
    QJsonObject collectedData;
    QWidget* widget = nullptr;
    for (auto it = widgetMap.begin(); it != widgetMap.end(); ++it) {
        const QString& id = it.key();
        widget = it.value();

        if (auto *lineEdit = qobject_cast<QLineEdit *>(widget)) {
            collectedData[id] = lineEdit->text();
        }
    }

    QJsonDocument jsonDocument(collectedData);
    QByteArray doc = jsonDocument.toJson(QJsonDocument::Indented);
    return QString::fromUtf8(doc);
}


