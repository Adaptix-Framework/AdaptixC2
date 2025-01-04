#include <Classes/WidgetBuilder.h>

SpinTable::SpinTable(int rows, int columns, QWidget* parent) : QWidget(parent)
{
    table = new QTableWidget(rows, columns, this);
    table->setAutoFillBackground( false );
    table->setShowGrid( false );
    table->setSortingEnabled( true );
    table->setWordWrap( true );
    table->setCornerButtonEnabled( false );
    table->setSelectionBehavior( QAbstractItemView::SelectRows );
    table->setSelectionMode( QAbstractItemView::SingleSelection );
    table->setFocusPolicy( Qt::NoFocus );
    table->setAlternatingRowColors( true );
    table->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    table->horizontalHeader()->setCascadingSectionResizes( true );
    table->horizontalHeader()->setHighlightSections( false );
    table->verticalHeader()->setVisible( false );

    buttonAdd   = new QPushButton("Add", this);
    buttonClear = new QPushButton("Clear", this);

    layout = new QGridLayout( this );
    layout->addWidget(table, 0, 0, 1, 2);
    layout->addWidget(buttonAdd, 1, 0, 1, 1);
    layout->addWidget(buttonClear, 1, 1, 1, 1);

    this->setLayout(layout);

    QObject::connect(buttonAdd, &QPushButton::clicked, this, [&]()
    {
        if (table->rowCount() < 1 )
            table->setRowCount(1 );
        else
            table->setRowCount(table->rowCount() + 1 );

        table->setItem(table->rowCount() - 1, 0, new QTableWidgetItem() );
        table->selectRow(table->rowCount() - 1 );
    } );

    QObject::connect(buttonClear, &QPushButton::clicked, this, [&]()
    {
        table->setRowCount(0);
    } );
}

FileSelector::FileSelector(QWidget* parent) : QWidget(parent)
{
    input = new QLineEdit(this);
    input->setReadOnly(true);

    button = new QPushButton(this);
    button->setIcon(QIcon::fromTheme("folder"));

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(input);
    layout->addWidget(button);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout);

    connect(button, &QPushButton::clicked, this, [&]()
    {
        QString selectedFile = QFileDialog::getOpenFileName(this, "Select a file");
        if (selectedFile.isEmpty())
            return;

        QString filePath = selectedFile;
        input->setText(filePath);

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return;

        QByteArray fileData = file.readAll();
        file.close();

        content = QString::fromUtf8(fileData.toBase64());
    } );
}

/// MAIN CLASS

WidgetBuilder::WidgetBuilder(const QByteArray& jsonData)
{
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

QString WidgetBuilder::GetError()
{
    return error;
}

QLayout* WidgetBuilder::BuildLayout(QString layoutType, QJsonObject rootObj)
{
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
        else if (type == "vline" || type == "hline") {
            auto line = new QFrame(widget);

            if (type == "vline"){
                line->setFrameShape(QFrame::VLine);
                line->setMinimumHeight(25);
            }
            else {
                line->setFrameShape(QFrame::HLine);
                line->setMinimumWidth(25);
            }

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addWidget(line, row, col, rowSpan, colSpan);
            } else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addWidget(line);
            }
        }
        else if (type == "input") {
            auto lineEdit = new QLineEdit(widget);

            lineEdit->setPlaceholderText(elementObj["placeholder"].toString());
            lineEdit->setText(elementObj["text"].toString());

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addWidget(lineEdit, row, col, rowSpan, colSpan);
            } else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addWidget(lineEdit);
            }

            if (!id.isEmpty()) {
                widgetMap[id] = lineEdit;
            }
        }
        else if (type == "file_selector") {
            auto selector = new FileSelector(widget);

            selector->input->setPlaceholderText(elementObj["placeholder"].toString());

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addWidget(selector, row, col, rowSpan, colSpan);
            } else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addWidget(selector);
            }

            if (!id.isEmpty()) {
                widgetMap[id] = selector;
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
        else if (type == "spinbox") {
            auto spinBox = new QSpinBox(widget);

            if( elementObj.contains("min") && elementObj["min"].toDouble() )
                spinBox->setMinimum( elementObj["min"].toDouble() );

            if( elementObj.contains("max") && elementObj["max"].toDouble() )
                spinBox->setMaximum( elementObj["max"].toDouble() );

            if( elementObj.contains("value") && elementObj["value"].toDouble() )
                spinBox->setValue( elementObj["value"].toDouble() );

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addWidget(spinBox, row, col, rowSpan, colSpan);
            } else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addWidget(spinBox);
            }

            if (!id.isEmpty()) {
                widgetMap[id] = spinBox;
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
        else if (type == "table") {
            int rowCount = elementObj["row_count"].toInt(0);
            int columnCount = elementObj["column_count"].toInt(0);

            auto tableWidget = new QTableWidget(rowCount, columnCount, widget);
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
        else if (type == "spin_table") {
            int rowCount = elementObj["row_count"].toInt(0);
            int columnCount = elementObj["column_count"].toInt(0);

            auto spinTable = new SpinTable(rowCount, columnCount, widget);

            QJsonArray headersArray = elementObj["headers"].toArray();
            for (int i = 0; i < headersArray.size(); ++i) {
                spinTable->table->setHorizontalHeaderItem(i, new QTableWidgetItem(headersArray[i].toString()));
            }

            QJsonArray dataArray = elementObj["data"].toArray();
            for (int i = 0; i < dataArray.size(); ++i) {
                QJsonArray rowArray = dataArray[i].toArray();
                for (int j = 0; j < rowArray.size(); ++j) {
                    QTableWidgetItem* item = new QTableWidgetItem(rowArray[j].toString());
                    spinTable->table->setItem(i, j, item);
                }
            }

            if (auto gridLayout = qobject_cast<QGridLayout*>(layout)) {
                gridLayout->addWidget(spinTable->table, row, col, rowSpan, colSpan);
            }
            else if (auto boxLayout = qobject_cast<QBoxLayout*>(layout)) {
                boxLayout->addWidget(spinTable);
            }

            if (!id.isEmpty()) {
                widgetMap[id] = spinTable;
            }
        }
        else if (type == "tab") {
            auto tabWidget = new QTabWidget(widget);

            QJsonArray tabsArray = elementObj["tabs"].toArray();
            for (const QJsonValue& tabValue : tabsArray) {
                QJsonObject tabObj = tabValue.toObject();
                QString title = tabObj["title"].toString();

                auto tabContent = new QWidget();
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


void WidgetBuilder::BuildWidget()
{
    if (qJsonObject.isEmpty())
        return;

    auto layout = BuildLayout("", qJsonObject);
    widget->setLayout(layout);
    valid = true;
}

QWidget *WidgetBuilder::GetWidget()
{
    return widget;
}

QString WidgetBuilder::CollectData()
{
    QJsonObject collectedData;
    QWidget* widget = nullptr;
    for (auto it = widgetMap.begin(); it != widgetMap.end(); ++it) {
        const QString& id = it.key();
        widget = it.value();

        if (auto lineEdit = qobject_cast<QLineEdit*>(widget)) {
            collectedData[id] = lineEdit->text();
        }

        else if (auto fileSelector = qobject_cast<FileSelector*>(widget)) {
            collectedData[id] = fileSelector->content;
        }

        else if (auto lineEdit = qobject_cast<QLineEdit*>(widget)) {
            collectedData[id] = lineEdit->text();
        }

        else if (auto comboBox = qobject_cast<QComboBox*>(widget)) {
            collectedData[id] = comboBox->currentText();
        }

        else if (auto spinBox = qobject_cast<QSpinBox*>(widget)) {
            collectedData[id] = spinBox->value();
        }

        else if (auto plainTextEdit = qobject_cast<QPlainTextEdit*>(widget)) {
            collectedData[id] = plainTextEdit->toPlainText();
        }

        else if (auto checkBox = qobject_cast<QCheckBox*>(widget)) {
            collectedData[id] = checkBox->isChecked();
        }

        else if (auto tableWidget = qobject_cast<QTableWidget*>(widget)) {
            QJsonArray tableData;

            for (int row = 0; row < tableWidget->rowCount(); ++row) {
                QJsonArray rowData;
                for (int col = 0; col < tableWidget->columnCount(); ++col) {
                    QTableWidgetItem* item = tableWidget->item(row, col);
                    if (item) {
                        rowData.append(item->text());
                        qDebug() << item->text();
                    } else {
                        rowData.append("");
                    }
                }
                tableData.append(rowData);
            }
            collectedData[id] = tableData;
        }

        else if (auto spinTable = qobject_cast<SpinTable*>(widget)) {
            QJsonArray tableData;

            for (int row = 0; row < spinTable->table->rowCount(); ++row) {
                QJsonArray rowData;
                for (int col = 0; col < spinTable->table->columnCount(); ++col) {
                    QTableWidgetItem *item = spinTable->table->item(row, col);
                    if (item) {
                        rowData.append(item->text());
                    } else {
                        rowData.append("");
                    }
                    tableData.append(rowData);
                }
            }
            collectedData[id] = tableData;
        }
    }

    QJsonDocument jsonDocument(collectedData);
    QByteArray doc = jsonDocument.toJson(QJsonDocument::Indented);
    return QString::fromUtf8(doc);
}

void WidgetBuilder::FillData(QString jsonString)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonString.toUtf8());
    if (!jsonDoc.isObject()) {
        return;
    }

    QJsonObject data = jsonDoc.object();

    for (auto it = data.begin(); it != data.end(); ++it) {
        QString id = it.key();
        QJsonValue value = it.value();

        if (!widgetMap.contains(id)) {
            continue;
        }

        QWidget* widget = widgetMap[id];

        if (auto lineEdit = qobject_cast<QLineEdit*>(widget)) {
            lineEdit->setText(value.toString());
        }

        else if (auto comboBox = qobject_cast<QComboBox*>(widget)) {
            int index = comboBox->findText(value.toString());
            if (index != -1) {
                comboBox->setCurrentIndex(index);
            }
        }

        else if (auto spinBox = qobject_cast<QSpinBox*>(widget)) {
            spinBox->setValue(value.toDouble());
        }

        else if (auto plainTextEdit = qobject_cast<QPlainTextEdit*>(widget)) {
            plainTextEdit->setPlainText(value.toString());
        }

        else if (auto fileSelector = qobject_cast<FileSelector*>(widget)) {
            fileSelector->input->setText("selected...");
            fileSelector->button->setEnabled(false);
        }

        else if (auto checkBox = qobject_cast<QCheckBox*>(widget)) {
            checkBox->setChecked(value.toBool());
        }

        else if (auto tableWidget = qobject_cast<QTableWidget*>(widget)) {
            QJsonArray tableData = value.toArray();

            for (int row = 0; row < tableData.size(); ++row) {
                QJsonArray rowArray = tableData[row].toArray();
                for (int col = 0; col < rowArray.size(); ++col) {
                    QString cellText = rowArray[col].toString();
                    QTableWidgetItem* item = tableWidget->item(row, col);
                    if (!item) {
                        item = new QTableWidgetItem();
                        tableWidget->setItem(row, col, item);
                    }
                    item->setText(cellText);
                }
            }
        }

        else if (auto spinTable = qobject_cast<SpinTable*>(widget)) {
            QJsonArray tableData = value.toArray();

            for (int row = 0; row < tableData.size(); ++row) {
                QJsonArray rowArray = tableData[row].toArray();
                for (int col = 0; col < rowArray.size(); ++col) {
                    QString cellText = rowArray[col].toString();
                    QTableWidgetItem* item = spinTable->table->item(row, col);
                    if (!item) {
                        item = new QTableWidgetItem();
                        spinTable->table->setItem(row, col, item);
                    }
                    item->setText(cellText);
                }
            }
        }
    }
}
