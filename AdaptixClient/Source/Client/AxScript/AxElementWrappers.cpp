#include <Client/AxScript/AxElementWrappers.h>
#include <QJSEngine>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFrame>
#include <QDateEdit>
#include <QDialog>
#include <QMenu>

/// MENU

AxActionWrapper::AxActionWrapper(const QString& text, const QJSValue& handler, QJSEngine* engine, QObject* parent) : AbstractAxMenuItem(parent), handler(handler), engine(engine) { pAction = new QAction(text, this); }

QAction* AxActionWrapper::action() const { return this->pAction; }

void AxActionWrapper::setContext(QVariantList context) {
    QObject::disconnect(pAction, nullptr, this, nullptr);

    QObject::connect(pAction, &QAction::triggered, this, [this, context]() { triggerWithContext(context); });
}

// void AxActionWrapper::triggerWithContext(const QVariantMap &context) const {
void AxActionWrapper::triggerWithContext(const QVariantList& arg) const {
    if (!handler.isCallable())
        return;

    QJSValue jsContext = engine->toScriptValue(arg);
    if (this->handler.isCallable())
        this->handler.call({ jsContext });
}



AxSeparatorWrapper::AxSeparatorWrapper(QObject* parent) : AbstractAxMenuItem(parent) {
    pAction = new QAction(this);
    pAction->setSeparator(true);
}

QAction* AxSeparatorWrapper::action() const { return this->pAction; }

void AxSeparatorWrapper::setContext(QVariantList context) {}



AxMenuWrapper::AxMenuWrapper(const QString& title, QObject* parent) : AbstractAxMenuItem(parent) { pMenu = new QMenu(title); }

QMenu* AxMenuWrapper::menu() const { return this->pMenu; }

void AxMenuWrapper::setContext(const QVariantList context)
{
    for (auto item : items)
        item->setContext(context);
}

void AxMenuWrapper::addItem(AbstractAxMenuItem* axItem)
{
    items.append(axItem);

    if (auto* wrapper1 = dynamic_cast<AxSeparatorWrapper*>(axItem)) {
        this->pMenu->addAction(wrapper1->action());
    }
    else if (auto* wrapper2 = dynamic_cast<AxActionWrapper*>(axItem)) {
        this->pMenu->addAction(wrapper2->action());
    }
    else if (auto* wrapper3 = dynamic_cast<AxMenuWrapper*>(axItem)) {
        this->pMenu->addMenu(wrapper3->menu());
    }
}



/// LAYOUT

AxBoxLayoutWrapper::AxBoxLayoutWrapper(const QBoxLayout::Direction dir, QObject* parent) : QObject(parent) { boxLayout = new QBoxLayout(dir); }

QBoxLayout* AxBoxLayoutWrapper::layout() const { return boxLayout; }

void AxBoxLayoutWrapper::addWidget(QObject* wrapper) const {
    if (auto* formElement = dynamic_cast<AbstractAxVisualElement*>(wrapper))
        boxLayout->addWidget(formElement->widget());
    else if (auto* spacerElement = qobject_cast<AxSpacerWrapper*>(wrapper))
        boxLayout->addItem(spacerElement->widget());
}

/// GRID LAYOUT

AxGridLayoutWrapper::AxGridLayoutWrapper(QObject* parent) : QObject(parent) { gridLayout = new QGridLayout(); }

QGridLayout* AxGridLayoutWrapper::layout() const { return gridLayout; }

void AxGridLayoutWrapper::addWidget(QObject* wrapper, const int row, const int col, const int rowSpan, const int colSpan) const {
    if (auto* formElement = dynamic_cast<AbstractAxVisualElement*>(wrapper))
        gridLayout->addWidget(formElement->widget(), row, col, rowSpan, colSpan);
    else if (auto* spacerElement = qobject_cast<AxSpacerWrapper*>(wrapper))
        gridLayout->addItem(spacerElement->widget(), row, col, rowSpan, colSpan);
}


/// LINE

AxLineWrapper::AxLineWrapper(const QFrame::Shape dir, QObject* parent) : QObject(parent) {
    line = new QFrame();
    line->setFrameShape(dir);
    if (dir == QFrame::VLine)
        line->setMinimumHeight(25);
    else
        line->setMinimumWidth(25);
}

QFrame* AxLineWrapper::widget() const { return line; }


/// SPACER

AxSpacerWrapper::AxSpacerWrapper(const int w, const int h, const QSizePolicy::Policy hData, const QSizePolicy::Policy vData, QObject* parent) : QObject(parent) { spacer = new QSpacerItem(w, h, hData, vData); }

QSpacerItem* AxSpacerWrapper::widget() const { return spacer; }


/// TEXTLINE

AxTextLineWrapper::AxTextLineWrapper(QLineEdit* edit, QObject* parent) : QObject(parent), lineedit(edit) {
    connect(lineedit, &QLineEdit::textChanged,      this, &AxTextLineWrapper::textChanged);
    connect(lineedit, &QLineEdit::textEdited,       this, &AxTextLineWrapper::textEdited);
    connect(lineedit, &QLineEdit::returnPressed,    this, &AxTextLineWrapper::returnPressed);
    connect(lineedit, &QLineEdit::editingFinished,  this, &AxTextLineWrapper::editingFinished);
}

QVariant AxTextLineWrapper::jsonMarshal() const { return lineedit->text(); }

void AxTextLineWrapper::jsonUnmarshal(const QVariant& value) { lineedit->setText(value.toString()); }

QLineEdit* AxTextLineWrapper::widget() const { return lineedit; }

QString AxTextLineWrapper::text() const { return lineedit->text(); }

void AxTextLineWrapper::setText(const QString& text) const { lineedit->setText(text); }

void AxTextLineWrapper::setPlaceholder(const QString& text) const { lineedit->setPlaceholderText(text); }

/// COMBO

AxComboBoxWrapper::AxComboBoxWrapper(QComboBox* comboBox, QObject* parent) : QObject(parent), comboBox(comboBox) {
    connect(comboBox, &QComboBox::currentTextChanged,  this, &AxComboBoxWrapper::currentTextChanged);
    connect(comboBox, &QComboBox::currentIndexChanged, this, &AxComboBoxWrapper::currentIndexChanged);
}

QVariant AxComboBoxWrapper::jsonMarshal() const { return comboBox->currentText(); }

void AxComboBoxWrapper::jsonUnmarshal(const QVariant& value) {
    int index = comboBox->findText(value.toString());
    if (index != -1)
        comboBox->setCurrentIndex(index);
}

QComboBox * AxComboBoxWrapper::widget() const { return comboBox; }

void AxComboBoxWrapper::addItem(const QString& text) const { comboBox->addItem(text); }

void AxComboBoxWrapper::addItems(const QJSValue& array) const {
    if (!array.isArray())
        return;

    QStringList items;
    const int length = array.property("length").toInt();
    for (int i = 0; i < length; ++i) {
        QJSValue val = array.property(i);
        items << val.toString();
    }

    comboBox->addItems(items);
}

void AxComboBoxWrapper::setItems(const QJSValue &array) const {
    if (!array.isArray())
        return;

    QStringList items;
    const int length = array.property("length").toInt();
    for (int i = 0; i < length; ++i) {
        QJSValue val = array.property(i);
        items << val.toString();
    }

    comboBox->clear();
    comboBox->addItems(items);
}

void AxComboBoxWrapper::clear() const { comboBox->clear(); }

QString AxComboBoxWrapper::currentText() const { return comboBox->currentText(); }

int AxComboBoxWrapper::currentIndex() const { return comboBox->currentIndex(); }

void AxComboBoxWrapper::setCurrentIndex(const int index) const { comboBox->setCurrentIndex(index); }

/// SPIN

AxSpinBoxWrapper::AxSpinBoxWrapper(QSpinBox* spin, QObject* parent) : QObject(parent), spin(spin) {
    connect(spin, &QSpinBox::valueChanged, this, &AxSpinBoxWrapper::valueChanged);
}

QVariant AxSpinBoxWrapper::jsonMarshal() const { return spin->value(); }

void AxSpinBoxWrapper::jsonUnmarshal(const QVariant& value) { spin->setValue(value.toInt()); }

QSpinBox* AxSpinBoxWrapper::widget() const { return spin; }

int AxSpinBoxWrapper::value() const { return spin->value(); }

void AxSpinBoxWrapper::setValue(const int value) const { spin->setValue(value); }

void AxSpinBoxWrapper::setRange(const int min, const int max) const { spin->setRange(min, max); }

/// DATE

AxDateEditWrapper::AxDateEditWrapper(QDateEdit* edit, const QString &format, QObject* parent) : QObject(parent), dateedit(edit) {
    edit->setCalendarPopup(true);
    edit->setDisplayFormat(format);
}

QVariant AxDateEditWrapper::jsonMarshal() const { return dateString(); }

void AxDateEditWrapper::jsonUnmarshal(const QVariant& value) { setDateString(value.toString()); }

QDateEdit* AxDateEditWrapper::widget() const { return dateedit; }

QString AxDateEditWrapper::dateString() const { return dateedit->date().toString(Qt::ISODate); }

void AxDateEditWrapper::setDateString(const QString& date) const { dateedit->setDate(QDate::fromString(date, Qt::ISODate)); }

/// TIME

AxTimeEditWrapper::AxTimeEditWrapper(QTimeEdit* edit, const QString &format, QObject* parent) : QObject(parent), timeedit(edit) {
    timeedit->setDisplayFormat(format);
}

QVariant AxTimeEditWrapper::jsonMarshal() const { return timeString(); }

void AxTimeEditWrapper::jsonUnmarshal(const QVariant& value) { setTimeString(value.toString()); }

QTimeEdit * AxTimeEditWrapper::widget() const { return timeedit; }

QString AxTimeEditWrapper::timeString() const { return timeedit->time().toString("HH:mm:ss"); }

void AxTimeEditWrapper::setTimeString(const QString& time) const { timeedit->setTime(QTime::fromString(time, "HH:mm:ss")); }

/// TEXTMULTI

AxTextMultiWrapper::AxTextMultiWrapper(QPlainTextEdit* edit, QObject* parent) : QObject(parent), textedit(edit) {}

QVariant AxTextMultiWrapper::jsonMarshal() const { return text(); }

void AxTextMultiWrapper::jsonUnmarshal(const QVariant& value) { setText(value.toString()); }

QPlainTextEdit * AxTextMultiWrapper::widget() const { return textedit; }

QString AxTextMultiWrapper::text() const { return textedit->toPlainText(); }

void AxTextMultiWrapper::setText(const QString& text) const { textedit->setPlainText(text); }

void AxTextMultiWrapper::appendText(const QString &text) const { textedit->appendPlainText(text); }

void AxTextMultiWrapper::setPlaceholder(const QString& text) const { textedit->setPlaceholderText(text); }

void AxTextMultiWrapper::setReadOnly(const bool &readonly) const { textedit->setReadOnly(readonly); }

/// CHECK

AxCheckBoxWrapper::AxCheckBoxWrapper(QCheckBox* box, QObject* parent) : QObject(parent), check(box) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(check, &QCheckBox::checkStateChanged, this, &AxCheckBoxWrapper::stateChanged);
#else
    connect(check, &QCheckBox::stateChanged, this, &AxCheckBoxWrapper::stateChanged);
#endif
}

QVariant AxCheckBoxWrapper::jsonMarshal() const { return isChecked(); }

void AxCheckBoxWrapper::jsonUnmarshal(const QVariant& value) { setChecked(value.toBool()); }

QCheckBox * AxCheckBoxWrapper::widget() const { return check; }

bool AxCheckBoxWrapper::isChecked() const { return check->isChecked(); }

void AxCheckBoxWrapper::setChecked(const bool checked) const { check->setChecked(checked); }

/// FILE SELECTOR

AxFileSelectorWrapper::AxFileSelectorWrapper(FileSelector* selector, QObject* parent) : QObject(parent), selector(selector) {}

FileSelector* AxFileSelectorWrapper::widget() const { return selector; }

QVariant AxFileSelectorWrapper::jsonMarshal() const { return selector->content; }

void AxFileSelectorWrapper::jsonUnmarshal(const QVariant& value) {
    selector->content = value.toString();
    selector->input->setText("Selected...");
}

void AxFileSelectorWrapper::setPlaceholder(const QString& text) const { selector->input->setPlaceholderText(text); }

/// LABEL

AxLabelWrapper::AxLabelWrapper(QLabel* label, QObject* parent) : QObject(parent), label(label) {}

QLabel* AxLabelWrapper::widget() const { return label; }

void AxLabelWrapper::setText(const QString& text) const { label->setText(text); }

QString AxLabelWrapper::text() const { return label->text(); }

/// TAB

AxTabWrapper::AxTabWrapper(QTabWidget* tabs, QObject* parent) : QObject(parent), tabs(tabs) {}

QTabWidget* AxTabWrapper::widget() const { return tabs; }

void AxTabWrapper::addTab(QObject* wrapper, const QString &title) const {
    if (auto* formElement = dynamic_cast<AbstractAxVisualElement*>(wrapper))
        tabs->addTab(formElement->widget(), title);
}

/// BUTTON

AxButtonWrapper::AxButtonWrapper(QPushButton* btn, QObject* parent) : QObject(parent), button(btn) {
    connect(button, &QPushButton::clicked, this, &AxButtonWrapper::clicked);
}

QPushButton* AxButtonWrapper::widget() const { return button; }

/// PANEL

AxPanelWrapper::AxPanelWrapper(QWidget* w, QObject* parent) : QObject(parent), panel(w) {}

QWidget* AxPanelWrapper::widget() const { return panel; }

void AxPanelWrapper::setLayout(QObject* layoutWrapper) const {
    if (auto* grid = qobject_cast<AxGridLayoutWrapper*>(layoutWrapper))
        panel->setLayout(grid->layout());
    else if (auto* box = qobject_cast<AxBoxLayoutWrapper*>(layoutWrapper))
        panel->setLayout(box->layout());
}

/// CONTAINER

AxContainerWrapper::AxContainerWrapper(QJSEngine* jsEngine, QObject* parent) : QObject(parent), engine(jsEngine) {}

void AxContainerWrapper::put(const QString& id, QObject* wrapper) { widgets[id] = wrapper; }

QObject* AxContainerWrapper::get(const QString &id) { return widgets[id]; }

bool AxContainerWrapper::contains(const QString &id) const { return widgets.contains(id); }

void AxContainerWrapper::remove(const QString& id)
{
    if (widgets.contains(id))
        widgets.remove(id);
}

QString AxContainerWrapper::toJson()
{
    QJsonObject json;
    for (auto it = widgets.begin(); it != widgets.end(); ++it) {
        auto* formElement = dynamic_cast<AbstractAxElement*>(it.value());
        if (!formElement)
            continue;

        QJsonValue value = QJsonValue::fromVariant(formElement->jsonMarshal());
        json.insert(it.key(), value);
    }

    QJsonDocument doc(json);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

void AxContainerWrapper::fromJson(const QString& jsonString)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return;

    QJsonObject obj = doc.object();

    for (auto it = widgets.begin(); it != widgets.end(); ++it) {
        auto* formElement = dynamic_cast<AbstractAxElement*>(it.value());
        if (!formElement)
            continue;

        QString key = it.key();
        if (obj.contains(key))
            formElement->jsonUnmarshal(obj.value(key).toVariant());
    }
}

QJSValue AxContainerWrapper::toProperty()
{
     QJSValue result = engine->newObject();

     for (auto it = widgets.begin(); it != widgets.end(); ++it) {
         auto* formElement = dynamic_cast<AbstractAxElement*>(it.value());
         if (!formElement)
             continue;
         result.setProperty(it.key(), formElement->jsonMarshal().toString());
     }
     return result;
}

void AxContainerWrapper::fromProperty(const QJSValue &obj)
{
     if (!obj.isObject())
         return;

     for (auto it = widgets.begin(); it != widgets.end(); ++it) {
         auto* formElement = dynamic_cast<AbstractAxElement*>(it.value());
         if (!formElement)
             continue;

         QString key = it.key();
         if (obj.hasProperty(key))
             formElement->jsonUnmarshal(obj.property(key).toString());
     }
}

/// DIALOG

AxDialogWrapper::AxDialogWrapper(const QString& title, QWidget* parent) : QObject(parent) {
    dialog = new QDialog(parent);
    dialog->setWindowTitle(title);
    layout = new QVBoxLayout(dialog);

    buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);

    layout->addWidget(buttons);
    dialog->setLayout(layout);

    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
}

void AxDialogWrapper::setLayout(QObject* layoutWrapper) {
    if (userLayout) {
        layout->removeItem(userLayout);
        delete userLayout;
        userLayout = nullptr;
    }

    if (auto* grid = qobject_cast<AxGridLayoutWrapper*>(layoutWrapper))
        userLayout = grid->layout();
    else if (auto* box = qobject_cast<AxBoxLayoutWrapper*>(layoutWrapper))
        userLayout = box->layout();

    if (userLayout)
        layout->insertLayout(0, userLayout);
}

void AxDialogWrapper::setSize(const int w, const int h ) const { dialog->resize(w, h); }

bool AxDialogWrapper::exec() const { return dialog->exec() == QDialog::Accepted; }

void AxDialogWrapper::close() const { dialog->close(); }
