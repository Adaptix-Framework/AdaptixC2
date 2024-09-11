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

WidgetBuilder::~WidgetBuilder() = default;

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
            auto label = new QLabel(widget);

            label->setText( elementObj["text"].toString() );

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
        else if (type == "combo") {
            auto comboBox = new QComboBox(widget);

            comboBox->setCurrentText( elementObj["text"].toString() );

            QJsonArray itemsArray = elementObj["items"].toArray();
            for (const QJsonValue& itemValue : itemsArray) {
                comboBox->addItem(itemValue.toString());
            }

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addWidget(comboBox, row, col, rowSpan, colSpan);
            } else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addWidget(comboBox);
            }

            if (!id.isEmpty()) {
                widgetMap[id] = comboBox;
            }
        }
        else if (type == "textedit") {
            auto textEdit = new QPlainTextEdit(widget);
            textEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

            textEdit->setPlaceholderText(elementObj["placeholder"].toString());
            textEdit->setPlainText( elementObj["text"].toString() );

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addWidget(textEdit, row, col, rowSpan, colSpan);
            } else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addWidget(textEdit);
            }

            if (!id.isEmpty()) {
                widgetMap[id] = textEdit;
            }
        }
        else if (type == "checkbox") {
            auto checkBox = new QCheckBox(widget);

            checkBox->setText( elementObj["text"].toString() );
            checkBox->setChecked( elementObj["checked"].toBool(false) );

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addWidget(checkBox, row, col, rowSpan, colSpan);
            } else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addWidget(checkBox);
            }

            if (!id.isEmpty()) {
                widgetMap[id] = checkBox;
            }
        }
        if (type == "table") {
            int rowCount = elementObj["rowCount"].toInt(0);
            int columnCount = elementObj["columnCount"].toInt(0);

            QTableWidget* tableWidget = new QTableWidget(rowCount, columnCount, widget);
            tableWidget->setAutoFillBackground( false );
            tableWidget->setShowGrid( false );
            tableWidget->setSortingEnabled( true );
            tableWidget->setWordWrap( true );
            tableWidget->setCornerButtonEnabled( false );
            tableWidget->setSelectionBehavior( QAbstractItemView::SelectRows );
            tableWidget->setSelectionMode( QAbstractItemView::SingleSelection );
            tableWidget->setFocusPolicy( Qt::NoFocus );
            tableWidget->setAlternatingRowColors( true );
            tableWidget->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
            tableWidget->horizontalHeader()->setCascadingSectionResizes( true );
            tableWidget->horizontalHeader()->setHighlightSections( false );
            tableWidget->verticalHeader()->setVisible( false );
            tableWidget->verticalHeader()->setDefaultSectionSize( 12 );

            QJsonArray headersArray = elementObj["headers"].toArray();
            for (int i = 0; i < headersArray.size(); ++i) {
                tableWidget->setHorizontalHeaderItem(i, new QTableWidgetItem(headersArray[i].toString()));
            }

            QJsonArray dataArray = elementObj["data"].toArray();
            for (int i = 0; i < dataArray.size(); ++i) {
                QJsonArray rowArray = dataArray[i].toArray();
                for (int j = 0; j < rowArray.size(); ++j) {
                    QTableWidgetItem* item = new QTableWidgetItem(rowArray[j].toString());
                    tableWidget->setItem(i, j, item);
                }
            }

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addWidget(tableWidget, row, col, rowSpan, colSpan);
            } else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addWidget(tableWidget);
            }

            if (!id.isEmpty()) {
                widgetMap[id] = tableWidget;
            }
        }
        else if (type == "tab") {
            auto tabWidget = new QTabWidget(widget);

            QJsonArray tabsArray = elementObj["tabs"].toArray();
            for (const QJsonValue& tabValue : tabsArray) {
                QJsonObject tabObj = tabValue.toObject();
                QString title = tabObj["title"].toString();

                QWidget* tabContent = new QWidget();
                QLayout* tabLayout = BuildLayout("", tabObj);
                tabContent->setLayout(tabLayout);
                tabWidget->addTab(tabContent, title);
            }

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addWidget(tabWidget, row, col, rowSpan, colSpan);
            } else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addWidget(tabWidget);
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

        if (auto lineEdit = qobject_cast<QLineEdit *>(widget)) {
            collectedData[id] = lineEdit->text();
        }

        else if (auto comboBox = qobject_cast<QComboBox*>(widget)) {
            collectedData[id] = comboBox->currentText();
        }

        else if (auto textEdit = qobject_cast<QPlainTextEdit*>(widget)) {
            collectedData[id] = textEdit->toPlainText();
        }

        else if (QCheckBox* checkBox = qobject_cast<QCheckBox*>(widget)) {
            collectedData[id] = checkBox->isChecked();
        }
        else if (QTableWidget* tableWidget = qobject_cast<QTableWidget*>(widget)) {
            QJsonArray tableData;

            for (int row = 0; row < tableWidget->rowCount(); ++row) {
                QJsonArray rowData;
                for (int col = 0; col < tableWidget->columnCount(); ++col) {
                    QTableWidgetItem* item = tableWidget->item(row, col);
                    if (item) {
                        rowData.append(item->text());
                    } else {
                        rowData.append("");
                    }
                }
                tableData.append(rowData);
            }

            collectedData[id] = tableData;
        }
    }

    QJsonDocument jsonDocument(collectedData);
    QByteArray doc = jsonDocument.toJson(QJsonDocument::Indented);
    return QString::fromUtf8(doc);
}


