#include <Client/AxScript/AxElementWrappers.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxScriptUtils.h>
#include <Client/AuthProfile.h>
#include <Client/PagedTableHelper.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Agent/Agent.h>
#include <Utils/NonBlockingDialogs.h>
#include <Utils/CustomElements/BoldHeaderView.h>
#include <Utils/CustomElements/Delegates.h>
#include <Utils/CustomElements/PageNavBar.h>

#include <oclero/qlementine/widgets/Menu.hpp>
#include <oclero/qlementine/widgets/Switch.hpp>
#include <oclero/qlementine/widgets/SegmentedControl.hpp>

#include <QJSEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFrame>
#include <QDateEdit>
#include <QDialog>
#include <QHeaderView>
#include <QMenu>
#include <QApplication>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>

namespace {

QString credFieldToSortKey(const QString& field)
{
    if (field == QLatin1String("id"))        return QStringLiteral("CredId");
    if (field == QLatin1String("username"))  return QStringLiteral("Username");
    if (field == QLatin1String("password"))  return QStringLiteral("Password");
    if (field == QLatin1String("realm"))     return QStringLiteral("Realm");
    if (field == QLatin1String("type"))      return QStringLiteral("Type");
    if (field == QLatin1String("tag"))       return QStringLiteral("Tag");
    if (field == QLatin1String("date"))      return QStringLiteral("Date");
    if (field == QLatin1String("storage"))   return QStringLiteral("Storage");
    if (field == QLatin1String("agent_id"))  return QStringLiteral("AgentId");
    if (field == QLatin1String("host"))      return QStringLiteral("Host");
    return {};
}

QString targetFieldToSortKey(const QString& field)
{
    if (field == QLatin1String("id"))       return QStringLiteral("TargetId");
    if (field == QLatin1String("computer")) return QStringLiteral("Computer");
    if (field == QLatin1String("domain"))   return QStringLiteral("Domain");
    if (field == QLatin1String("address"))  return QStringLiteral("Address");
    if (field == QLatin1String("tag"))      return QStringLiteral("Tag");
    if (field == QLatin1String("os"))       return QStringLiteral("Os");
    if (field == QLatin1String("os_desc"))  return QStringLiteral("OsDesk");
    if (field == QLatin1String("info"))     return QStringLiteral("Info");
    if (field == QLatin1String("date"))     return QStringLiteral("Date");
    if (field == QLatin1String("alive"))    return QStringLiteral("Alive");
    return {};
}

QString downloadFieldToSortKey(const QString& field)
{
    if (field == QLatin1String("id"))         return QStringLiteral("FileId");
    if (field == QLatin1String("agent_id"))   return QStringLiteral("AgentId");
    if (field == QLatin1String("agent_name")) return QStringLiteral("AgentName");
    if (field == QLatin1String("user"))       return QStringLiteral("User");
    if (field == QLatin1String("computer"))   return QStringLiteral("Computer");
    if (field == QLatin1String("filename"))   return QStringLiteral("RemotePath");
    if (field == QLatin1String("total_size")) return QStringLiteral("TotalSize");
    if (field == QLatin1String("recv_size"))  return QStringLiteral("RecvSize");
    if (field == QLatin1String("state"))      return QStringLiteral("State");
    if (field == QLatin1String("date"))       return QStringLiteral("Date");
    return {};
}

AuthProfile* profileFromEngine(AxScriptEngine* jsEngine)
{
    if (!jsEngine || !jsEngine->manager())
        return nullptr;
    AdaptixWidget* aw = jsEngine->manager()->GetAdaptix();
    return aw ? aw->GetProfile() : nullptr;
}

QWidget* selectorTransientHost(AxScriptEngine* jsEngine)
{
    if (jsEngine && jsEngine->manager()) {
        if (AdaptixWidget* aw = jsEngine->manager()->GetAdaptix()) {
            if (QWidget* w = aw->window())
                return w;
        }
    }
    if (QWidget* active = QApplication::activeWindow())
        return active;
    const auto tops = QApplication::topLevelWidgets();
    for (QWidget* w : tops) {
        if (w && w->isVisible() && w->isWindow())
            return w;
    }
    return nullptr;
}

void prepareSelectorDialog(QDialog* dialog, const QString& title, AxScriptEngine* jsEngine)
{
    if (!dialog)
        return;

    if (dialog->parentWidget())
        dialog->setParent(nullptr);

    dialog->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
    dialog->setWindowTitle(title);
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setModal(true);
    dialog->setMinimumSize(640, 400);
    if (dialog->width() < 200 || dialog->height() < 150)
        dialog->resize(900, 520);

    if (QWidget* host = selectorTransientHost(jsEngine)) {
        const QRect pr = host->frameGeometry();
        const QSize sz = dialog->size().expandedTo(dialog->minimumSize());
        dialog->move(pr.center() - QPoint(sz.width() / 2, sz.height() / 2));
    } else if (QScreen* scr = QGuiApplication::primaryScreen()) {
        const QRect ag = scr->availableGeometry();
        dialog->move(ag.center() - dialog->rect().center());
    }
}

int execSelectorDialog(QDialog* dialog, AxScriptEngine* jsEngine)
{
    if (!dialog)
        return QDialog::Rejected;

    prepareSelectorDialog(dialog, dialog->windowTitle(), jsEngine);

    dialog->ensurePolished();
    dialog->winId();
    if (QWidget* host = selectorTransientHost(jsEngine)) {
        host->winId();
        if (QWindow* dw = dialog->windowHandle()) {
            if (QWindow* hw = host->windowHandle())
                dw->setTransientParent(hw);
        }
    }

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    QApplication::alert(dialog);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    return dialog->exec();
}

static constexpr int kSelectorDefaultPageSize = 50;

void configureSelectorPagination(PageNavBar* bar, PagedTableHelper* helper, int size = kSelectorDefaultPageSize)
{
    if (!bar || !helper)
        return;
    bar->setIsolated(true);
    bar->setPageSize(size, /*syncGlobal=*/false);
    helper->setPageSize(bar->pageSize());
}

void fillSelectorHeaders(const QJSValue& headers, const QMap<QString, QString>& fieldMap, const QStringList& defaultKeys, const QString& hiddenLabel, const QString& hiddenKey, QVector<QString>& headerLabels, QVector<QString>& fieldKeys)
{
    headerLabels.clear();
    fieldKeys.clear();

    const bool headersOk = headers.isArray();
    const int length = headersOk ? headers.property(QStringLiteral("length")).toInt() : 0;
    for (int i = 0; i < length; ++i) {
        const QString val = headers.property(i).toString();
        if (fieldMap.contains(val)) {
            headerLabels.append(fieldMap.value(val));
            fieldKeys.append(val);
        }
    }

    if (fieldKeys.isEmpty()) {
        for (const QString& val : defaultKeys) {
            if (fieldMap.contains(val)) {
                headerLabels.append(fieldMap.value(val));
                fieldKeys.append(val);
            }
        }
    }

    headerLabels.append(hiddenLabel);
    fieldKeys.append(hiddenKey);
}

void applySelectorTableColumns(QTableView* tableView, const QVector<QString>& headerLabels)
{
    if (!tableView || headerLabels.isEmpty())
        return;
    const int lastCol = headerLabels.size() - 1;
    if (lastCol >= 0)
        tableView->hideColumn(lastCol);
    if (headerLabels.size() > 2) {
        for (int i = 0; i < headerLabels.size() - 2; ++i)
            tableView->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
}

QJSValue emptyJsArray(AxScriptEngine* jsEngine)
{
    if (!jsEngine || !jsEngine->engine())
        return {};
    return jsEngine->engine()->newArray(0);
}

}

/// MENU

AxActionWrapper::AxActionWrapper(const QString& text, const QJSValue& handler, QJSEngine* engine, QObject* parent) : AbstractAxMenuItem(parent), handler(handler), engine(engine) { pAction = new QAction(text, this); }

QAction* AxActionWrapper::action() const { return this->pAction; }

void AxActionWrapper::setContext(QVariantList context)
{
    disconnect(pAction, nullptr, this, nullptr);
    connect(pAction, &QAction::triggered, this, [this, context]() { triggerWithContext(context); }, Qt::QueuedConnection);
}

void AxActionWrapper::triggerWithContext(const QVariantList& arg) const
{
    if (!handler.isCallable())
        return;

    QJSValue jsContext = engine->toScriptValue(arg);
    if (this->handler.isCallable())
        this->handler.call({ jsContext });
}

void AxActionWrapper::setIcon(const QString& resourcePath)
{
    if (pAction)
        pAction->setIcon(AxScriptUtils::resolveMenuIcon(resourcePath));
}



AxSeparatorWrapper::AxSeparatorWrapper(QObject* parent) : AbstractAxMenuItem(parent)
{
    pAction = new QAction(this);
    pAction->setSeparator(true);
}

QAction* AxSeparatorWrapper::action() const { return this->pAction; }

void AxSeparatorWrapper::setContext(QVariantList context) {}



AxMenuWrapper::AxMenuWrapper(const QString& title, QObject* parent) : AbstractAxMenuItem(parent) { pMenu = new oclero::qlementine::Menu(title); }

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

void AxMenuWrapper::setIcon(const QString& resourcePath)
{
    if (pMenu)
        pMenu->setIcon(AxScriptUtils::resolveMenuIcon(resourcePath));
}



/// LAYOUT

AxBoxLayoutWrapper::AxBoxLayoutWrapper(const QBoxLayout::Direction dir, QObject* parent) : QObject(parent) { boxLayout = new QBoxLayout(dir); }

QBoxLayout* AxBoxLayoutWrapper::layout() const { return boxLayout; }

void AxBoxLayoutWrapper::addWidget(QObject* wrapper) const
{
    if (auto* formElement = dynamic_cast<AbstractAxVisualElement*>(wrapper))
        boxLayout->addWidget(formElement->widget());
    else if (auto* spacerElement = qobject_cast<AxSpacerWrapper*>(wrapper))
        boxLayout->addItem(spacerElement->widget());
}

void AxBoxLayoutWrapper::addStretch(const int stretch) const
{
    if (boxLayout)
        boxLayout->addStretch(stretch);
}

void AxBoxLayoutWrapper::setContentsMargins(const int left, const int top, const int right, const int bottom) const
{
    if (boxLayout)
        boxLayout->setContentsMargins(left, top, right, bottom);
}

void AxBoxLayoutWrapper::setSpacing(const int spacing) const
{
    if (boxLayout)
        boxLayout->setSpacing(spacing);
}

/// GRID LAYOUT

AxGridLayoutWrapper::AxGridLayoutWrapper(QObject* parent) : QObject(parent) { gridLayout = new QGridLayout(); }

QGridLayout* AxGridLayoutWrapper::layout() const { return gridLayout; }

void AxGridLayoutWrapper::addWidget(QObject* wrapper, const int row, const int col, const int rowSpan, const int colSpan) const
{
    if (auto* formElement = dynamic_cast<AbstractAxVisualElement*>(wrapper))
        gridLayout->addWidget(formElement->widget(), row, col, rowSpan, colSpan);
    else if (auto* spacerElement = qobject_cast<AxSpacerWrapper*>(wrapper))
        gridLayout->addItem(spacerElement->widget(), row, col, rowSpan, colSpan);
}


/// LINE

AxLineWrapper::AxLineWrapper(const QFrame::Shape dir, QObject* parent) : QObject(parent)
{
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

AxTextLineWrapper::AxTextLineWrapper(QLineEdit* edit, QObject* parent) : QObject(parent), lineedit(edit)
{
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

void AxTextLineWrapper::setReadOnly(const bool &readonly) const { lineedit->setReadOnly(readonly); }

/// COMBO

AxComboBoxWrapper::AxComboBoxWrapper(QComboBox* comboBox, QObject* parent) : QObject(parent), comboBox(comboBox)
{
    connect(comboBox, &QComboBox::currentTextChanged,  this, &AxComboBoxWrapper::currentTextChanged);
    connect(comboBox, &QComboBox::currentIndexChanged, this, &AxComboBoxWrapper::currentIndexChanged);
}

QVariant AxComboBoxWrapper::jsonMarshal() const { return comboBox->currentText(); }

void AxComboBoxWrapper::jsonUnmarshal(const QVariant& value)
{
    int index = comboBox->findText(value.toString());
    if (index != -1)
        comboBox->setCurrentIndex(index);
}

QComboBox * AxComboBoxWrapper::widget() const { return comboBox; }

void AxComboBoxWrapper::addItem(const QString& text) const { comboBox->addItem(text); }

void AxComboBoxWrapper::addItems(const QJSValue& array) const
{
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

void AxComboBoxWrapper::setItems(const QJSValue &array) const
{
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

AxDateEditWrapper::AxDateEditWrapper(QDateEdit* edit, const QString &format, QObject* parent) : QObject(parent), dateedit(edit)
{
    edit->setCalendarPopup(true);
    edit->setDisplayFormat(format);
}

QVariant AxDateEditWrapper::jsonMarshal() const { return dateString(); }

void AxDateEditWrapper::jsonUnmarshal(const QVariant& value) { setDateString(value.toString()); }

QDateEdit* AxDateEditWrapper::widget() const { return dateedit; }

QString AxDateEditWrapper::dateString() const { return dateedit->date().toString( dateedit->displayFormat()); }

void AxDateEditWrapper::setDateString(const QString& date) const { dateedit->setDate(QDate::fromString(date, dateedit->displayFormat())); }

/// TIME

AxTimeEditWrapper::AxTimeEditWrapper(QTimeEdit* edit, const QString &format, QObject* parent) : QObject(parent), timeedit(edit)
{
    timeedit->setDisplayFormat(format);
}

QVariant AxTimeEditWrapper::jsonMarshal() const { return timeString(); }

void AxTimeEditWrapper::jsonUnmarshal(const QVariant& value) { setTimeString(value.toString()); }

QTimeEdit * AxTimeEditWrapper::widget() const { return timeedit; }

QString AxTimeEditWrapper::timeString() const { return timeedit->time().toString(timeedit->displayFormat()); }

void AxTimeEditWrapper::setTimeString(const QString& time) const { timeedit->setTime(QTime::fromString(time, timeedit->displayFormat())); }

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

AxCheckBoxWrapper::AxCheckBoxWrapper(QCheckBox* box, QObject* parent) : QObject(parent), check(box)
{
    connect(check, &QCheckBox::checkStateChanged, this, &AxCheckBoxWrapper::stateChanged);
}

QVariant AxCheckBoxWrapper::jsonMarshal() const { return isChecked(); }

void AxCheckBoxWrapper::jsonUnmarshal(const QVariant& value) { setChecked(value.toBool()); }

QCheckBox * AxCheckBoxWrapper::widget() const { return check; }

bool AxCheckBoxWrapper::isChecked() const { return check->isChecked(); }

void AxCheckBoxWrapper::setChecked(const bool checked) const { check->setChecked(checked); }

void AxSelectorFile::setPlaceholder(const QString& text) const { lineEdit->setPlaceholderText(text); }

/// SWITCH

AxSwitchWrapper::AxSwitchWrapper(oclero::qlementine::Switch* sw, QObject* parent) : QObject(parent), sw(sw)
{
    connect(sw, &oclero::qlementine::Switch::toggled, this, &AxSwitchWrapper::toggled);
}

QVariant AxSwitchWrapper::jsonMarshal() const { return isChecked(); }

void AxSwitchWrapper::jsonUnmarshal(const QVariant& value) { setChecked(value.toBool()); }

QWidget* AxSwitchWrapper::widget() const { return sw; }

bool AxSwitchWrapper::isChecked() const { return sw->isChecked(); }

void AxSwitchWrapper::setChecked(const bool checked) const { sw->setChecked(checked); }

void AxSwitchWrapper::setText(const QString& text) const { sw->setText(text); }

QString AxSwitchWrapper::text() const { return sw->text(); }

/// SEGMENTED CONTROL

AxSegmentedControlWrapper::AxSegmentedControlWrapper(oclero::qlementine::SegmentedControl* sc, QObject* parent) : QObject(parent), segControl(sc)
{
    connect(sc, &oclero::qlementine::SegmentedControl::currentIndexChanged, this, &AxSegmentedControlWrapper::currentIndexChanged);
}

QVariant AxSegmentedControlWrapper::jsonMarshal() const { return currentIndex(); }

void AxSegmentedControlWrapper::jsonUnmarshal(const QVariant& value) { setCurrentIndex(value.toInt()); }

QWidget* AxSegmentedControlWrapper::widget() const { return segControl; }

void AxSegmentedControlWrapper::addItem(const QString& text) const { segControl->addItem(text); }

void AxSegmentedControlWrapper::addItems(const QJSValue& array) const
{
    if (!array.isArray())
        return;
    const int length = array.property("length").toInt();
    for (int i = 0; i < length; ++i)
        segControl->addItem(array.property(i).toString());
}

int AxSegmentedControlWrapper::currentIndex() const { return segControl->currentIndex(); }

void AxSegmentedControlWrapper::setCurrentIndex(const int index) const { segControl->setCurrentIndex(index); }

QString AxSegmentedControlWrapper::currentText() const
{
    int idx = segControl->currentIndex();
    return idx >= 0 ? segControl->getItemText(idx) : QString();
}

int AxSegmentedControlWrapper::count() const { return segControl->itemCount(); }

void AxSegmentedControlWrapper::removeItem(const int index) const { segControl->removeItem(index); }

QString AxSegmentedControlWrapper::itemText(const int index) const { return segControl->getItemText(index); }

void AxSegmentedControlWrapper::setItemText(const int index, const QString& text) const { segControl->setItemText(index, text); }

/// LABEL

AxLabelWrapper::AxLabelWrapper(QLabel* label, QObject* parent) : QObject(parent), label(label) {}

QLabel* AxLabelWrapper::widget() const { return label; }

void AxLabelWrapper::setText(const QString& text) const { label->setText(text); }

QString AxLabelWrapper::text() const { return label->text(); }

/// TAB

AxTabWrapper::AxTabWrapper(QTabWidget* tabs, QObject* parent) : QObject(parent), tabs(tabs) {}

QTabWidget* AxTabWrapper::widget() const { return tabs; }

void AxTabWrapper::addTab(QObject* wrapper, const QString &title) const
{
    if (auto* formElement = dynamic_cast<AbstractAxVisualElement*>(wrapper))
        tabs->addTab(formElement->widget(), title);
}

/// TABLE

AxTableWidgetWrapper::AxTableWidgetWrapper(const QJSValue &headers, QTableView* tableView, QJSEngine* jsEngine, QObject* parent) : QObject(parent), table(tableView), engine(jsEngine) {
    model = new QStandardItemModel(this);
    table->setModel(model);

    connect(model, &QStandardItemModel::itemChanged, this, [this](QStandardItem* item) {
        Q_EMIT cellChanged(item->row(), item->column());
    });
    connect(table, &QTableView::clicked, this, [this](const QModelIndex &index) {
        Q_EMIT cellClicked(index.row(), index.column());
    });
    connect(table, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        Q_EMIT cellDoubleClicked(index.row(), index.column());
    });

    table->setHorizontalHeader(new BoldHeaderView(Qt::Horizontal, table));
    table->setAlternatingRowColors( true );
    table->setAutoFillBackground( false );
    table->setShowGrid( false );
    table->setSortingEnabled( true );
    table->setWordWrap( true );
    table->setCornerButtonEnabled( true );
    table->setSelectionBehavior( QAbstractItemView::SelectRows );
    table->setFocusPolicy( Qt::NoFocus );
    table->horizontalHeader()->setSectionResizeMode( QHeaderView::Stretch );
    table->horizontalHeader()->setCascadingSectionResizes( true );
    table->horizontalHeader()->setHighlightSections( false );
    table->verticalHeader()->setVisible( false );
    table->setItemDelegate(new PaddingDelegate(table));

    this->setColumns(headers);
}

QTableView* AxTableWidgetWrapper::widget() const { return table; }

QVariant AxTableWidgetWrapper::jsonMarshal() const
{
    QJsonArray rowsArray;
    for (int row = 0; row < model->rowCount(); ++row) {
        QJsonArray rowArray;
        for (int col = 0; col < model->columnCount(); ++col) {
            auto item = model->item(row, col);
            rowArray.append(item ? item->text() : QString());
        }
        rowsArray.append(rowArray);
    }
    return rowsArray;
}

void AxTableWidgetWrapper::jsonUnmarshal(const QVariant& value)
{
    QJsonArray rowsArray = QJsonDocument::fromJson(value.toByteArray()).array();
    model->setRowCount(rowsArray.size());

    for (int row = 0; row < rowsArray.size(); ++row) {
        QJsonArray rowArray = rowsArray[row].toArray();
        if (model->columnCount() < rowArray.size())
            model->setColumnCount(rowArray.size());

        for (int col = 0; col < rowArray.size(); ++col) {
            QStandardItem* item = model->item(row, col);
            if (!item) {
                item = new QStandardItem();
                model->setItem(row, col, item);
            }
            item->setText(rowArray[col].toString());
        }
    }
}

void AxTableWidgetWrapper::addColumn(const QString &header) const
{
    int column = model->columnCount()+1;
    model->setColumnCount(column);
    model->setHorizontalHeaderItem(column-1, new QStandardItem(header));
}

void AxTableWidgetWrapper::setColumns(const QJSValue &headers) const
{
    if (!headers.isArray())
        return;

    const int length = headers.property("length").toInt();

    model->setColumnCount(length);

    for (int i = 0; i < length; ++i) {
        QJSValue val = headers.property(i);
        model->setHorizontalHeaderItem(i, new QStandardItem(val.toString()));
    }
}

void AxTableWidgetWrapper::addItem(const QJSValue &items) const
{
    if (!items.isArray())
        return;

    if( model->rowCount() < 1 )
        model->setRowCount( 1 );
    else
        model->setRowCount( model->rowCount() + 1 );


    bool isSortingEnabled = table->isSortingEnabled();
    table->setSortingEnabled( false );

    const int length = items.property("length").toInt();
    for (int i = 0; i < model->columnCount(); i++ ) {
        QString text = "";
        if (i < length)
            text = items.property(i).toString();
        model->setItem( model->rowCount() - 1, i, new QStandardItem(text) );
    }
    table->setSortingEnabled( isSortingEnabled );
}

int AxTableWidgetWrapper::rowCount() const { return model->rowCount(); }

int AxTableWidgetWrapper::columnCount() const { return model->columnCount(); }

void AxTableWidgetWrapper::setRowCount(const int rows) { model->setRowCount(rows); }

void AxTableWidgetWrapper::setColumnCount(const int cols) { model->setColumnCount(cols); }

int AxTableWidgetWrapper::currentRow() const { return table->currentIndex().row(); }

int AxTableWidgetWrapper::currentColumn() const { return table->currentIndex().column(); }

void AxTableWidgetWrapper::setSortingEnabled(const bool enable) { table->setSortingEnabled(enable); }

void AxTableWidgetWrapper::resizeToContent(const int column) { table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents); }

QString AxTableWidgetWrapper::text(const int row, const int column) const {
    auto* item = model->item(row, column);
    return item ? item->text() : QString();
}

void AxTableWidgetWrapper::setText(const int row, const int column, const QString &text) const {
    auto* item = model->item(row, column);
    if (!item) {
        model->setItem(row, column, new QStandardItem(text));
    } else {
        item->setText(text);
    }
}

void AxTableWidgetWrapper::setReadOnly(const bool read)
{
    for(int rowIndex = 0; rowIndex < model->rowCount(); rowIndex++) {
        for(int columnIndex = 0; columnIndex < model->columnCount(); columnIndex++) {
            auto item = model->item(rowIndex, columnIndex);
            if (item) {
                if (read)
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                else
                    item->setFlags(item->flags() | Qt::ItemIsEditable);
            }
        }
    }
}

void AxTableWidgetWrapper::hideColumn(const int column) { table->hideColumn(column); }

void AxTableWidgetWrapper::setHeadersVisible(const bool enable) { table->horizontalHeader()->setVisible(enable); }

void AxTableWidgetWrapper::setColumnAlign(const int column, const QString &align)
{
    int iAlign= Qt::AlignLeft | Qt::AlignVCenter;
    if (align == "center")
        iAlign = Qt::AlignCenter;
    else if (align == "right")
        iAlign = Qt::AlignRight | Qt::AlignVCenter;

    for(int rowIndex = 0; rowIndex < model->rowCount(); rowIndex++) {
        if (auto* item = model->item(rowIndex, column))
            item->setTextAlignment(static_cast<Qt::AlignmentFlag>(iAlign));
    }
}

void AxTableWidgetWrapper::clear()
{
    QSignalBlocker blocker(table->selectionModel());
    model->removeRows(0, model->rowCount());
}

QJSValue AxTableWidgetWrapper::selectedRows()
{
    QSet<int> rowSet;
    for( int rowIndex = 0 ; rowIndex < model->rowCount() ; rowIndex++ ) {
        if ( table->selectionModel()->isSelected(model->index(rowIndex, 0)) )
            rowSet.insert(rowIndex);
    }

    QJSValue jsArray = engine->newArray(rowSet.size());
    int i = 0;
    for (int row : rowSet) {
        jsArray.setProperty(i++, row);
    }
    return jsArray;
}

/// LIST

class CompactListDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QLineEdit* editor = new QLineEdit(parent);
        editor->setContentsMargins(0, 0, 0, 0);
        return editor;
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        return QStyledItemDelegate::sizeHint(option, index);
    }
};

AxListWidgetWrapper::AxListWidgetWrapper(QWidget* container, QListWidget* widget, QPushButton* btnAdd, QPushButton* btnRemove, QJSEngine* engine, QObject* parent)
    : QObject(parent), container(container), list(widget), btnAdd(btnAdd), btnRemove(btnRemove), engine(engine)
{
    list->setObjectName("AxCompactList");
    list->setAlternatingRowColors(true);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    list->setEditTriggers(QAbstractItemView::DoubleClicked);

    list->setItemDelegate(new CompactListDelegate(list));
    list->setSpacing(0);

    list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(list, &QWidget::customContextMenuRequested, this, &AxListWidgetWrapper::showContextMenu);

    connect(list, &QListWidget::currentTextChanged, this, &AxListWidgetWrapper::currentTextChanged);
    connect(list, &QListWidget::currentRowChanged,  this, &AxListWidgetWrapper::currentRowChanged);
    connect(list, &QListWidget::itemClicked,        this, [this](const QListWidgetItem* item) { if (item) Q_EMIT itemClickedText(item->text()); });
    connect(list, &QListWidget::itemDoubleClicked,  this, [this](const QListWidgetItem* item) { if (item) Q_EMIT itemDoubleClickedText(item->text()); });

    btnAdd->setVisible(false);
    btnRemove->setVisible(false);
    connect(btnAdd,    &QPushButton::clicked, this, &AxListWidgetWrapper::onAddClicked);
    connect(btnRemove, &QPushButton::clicked, this, &AxListWidgetWrapper::onRemoveClicked);
}

QVariant AxListWidgetWrapper::jsonMarshal() const
{
    QVariantList listData;
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem* item = list->item(i);
        if (item && item->text() != "")
            listData << item->text();
    }
    return listData;
}

void AxListWidgetWrapper::jsonUnmarshal(const QVariant& value)
{
    list->clear();
    const QVariantList items = value.toList();
    for (const QVariant& v : items) {
        list->addItem(v.toString());
    }
}

QWidget* AxListWidgetWrapper::widget() const { return container; }

QJSValue AxListWidgetWrapper::items()
{
    QJSValue jsArray = engine->newArray(list->count());
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem* item = list->item(i);
        if (item)
            jsArray.setProperty(i, item->text());
    }
    return jsArray;
}

void AxListWidgetWrapper::addItem(const QString& text)
{
    QListWidgetItem* item = new QListWidgetItem(text);
    if (readonly)
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    else
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    list->addItem(item);
}

void AxListWidgetWrapper::addItems(const QJSValue &items)
{
    if (!items.isArray())
        return;

    const int length = items.property("length").toInt();
    for (int i = 0; i < length; i++ ) {
        QString text = items.property(i).toString();

        QListWidgetItem* item = new QListWidgetItem(text);
        if (readonly)
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        else
            item->setFlags(item->flags() | Qt::ItemIsEditable);
        list->addItem(item);
    }
}

void AxListWidgetWrapper::removeItem(const int index) { delete list->takeItem(index); }

QString AxListWidgetWrapper::itemText(const int index) const
{
    QListWidgetItem* item = list->item(index);
    return item ? item->text() : QString();
}

void AxListWidgetWrapper::setItemText(const int index, const QString& text)
{
    QListWidgetItem* item = list->item(index);
    if (item)
        item->setText(text);
}

void AxListWidgetWrapper::clear() { list->clear(); }

int AxListWidgetWrapper::count() const { return list->count(); }

int AxListWidgetWrapper::currentRow() const { return list->currentRow(); }

void AxListWidgetWrapper::setCurrentRow(const int row) { list->setCurrentRow(row); }

QJSValue AxListWidgetWrapper::selectedRows() const
{
    QList<QListWidgetItem*> items = list->selectedItems();
    QJSValue array = engine->newArray(items.size());
    for (int i = 0; i < items.size(); ++i) {
        array.setProperty(i, list->row(items[i]));
    }
    return array;
}

void AxListWidgetWrapper::setReadOnly(const bool readonly)
{
    this->readonly = readonly;
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem* item = list->item(i);
        if (this->readonly)
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        else
            item->setFlags(item->flags() | Qt::ItemIsEditable);
    }
}

void AxListWidgetWrapper::setDragDropEnabled(const bool enabled)
{
    if (enabled) {
        list->setDragDropMode(QAbstractItemView::InternalMove);
        list->setDefaultDropAction(Qt::MoveAction);
    } else {
        list->setDragDropMode(QAbstractItemView::NoDragDrop);
    }
}

void AxListWidgetWrapper::setMenuEnabled(const bool enabled)
{
    this->menuEnabled = enabled;
    disconnect(list, &QWidget::customContextMenuRequested, this, &AxListWidgetWrapper::showContextMenu);
    if (enabled) {
        list->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(list, &QWidget::customContextMenuRequested, this, &AxListWidgetWrapper::showContextMenu);
    } else {
        list->setContextMenuPolicy(Qt::DefaultContextMenu);
    }
}

void AxListWidgetWrapper::showContextMenu(const QPoint &pos)
{
    oclero::qlementine::Menu menu(list);

    QAction* addAction = menu.addAction("Add");
    QAction* removeAction = menu.addAction("Remove");

    QAction* selected = menu.exec(list->viewport()->mapToGlobal(pos));
    if (selected == addAction) {
        onAddClicked();
    } else if (selected == removeAction) {
        onRemoveClicked();
    }
}

void AxListWidgetWrapper::setButtonsEnabled(const bool enabled)
{
    btnAdd->setVisible(enabled);
    btnRemove->setVisible(enabled);
}

void AxListWidgetWrapper::onAddClicked()
{
    addItem("");
    list->setCurrentRow(list->count() - 1);
    list->editItem(list->item(list->count() - 1));
}

void AxListWidgetWrapper::onRemoveClicked()
{
    QList<QListWidgetItem*> selectedItems = list->selectedItems();
    for (QListWidgetItem* item : selectedItems)
        delete item;
}

/// BUTTON

AxButtonWrapper::AxButtonWrapper(QPushButton* btn, QObject* parent) : QObject(parent), button(btn) {
    connect(button, &QPushButton::clicked, this, &AxButtonWrapper::clicked);
}

QPushButton* AxButtonWrapper::widget() const { return button; }

/// GROUPBOX

AxGroupBoxWrapper::AxGroupBoxWrapper(const bool checkable, QGroupBox* box, QObject *parent) : QObject(parent), groupBox(box)
{
    groupBox->setCheckable(checkable);
    groupBox->setLayout(new QHBoxLayout());
    connect(groupBox, &QGroupBox::clicked, this, &AxGroupBoxWrapper::clicked);
}

QVariant AxGroupBoxWrapper::jsonMarshal() const { return groupBox->isChecked(); }

void AxGroupBoxWrapper::jsonUnmarshal(const QVariant &value) { groupBox->setChecked(value.toBool()); }

QGroupBox* AxGroupBoxWrapper::widget() const { return groupBox; }

void AxGroupBoxWrapper::setTitle(const QString &title) { groupBox->setTitle(title); }

bool AxGroupBoxWrapper::isCheckable() const { return groupBox->isCheckable(); }

void AxGroupBoxWrapper::setCheckable(const bool checkable) { groupBox->setCheckable(checkable); }

bool AxGroupBoxWrapper::isChecked() const { return groupBox->isChecked(); }

void AxGroupBoxWrapper::setChecked(const bool checked) { groupBox->setChecked(checked); }

void AxGroupBoxWrapper::setPanel(QObject* panel) const
{
    if (auto* widget = dynamic_cast<AbstractAxVisualElement*>(panel)) {
        delete groupBox->layout();
        QHBoxLayout* layout = new QHBoxLayout();
        layout->setContentsMargins(1,1,1,1);
        layout->addWidget(widget->widget());
        groupBox->setLayout(layout);
    }
}

/// SCROLLAREA

AxScrollAreaWrapper::AxScrollAreaWrapper(QScrollArea* area, QObject* parent) : QObject(parent), scrollArea(area) { scrollArea->setWidgetResizable(true); }

QScrollArea* AxScrollAreaWrapper::widget() const { return scrollArea; }

void AxScrollAreaWrapper::setPanel(QObject* panel) const
{
    if (auto* widget = dynamic_cast<AbstractAxVisualElement*>(panel))
        scrollArea->setWidget(widget->widget());
}

void AxScrollAreaWrapper::setWidgetResizable(const bool resizable) { scrollArea->setWidgetResizable(resizable); }


/// SPLITTER

AxSplitterWrapper::AxSplitterWrapper(QSplitter* splitter, QObject *parent) : QObject(parent), splitter(splitter)
{
    splitter->setHandleWidth(3);
    connect(splitter, &QSplitter::splitterMoved, this, &AxSplitterWrapper::splitterMoved);
}

QSplitter* AxSplitterWrapper::widget() const { return splitter; }

void AxSplitterWrapper::addPage(QObject *w)
{
    if (auto* widget = dynamic_cast<AbstractAxVisualElement*>(w))
        return splitter->addWidget(widget->widget());
}

void AxSplitterWrapper::setSizes(const QVariantList &sizes)
{
    QList<int> list;
    for (const QVariant& v : sizes)
        list << v.toInt();
    splitter->setSizes(list);
}

/// STACK

AxStackedWidgetWrapper::AxStackedWidgetWrapper(QStackedWidget* widget, QObject *parent): QObject(parent), stack(widget) {
    connect(stack, &QStackedWidget::currentChanged, this, &AxStackedWidgetWrapper::currentChanged);
}

QStackedWidget* AxStackedWidgetWrapper::widget() const { return stack; }

int AxStackedWidgetWrapper::addPage(QObject* page)
{
    if (auto* widget = dynamic_cast<AbstractAxVisualElement*>(page))
        return stack->addWidget(widget->widget());

    return -1;
}

int AxStackedWidgetWrapper::insertPage(const int index, QObject *page)
{
    if (auto* widget = dynamic_cast<AbstractAxVisualElement*>(page))
        return stack->insertWidget(index, widget->widget());

    return -1;
}

void AxStackedWidgetWrapper::removePage(const int index)
{
    if (auto* page = stack->widget(index))
        stack->removeWidget(page);
}

void AxStackedWidgetWrapper::setCurrentIndex(const int index) { stack->setCurrentIndex(index); }

int AxStackedWidgetWrapper::currentIndex() const { return stack->currentIndex(); }

int AxStackedWidgetWrapper::count() const { return stack->count(); }

/// PANEL

AxPanelWrapper::AxPanelWrapper(QWidget* w, QObject* parent) : QObject(parent), panel(w) {}

QWidget* AxPanelWrapper::widget() const { return panel; }

void AxPanelWrapper::setLayout(QObject* layoutWrapper) const
{
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
    if (widgets.contains(id)) {
        widgets[id]->deleteLater(); /// ToDo: ???
        widgets.remove(id);
    }
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

AxDialogWrapper::AxDialogWrapper(const QString& title, QWidget* parent) : QObject(parent)
{
    dialog = new QDialog(parent);
    dialog->setWindowTitle(title);
    dialog->setProperty("Main", "base");
    layout = new QVBoxLayout(dialog);

    buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    layout->addWidget(buttons);
    dialog->setLayout(layout);

    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
}

void AxDialogWrapper::setLayout(QObject* layoutWrapper)
{
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

void AxDialogWrapper::setButtonsText(const QString &ok_text, const QString &cancel_text) const
{
    QPushButton *okButton = buttons->button(QDialogButtonBox::Ok);
    if (okButton) {
        okButton->setText(ok_text);
    }
    QPushButton *cancelButton = buttons->button(QDialogButtonBox::Cancel);
    if (cancelButton) {
        cancelButton->setText(cancel_text);
    }
}



#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>

AxExtDialogWrapper::AxExtDialogWrapper(AdaptixWidget* w, const QString& title) : QObject(nullptr)
{
    adaptixWidget = w;
    QString project = w->GetProfile()->GetProject();

    dialogId = title + "-" + project;
    dialogTitle = title;

    dialog = new QDialog();
    dialog->setWindowTitle(title);
    dialog->setProperty("Main", "base");
    layout = new QVBoxLayout(dialog);

    buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    layout->addWidget(buttons);
    dialog->setLayout(layout);

    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

    if (adaptixWidget) {
        adaptixWidget->AddExtDock(dialogId, title, [this]() {
            show();
        });
    }
}

AxExtDialogWrapper::~AxExtDialogWrapper()
{
    if (adaptixWidget)
        adaptixWidget->RemoveExtDock(dialogId);

    if (dialog) {
        dialog->close();
        dialog->deleteLater();
    }
}

void AxExtDialogWrapper::setLayout(QObject* layoutWrapper)
{
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

void AxExtDialogWrapper::setSize(const int w, const int h) const { dialog->resize(w, h); }

bool AxExtDialogWrapper::exec() const { return dialog->exec() == QDialog::Accepted; }

void AxExtDialogWrapper::show() const { dialog->show(); }

void AxExtDialogWrapper::close() const { dialog->close(); }

void AxExtDialogWrapper::setButtonsText(const QString &ok_text, const QString &cancel_text) const
{
    QPushButton *okButton = buttons->button(QDialogButtonBox::Ok);
    if (okButton) {
        okButton->setText(ok_text);
    }
    QPushButton *cancelButton = buttons->button(QDialogButtonBox::Cancel);
    if (cancelButton) {
        cancelButton->setText(cancel_text);
    }
}

/// FILE SELECTOR

AxSelectorFile::AxSelectorFile(QLineEdit* edit, QObject* parent) : QObject(parent), lineEdit(edit)
{
    lineEdit->setReadOnly(true);

    auto action = lineEdit->addAction(QIcon(":/icons/folder"), QLineEdit::TrailingPosition);
    connect(action, &QAction::triggered, this, &AxSelectorFile::onSelectFile);
}

QLineEdit* AxSelectorFile::widget() const { return lineEdit; }

QVariant AxSelectorFile::jsonMarshal() const { return content; }

void AxSelectorFile::jsonUnmarshal(const QVariant& value)
{
    content = value.toString();
    lineEdit->setText("Selected...");
}

void AxSelectorFile::onSelectFile()
{
    if (!lineEdit)
        return;

    NonBlockingDialogs::getOpenFileName(lineEdit, "Select a file", "", "All Files (*.*)",
        [this](const QString& selectedFile) {
            if (!lineEdit || selectedFile.isEmpty())
                return;

            lineEdit->setText(selectedFile);

            QFile file(selectedFile);
            if (!file.open(QIODevice::ReadOnly))
                return;

            QByteArray fileData = file.readAll();
            file.close();

            content = QString::fromUtf8(fileData.toBase64());
        });
}

/// SELECTOR CREDENTIALS

AxDialogCreds::AxDialogCreds(const QJSValue &headers, AuthProfile* profile, QWidget *parent) : QDialog(parent)
{
    this->setProperty("Main", "base");

    tableView = new QTableView(this);
    tableView->setAlternatingRowColors(true);
    tableView->setShowGrid(false);
    tableView->setSortingEnabled(false);
    tableView->setWordWrap(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setFocusPolicy(Qt::NoFocus);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->horizontalHeader()->setCascadingSectionResizes(true);
    tableView->horizontalHeader()->setHighlightSections(false);
    tableView->horizontalHeader()->setSortIndicatorShown(true);
    tableView->horizontalHeader()->setSectionsClickable(true);
    tableView->verticalHeader()->setVisible(false);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    tableModel = new AxCredsTableModel(this);
    tableView->setModel(tableModel);
    tableView->setItemDelegate(new PaddingDelegate(tableView));

    pageNavBar = new PageNavBar(this);
    pageNavBar->setFilterPlaceholder("filter: (adm | user) & aes256");
    pageNavBar->setAgentComboVisible(false);
    pageNavBar->setAutoVisible(true);

    chooseButton = new QPushButton("Choose", this);

    spacer_1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Maximum);
    spacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Maximum);

    bottomLayout = new QHBoxLayout();
    bottomLayout->addItem(spacer_1);
    bottomLayout->addWidget(chooseButton);
    bottomLayout->addItem(spacer_2);

    mainLayout = new QVBoxLayout();
    mainLayout->addWidget(pageNavBar);
    mainLayout->addWidget(tableView);
    mainLayout->addLayout(bottomLayout);

    setLayout(mainLayout);

    connect(chooseButton, &QPushButton::clicked, this, &AxDialogCreds::onClicked);
    connect(tableView, &QTableView::doubleClicked, this, &AxDialogCreds::onClicked);
    connect(this, &QDialog::finished, this, [this](int) {
        if (pageHelper)
            pageHelper->cancel();
    });

    QVector<QString> headerLabels;
    QVector<QString> fieldKeys;
    fillSelectorHeaders( headers, FIELD_MAP_CREDS, {"username", "password", "realm", "type", "tag", "storage", "host"}, "CredId", "id", headerLabels, fieldKeys);

    tableModel->setHeaders(headerLabels, fieldKeys);
    applySelectorTableColumns(tableView, headerLabels);

    for (int i = 0; i < fieldKeys.size(); ++i) {
        if (fieldKeys[i] == QLatin1String("date")) {
            tableView->horizontalHeader()->setSortIndicator(i, Qt::DescendingOrder);
            break;
        }
    }

    if (profile) {
        pageHelper = new PagedTableHelper(profile, QStringLiteral("/creds/list"), this);
        configureSelectorPagination(pageNavBar, pageHelper);

        connect(pageHelper, &PagedTableHelper::pageReady,      this, &AxDialogCreds::onPageReady);
        connect(pageHelper, &PagedTableHelper::errorOccurred,  this, &AxDialogCreds::onPageError);
        connect(pageHelper, &PagedTableHelper::loadingChanged, this, [this](bool loading) {
            pageNavBar->setLoading(loading);
            tableView->setEnabled(!loading);
            chooseButton->setEnabled(!loading);
        });

        connect(pageNavBar, &PageNavBar::prevClicked, this, [this]() {
            m_offset = qMax(0, m_offset - pageNavBar->pageSize());
            loadCurrentPage();
        });
        connect(pageNavBar, &PageNavBar::nextClicked, this, [this]() {
            m_offset += pageNavBar->pageSize();
            loadCurrentPage();
        });
        connect(pageNavBar, &PageNavBar::pageSizeChanged, this, [this](int size) {
            pageHelper->setPageSize(size);
            m_offset = 0;
            loadCurrentPage();
        });
        connect(pageNavBar, &PageNavBar::filterChanged, this, [this]() {
            m_offset = 0;
            loadCurrentPage();
        });

        connect(tableView->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int section, Qt::SortOrder order) {
            const QString key = credFieldToSortKey(tableModel->fieldKey(section));
            if (key.isEmpty())
                return;
            const QString newOrder = (order == Qt::AscendingOrder) ? QStringLiteral("asc") : QStringLiteral("desc");
            if (key == m_sortCol && newOrder == m_sortOrder)
                return;
            m_sortCol = key;
            m_sortOrder = newOrder;
            m_offset = 0;
            loadCurrentPage();
        });
    } else {
        pageNavBar->setError(QStringLiteral("No auth profile"));
        pageNavBar->setPrevEnabled(false);
        pageNavBar->setNextEnabled(false);
    }
}

AxDialogCreds::~AxDialogCreds()
{
    if (pageHelper)
        pageHelper->cancel();
}

void AxDialogCreds::prepare()
{
    selectedData.clear();
    m_offset = 0;
    if (pageHelper)
        loadCurrentPage();
}

void AxDialogCreds::loadCurrentPage()
{
    if (!pageHelper)
        return;
    pageHelper->setPageSize(pageNavBar->pageSize());
    pageHelper->setParam(QStringLiteral("q"), pageNavBar->filterText());
    pageHelper->setParam(QStringLiteral("sort"), m_sortCol);
    pageHelper->setParam(QStringLiteral("order"), m_sortOrder);
    pageHelper->loadPage(m_offset);
}

void AxDialogCreds::onPageReady(const QJsonObject& response)
{
    QJsonArray items = response.value(QStringLiteral("items")).toArray();

    QVector<CredentialData> page;
    page.reserve(items.size());

    for (const QJsonValue& v : items) {
        QJsonObject obj = v.toObject();
        CredentialData c;
        c.CredId        = parseI64(obj, QStringLiteral("c_creds_id"));
        c.Username      = obj[QStringLiteral("c_username")].toString();
        c.Password      = obj[QStringLiteral("c_password")].toString();
        c.Realm         = obj[QStringLiteral("c_realm")].toString();
        c.Type          = obj[QStringLiteral("c_type")].toString();
        c.Tag           = obj[QStringLiteral("c_tag")].toString();
        c.DateTimestamp = parseI64(obj, QStringLiteral("c_date"));
        c.Date          = UnixTimestampGlobalToStringLocal(c.DateTimestamp);
        c.Storage       = obj[QStringLiteral("c_storage")].toString();
        c.AgentId       = parseI64(obj, QStringLiteral("c_agent_id"));
        c.Host          = obj[QStringLiteral("c_host")].toString();
        page.append(c);
    }

    tableModel->setData(page);

    const int total = response[QStringLiteral("total")].toInt();
    const int shown = page.size();
    const int from  = shown == 0 ? 0 : m_offset + 1;
    const int to    = m_offset + shown;
    pageNavBar->setInfo(from, to, total);
    pageNavBar->setPrevEnabled(m_offset > 0);
    pageNavBar->setNextEnabled(m_offset + shown < total);
}

void AxDialogCreds::onPageError(const QString& message)
{
    if (message.contains(QStringLiteral("invalid filter"), Qt::CaseInsensitive) || message.startsWith(QStringLiteral("filter:"), Qt::CaseInsensitive))
        return;

    tableModel->setData({});
    pageNavBar->setError(message);
    pageNavBar->setPrevEnabled(false);
    pageNavBar->setNextEnabled(false);
}

QVector<CredentialData> AxDialogCreds::data() { return selectedData; }

void AxDialogCreds::onClicked()
{
    selectedData.clear();
    const QModelIndexList selected = tableView->selectionModel()->selectedRows();
    for (const auto& index : selected) {
        if (index.isValid())
            selectedData.append(tableModel->getCredential(index.row()));
    }
    this->accept();
}

AxSelectorCreds::AxSelectorCreds(const QJSValue &headers, AxScriptEngine* jsEngine, QObject* parent) : QObject(parent), scriptEngine(jsEngine)
{
    dialog = new AxDialogCreds(headers, profileFromEngine(jsEngine), nullptr);
    prepareSelectorDialog(dialog, "Choose credentials", jsEngine);
}

void AxSelectorCreds::setSize(const int w, const int h ) const
{
    if (dialog)
        dialog->resize(w, h);
}

QJSValue AxSelectorCreds::exec() const
{
    if (!dialog || !scriptEngine || !scriptEngine->engine())
        return emptyJsArray(scriptEngine);

    prepareSelectorDialog(dialog, "Choose credentials", scriptEngine);
    dialog->prepare();

    QVector<CredentialData> vecCreds;
    if (execSelectorDialog(dialog, scriptEngine) == QDialog::Accepted) {
        vecCreds = dialog->data();
    }

    QVariantList list;
    for (const auto& cred : vecCreds) {
        QVariantMap map;
        map["id"]       = cred.CredId;
        map["username"] = cred.Username;
        map["password"] = cred.Password;
        map["realm"]    = cred.Realm;
        map["type"]     = cred.Type;
        map["tag"]      = cred.Tag;
        map["date"]     = cred.Date;
        map["storage"]  = cred.Storage;
        map["agent_id"] = cred.AgentId;
        map["host"]     = cred.Host;
        list.append(map);
    }
    return scriptEngine->engine()->toScriptValue(list);
}

void AxSelectorCreds::close() const
{
    if (dialog)
        dialog->close();
}

/// SELECTOR AGENTS

AxDialogAgents::AxDialogAgents(const QJSValue &headers, const QVector<AgentData> &vecAgents, QWidget *parent)
    : QDialog(parent)
{
    this->setProperty("Main", "base");

    tableView = new QTableView(this);
    tableView->setAlternatingRowColors(true);
    tableView->setShowGrid(false);
    tableView->setSortingEnabled(true);
    tableView->setWordWrap(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setFocusPolicy(Qt::NoFocus);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->horizontalHeader()->setCascadingSectionResizes(true);
    tableView->horizontalHeader()->setHighlightSections(false);
    tableView->verticalHeader()->setVisible(false);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    tableModel = new AxAgentsTableModel(this);
    proxyModel = new AxAgentsFilterProxyModel(this);
    proxyModel->setSourceModel(tableModel);
    tableView->setModel(proxyModel);
    tableView->setItemDelegate(new PaddingDelegate(tableView));

    chooseButton = new QPushButton("Choose", this);

    spacer_1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Maximum);
    spacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Maximum);

    bottomLayout = new QHBoxLayout();
    bottomLayout->addItem(spacer_1);
    bottomLayout->addWidget(chooseButton);
    bottomLayout->addItem(spacer_2);

    searchWidget = new QWidget(this);

    searchLineEdit = new QLineEdit(searchWidget);
    searchLineEdit->setPlaceholderText("filter");

    hideButton = new ClickableLabel("X");
    hideButton->setCursor(Qt::PointingHandCursor);

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(searchLineEdit);
    searchLayout->addWidget(hideButton);

    mainLayout = new QVBoxLayout();
    mainLayout->addWidget(searchWidget);
    mainLayout->addWidget(tableView);
    mainLayout->addLayout(bottomLayout);

    setLayout(mainLayout);

    connect(searchLineEdit, &QLineEdit::textEdited,   this, &AxDialogAgents::handleSearch);
    connect(chooseButton,   &QPushButton::clicked,    this, &AxDialogAgents::onClicked);
    connect(hideButton,     &ClickableLabel::clicked, this, &AxDialogAgents::clearSearch);

    QVector<QString> headerLabels;
    QVector<QString> fieldKeys;
    fillSelectorHeaders( headers, FIELD_MAP_AGENTS, {"id", "type", "computer", "username", "process", "pid", "os", "tags"}, "Agent ID", "id", headerLabels, fieldKeys);

    tableModel->setHeaders(headerLabels, fieldKeys);
    tableModel->setData(vecAgents);
    applySelectorTableColumns(tableView, headerLabels);
}

QVector<AgentData> AxDialogAgents::data() { return selectedData; }

void AxDialogAgents::onClicked()
{
    selectedData.clear();
    QModelIndexList selected = tableView->selectionModel()->selectedRows();
    for (const auto& proxyIndex : selected) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (sourceIndex.isValid()) {
            selectedData.append(tableModel->getAgent(sourceIndex.row()));
        }
    }
    this->accept();
}

void AxDialogAgents::handleSearch()
{
    proxyModel->setFilterText(searchLineEdit->text());
}

void AxDialogAgents::clearSearch()
{
    searchLineEdit->clear();
    handleSearch();
}

AxSelectorAgents::AxSelectorAgents(const QJSValue &headers, AxScriptEngine* jsEngine, QObject* parent) : QObject(parent), scriptEngine(jsEngine)
{
    QVector<AgentData> vecAgents;
    if (scriptEngine && scriptEngine->manager()) {
        const auto agents = scriptEngine->manager()->GetAgents().values();
        for (auto* agent : agents) {
            if (agent)
                vecAgents.append(agent->data);
        }
    }

    dialog = new AxDialogAgents(headers, vecAgents, nullptr);
    prepareSelectorDialog(dialog, "Choose agent", jsEngine);
}

void AxSelectorAgents::setSize(const int w, const int h ) const
{
    if (dialog)
        dialog->resize(w, h);
}

QJSValue AxSelectorAgents::exec() const
{
    if (!dialog || !scriptEngine || !scriptEngine->engine())
        return emptyJsArray(scriptEngine);

    prepareSelectorDialog(dialog, "Choose agent", scriptEngine);

    QVector<AgentData> vecAgents;
    if (execSelectorDialog(dialog, scriptEngine) == QDialog::Accepted) {
        vecAgents = dialog->data();
    }

    QVariantList list;
    for (auto agentData : vecAgents) {
        QString username = agentData.Username;
        if ( agentData.Elevated )
            username = "* " + username;
        if ( !agentData.Impersonated.isEmpty() )
            username += " [" + agentData.Impersonated + "]";

        QString process  = QString("%1 (%2)").arg(agentData.Process).arg(agentData.Arch);

        QString os = "unknown";
        if (agentData.Os == OS_WINDOWS)    os = "windows";
        else if (agentData.Os == OS_LINUX) os = "linux";
        else if (agentData.Os == OS_MAC)   os = "macos";

        QVariantMap map;
        map["id"]          = agentData.Id;
        map["type"]        = agentData.Name;
        map["listener"]    = agentData.Listener;
        map["external_ip"] = agentData.ExternalIP;
        map["internal_ip"] = agentData.InternalIP;
        map["domain"]      = agentData.Domain;
        map["computer"]    = agentData.Computer;
        map["username"]    = username;
        map["process"]     = process;
        map["pid"]         = agentData.Pid;
        map["tid"]         = agentData.Tid;
        map["os"]          = os;
        map["tags"]        = agentData.Tags;
        list.append(map);
    }
    return scriptEngine->engine()->toScriptValue(list);
}

void AxSelectorAgents::close() const
{
    if (dialog)
        dialog->close();
}



/// SELECTOR LISTENERS

AxDialogListeners::AxDialogListeners(const QJSValue &headers, const QVector<ListenerData> &vecListeners, QWidget *parent) : QDialog(parent)
{
    this->setProperty("Main", "base");

    tableView = new QTableView(this);
    tableView->setAlternatingRowColors(true);
    tableView->setShowGrid(false);
    tableView->setSortingEnabled(true);
    tableView->setWordWrap(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setFocusPolicy(Qt::NoFocus);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->horizontalHeader()->setCascadingSectionResizes(true);
    tableView->horizontalHeader()->setHighlightSections(false);
    tableView->verticalHeader()->setVisible(false);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    tableModel = new AxListenersTableModel(this);
    proxyModel = new AxListenersFilterProxyModel(this);
    proxyModel->setSourceModel(tableModel);
    tableView->setModel(proxyModel);
    tableView->setItemDelegate(new PaddingDelegate(tableView));

    chooseButton = new QPushButton("Choose", this);

    spacer_1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Maximum);
    spacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Maximum);

    bottomLayout = new QHBoxLayout();
    bottomLayout->addItem(spacer_1);
    bottomLayout->addWidget(chooseButton);
    bottomLayout->addItem(spacer_2);

    searchWidget = new QWidget(this);

    searchLineEdit = new QLineEdit(searchWidget);
    searchLineEdit->setPlaceholderText("filter");

    hideButton = new ClickableLabel("X");
    hideButton->setCursor(Qt::PointingHandCursor);

    searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(4);
    searchLayout->addWidget(searchLineEdit);
    searchLayout->addWidget(hideButton);

    mainLayout = new QVBoxLayout();
    mainLayout->addWidget(searchWidget);
    mainLayout->addWidget(tableView);
    mainLayout->addLayout(bottomLayout);

    setLayout(mainLayout);

    connect(searchLineEdit, &QLineEdit::textEdited,   this, &AxDialogListeners::handleSearch);
    connect(chooseButton,   &QPushButton::clicked,    this, &AxDialogListeners::onClicked);
    connect(hideButton,     &ClickableLabel::clicked, this, &AxDialogListeners::clearSearch);

    QVector<QString> headerLabels;
    QVector<QString> fieldKeys;
    fillSelectorHeaders(headers, FIELD_MAP_LISTENERS, {"name", "type", "protocol", "bind_host", "bind_port", "status"}, "Name", "name", headerLabels, fieldKeys);

    tableModel->setHeaders(headerLabels, fieldKeys);
    tableModel->setData(vecListeners);
    applySelectorTableColumns(tableView, headerLabels);
}

QVector<ListenerData> AxDialogListeners::data() { return selectedData; }

void AxDialogListeners::onClicked()
{
    selectedData.clear();
    QModelIndexList selected = tableView->selectionModel()->selectedRows();
    for (const auto& proxyIndex : selected) {
        QModelIndex sourceIndex = proxyModel->mapToSource(proxyIndex);
        if (sourceIndex.isValid()) {
            selectedData.append(tableModel->getListener(sourceIndex.row()));
        }
    }
    this->accept();
}

void AxDialogListeners::handleSearch()
{
    proxyModel->setFilterText(searchLineEdit->text());
}

void AxDialogListeners::clearSearch()
{
    searchLineEdit->clear();
    handleSearch();
}

AxSelectorListeners::AxSelectorListeners(const QJSValue &headers, AxScriptEngine* jsEngine, QObject* parent) : QObject(parent), scriptEngine(jsEngine)
{
    QVector<ListenerData> vecListeners;
    if (scriptEngine && scriptEngine->manager())
        vecListeners = scriptEngine->manager()->GetListeners();

    dialog = new AxDialogListeners(headers, vecListeners, nullptr);
    prepareSelectorDialog(dialog, "Choose listener", jsEngine);
}

void AxSelectorListeners::setSize(const int w, const int h ) const
{
    if (dialog)
        dialog->resize(w, h);
}

QJSValue AxSelectorListeners::exec() const
{
    if (!dialog || !scriptEngine || !scriptEngine->engine())
        return emptyJsArray(scriptEngine);

    prepareSelectorDialog(dialog, "Choose listener", scriptEngine);

    QVector<ListenerData> vecListeners;
    if (execSelectorDialog(dialog, scriptEngine) == QDialog::Accepted) {
        vecListeners = dialog->data();
    }

    QVariantList list;
    for (auto listener : vecListeners) {
        QVariantMap map;
        map["name"]       = listener.Name;
        map["type"]       = listener.ListenerType;
        map["protocol"]   = listener.ListenerProtocol;
        map["bind_host"]  = listener.BindHost;
        map["bind_port"]  = listener.BindPort;
        map["agent_addr"] = listener.AgentAddresses;
        map["status"]     = listener.Status;
        map["date"]       = listener.Date;
        list.append(map);
    }
    return scriptEngine->engine()->toScriptValue(list);
}

void AxSelectorListeners::close() const
{
    if (dialog)
        dialog->close();
}



/// SELECTOR TARGETS

AxDialogTargets::AxDialogTargets(const QJSValue &headers, AuthProfile* profile, QWidget *parent) : QDialog(parent)
{
    this->setProperty("Main", "base");

    tableView = new QTableView(this);
    tableView->setAlternatingRowColors(true);
    tableView->setShowGrid(false);
    tableView->setSortingEnabled(false);
    tableView->setWordWrap(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setFocusPolicy(Qt::NoFocus);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->horizontalHeader()->setCascadingSectionResizes(true);
    tableView->horizontalHeader()->setHighlightSections(false);
    tableView->horizontalHeader()->setSortIndicatorShown(true);
    tableView->horizontalHeader()->setSectionsClickable(true);
    tableView->verticalHeader()->setVisible(false);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    tableModel = new AxTargetsTableModel(this);
    tableView->setModel(tableModel);
    tableView->setItemDelegate(new PaddingDelegate(tableView));

    pageNavBar = new PageNavBar(this);
    pageNavBar->setFilterPlaceholder("filter: (win | linux) & ^(test)");
    pageNavBar->setAgentComboVisible(false);
    pageNavBar->setAutoVisible(true);

    chooseButton = new QPushButton("Choose", this);

    spacer_1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Maximum);
    spacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Maximum);

    bottomLayout = new QHBoxLayout();
    bottomLayout->addItem(spacer_1);
    bottomLayout->addWidget(chooseButton);
    bottomLayout->addItem(spacer_2);

    mainLayout = new QVBoxLayout();
    mainLayout->addWidget(pageNavBar);
    mainLayout->addWidget(tableView);
    mainLayout->addLayout(bottomLayout);

    setLayout(mainLayout);

    connect(chooseButton, &QPushButton::clicked, this, &AxDialogTargets::onClicked);
    connect(tableView, &QTableView::doubleClicked, this, &AxDialogTargets::onClicked);
    connect(this, &QDialog::finished, this, [this](int) {
        if (pageHelper)
            pageHelper->cancel();
    });

    QVector<QString> headerLabels;
    QVector<QString> fieldKeys;
    fillSelectorHeaders(headers, FIELD_MAP_TARGETS, {"computer", "domain", "address", "os", "tag", "alive"}, "Target ID", "id", headerLabels, fieldKeys);

    tableModel->setHeaders(headerLabels, fieldKeys);
    applySelectorTableColumns(tableView, headerLabels);

    for (int i = 0; i < fieldKeys.size(); ++i) {
        if (fieldKeys[i] == QLatin1String("date")) {
            tableView->horizontalHeader()->setSortIndicator(i, Qt::DescendingOrder);
            break;
        }
    }

    if (profile) {
        pageHelper = new PagedTableHelper(profile, QStringLiteral("/targets/list"), this);
        configureSelectorPagination(pageNavBar, pageHelper);

        connect(pageHelper, &PagedTableHelper::pageReady,      this, &AxDialogTargets::onPageReady);
        connect(pageHelper, &PagedTableHelper::errorOccurred,  this, &AxDialogTargets::onPageError);
        connect(pageHelper, &PagedTableHelper::loadingChanged, this, [this](bool loading) {
            pageNavBar->setLoading(loading);
            tableView->setEnabled(!loading);
            chooseButton->setEnabled(!loading);
        });

        connect(pageNavBar, &PageNavBar::prevClicked, this, [this]() {
            m_offset = qMax(0, m_offset - pageNavBar->pageSize());
            loadCurrentPage();
        });
        connect(pageNavBar, &PageNavBar::nextClicked, this, [this]() {
            m_offset += pageNavBar->pageSize();
            loadCurrentPage();
        });
        connect(pageNavBar, &PageNavBar::pageSizeChanged, this, [this](int size) {
            pageHelper->setPageSize(size);
            m_offset = 0;
            loadCurrentPage();
        });
        connect(pageNavBar, &PageNavBar::filterChanged, this, [this]() {
            m_offset = 0;
            loadCurrentPage();
        });

        connect(tableView->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int section, Qt::SortOrder order) {
            const QString key = targetFieldToSortKey(tableModel->fieldKey(section));
            if (key.isEmpty())
                return;
            const QString newOrder = (order == Qt::AscendingOrder) ? QStringLiteral("asc") : QStringLiteral("desc");
            if (key == m_sortCol && newOrder == m_sortOrder)
                return;
            m_sortCol = key;
            m_sortOrder = newOrder;
            m_offset = 0;
            loadCurrentPage();
        });
    } else {
        pageNavBar->setError(QStringLiteral("No auth profile"));
        pageNavBar->setPrevEnabled(false);
        pageNavBar->setNextEnabled(false);
    }
}

AxDialogTargets::~AxDialogTargets()
{
    if (pageHelper)
        pageHelper->cancel();
}

void AxDialogTargets::prepare()
{
    selectedData.clear();
    m_offset = 0;
    if (pageHelper)
        loadCurrentPage();
}

void AxDialogTargets::loadCurrentPage()
{
    if (!pageHelper)
        return;
    pageHelper->setPageSize(pageNavBar->pageSize());
    pageHelper->setParam(QStringLiteral("q"), pageNavBar->filterText());
    pageHelper->setParam(QStringLiteral("sort"), m_sortCol);
    pageHelper->setParam(QStringLiteral("order"), m_sortOrder);
    pageHelper->loadPage(m_offset);
}

void AxDialogTargets::onPageReady(const QJsonObject& response)
{
    QJsonArray items = response.value(QStringLiteral("items")).toArray();

    QVector<TargetData> page;
    page.reserve(items.size());

    for (const QJsonValue& v : items) {
        QJsonObject obj = v.toObject();
        TargetData t;
        t.TargetId      = parseI64(obj, QStringLiteral("t_target_id"));
        t.Computer      = obj[QStringLiteral("t_computer")].toString();
        t.Domain        = obj[QStringLiteral("t_domain")].toString();
        t.Address       = obj[QStringLiteral("t_address")].toString();
        t.Tag           = obj[QStringLiteral("t_tag")].toString();
        t.Os            = obj[QStringLiteral("t_os")].toInt();
        t.OsDesc        = obj[QStringLiteral("t_os_desk")].toString();
        t.DateTimestamp = parseI64(obj, QStringLiteral("t_date"));
        t.Date          = UnixTimestampGlobalToStringLocal(t.DateTimestamp);
        t.Info          = obj[QStringLiteral("t_info")].toString();
        t.Alive         = obj[QStringLiteral("t_alive")].toBool();
        for (const QJsonValue& av : obj[QStringLiteral("t_agents")].toArray()) {
            if (av.isDouble() || av.isString())
                t.Agents.append(parseI64(av));
        }
        page.append(t);
    }

    tableModel->setData(page);

    const int total = response[QStringLiteral("total")].toInt();
    const int shown = page.size();
    const int from  = shown == 0 ? 0 : m_offset + 1;
    const int to    = m_offset + shown;
    pageNavBar->setInfo(from, to, total);
    pageNavBar->setPrevEnabled(m_offset > 0);
    pageNavBar->setNextEnabled(m_offset + shown < total);
}

void AxDialogTargets::onPageError(const QString& message)
{
    if (message.contains(QStringLiteral("invalid filter"), Qt::CaseInsensitive) || message.startsWith(QStringLiteral("filter:"), Qt::CaseInsensitive))
        return;

    tableModel->setData({});
    pageNavBar->setError(message);
    pageNavBar->setPrevEnabled(false);
    pageNavBar->setNextEnabled(false);
}

QVector<TargetData> AxDialogTargets::data() { return selectedData; }

void AxDialogTargets::onClicked()
{
    selectedData.clear();
    const QModelIndexList selected = tableView->selectionModel()->selectedRows();
    for (const auto& index : selected) {
        if (index.isValid())
            selectedData.append(tableModel->getTarget(index.row()));
    }
    this->accept();
}

AxSelectorTargets::AxSelectorTargets(const QJSValue &headers, AxScriptEngine* jsEngine, QObject* parent) : QObject(parent), scriptEngine(jsEngine)
{
    dialog = new AxDialogTargets(headers, profileFromEngine(jsEngine), nullptr);
    prepareSelectorDialog(dialog, QStringLiteral("Choose target"), jsEngine);
}

void AxSelectorTargets::setSize(const int w, const int h ) const
{
    if (dialog)
        dialog->resize(w, h);
}

QJSValue AxSelectorTargets::exec() const
{
    if (!dialog || !scriptEngine || !scriptEngine->engine())
        return emptyJsArray(scriptEngine);

    prepareSelectorDialog(dialog, QStringLiteral("Choose target"), scriptEngine);
    dialog->prepare();

    QVector<TargetData> vecTargets;
    if (execSelectorDialog(dialog, scriptEngine) == QDialog::Accepted) {
        vecTargets = dialog->data();
    }

    QVariantList list;
    for (auto target : vecTargets) {
        QString os = "unknown";
        if (target.Os == OS_WINDOWS)    os = "windows";
        else if (target.Os == OS_LINUX) os = "linux";
        else if (target.Os == OS_MAC)   os = "macos";

        QVariantMap map;
        map["id"]       = target.TargetId;
        map["computer"] = target.Computer;
        map["domain"]   = target.Domain;
        map["address"]  = target.Address;
        map["tag"]      = target.Tag;
        map["os"]       = os;
        map["os_desc"]  = target.OsDesc;
        map["info"]     = target.Info;
        map["date"]     = target.Date;
        map["alive"]    = target.Alive;
        list.append(map);
    }
    return scriptEngine->engine()->toScriptValue(list);
}

void AxSelectorTargets::close() const
{
    if (dialog)
        dialog->close();
}



/// SELECTOR DOWNLOADS

AxDialogDownloads::AxDialogDownloads(const QJSValue &headers, AuthProfile* profile, QWidget *parent) : QDialog(parent)
{
    this->setProperty("Main", "base");

    tableView = new QTableView(this);
    tableView->setAlternatingRowColors(true);
    tableView->setShowGrid(false);
    tableView->setSortingEnabled(false);
    tableView->setWordWrap(true);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setFocusPolicy(Qt::NoFocus);
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableView->horizontalHeader()->setCascadingSectionResizes(true);
    tableView->horizontalHeader()->setHighlightSections(false);
    tableView->horizontalHeader()->setSortIndicatorShown(true);
    tableView->horizontalHeader()->setSectionsClickable(true);
    tableView->verticalHeader()->setVisible(false);
    tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    tableModel = new AxDownloadsTableModel(this);
    tableView->setModel(tableModel);
    tableView->setItemDelegate(new PaddingDelegate(tableView));

    pageNavBar = new PageNavBar(this);
    pageNavBar->setFilterPlaceholder("filter: report | .zip");
    pageNavBar->setAgentComboVisible(false);
    pageNavBar->setAutoVisible(true);

    chooseButton = new QPushButton("Choose", this);

    spacer_1 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Maximum);
    spacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Maximum);

    bottomLayout = new QHBoxLayout();
    bottomLayout->addItem(spacer_1);
    bottomLayout->addWidget(chooseButton);
    bottomLayout->addItem(spacer_2);

    mainLayout = new QVBoxLayout();
    mainLayout->addWidget(pageNavBar);
    mainLayout->addWidget(tableView);
    mainLayout->addLayout(bottomLayout);

    setLayout(mainLayout);

    connect(chooseButton, &QPushButton::clicked, this, &AxDialogDownloads::onClicked);
    connect(tableView, &QTableView::doubleClicked, this, &AxDialogDownloads::onClicked);
    connect(this, &QDialog::finished, this, [this](int) {
        if (pageHelper)
            pageHelper->cancel();
    });

    QVector<QString> headerLabels;
    QVector<QString> fieldKeys;
    fillSelectorHeaders(headers, FIELD_MAP_DOWNLOADS, {"filename", "agent_name", "computer", "total_size", "state"}, "File ID", "id", headerLabels, fieldKeys);

    tableModel->setHeaders(headerLabels, fieldKeys);
    applySelectorTableColumns(tableView, headerLabels);

    for (int i = 0; i < fieldKeys.size(); ++i) {
        if (fieldKeys[i] == QLatin1String("date")) {
            tableView->horizontalHeader()->setSortIndicator(i, Qt::DescendingOrder);
            break;
        }
    }

    if (profile) {
        pageHelper = new PagedTableHelper(profile, QStringLiteral("/download/list"), this);
        configureSelectorPagination(pageNavBar, pageHelper);

        connect(pageHelper, &PagedTableHelper::pageReady,      this, &AxDialogDownloads::onPageReady);
        connect(pageHelper, &PagedTableHelper::errorOccurred,  this, &AxDialogDownloads::onPageError);
        connect(pageHelper, &PagedTableHelper::loadingChanged, this, [this](bool loading) {
            pageNavBar->setLoading(loading);
            tableView->setEnabled(!loading);
            chooseButton->setEnabled(!loading);
        });

        connect(pageNavBar, &PageNavBar::prevClicked, this, [this]() {
            m_offset = qMax(0, m_offset - pageNavBar->pageSize());
            loadCurrentPage();
        });
        connect(pageNavBar, &PageNavBar::nextClicked, this, [this]() {
            m_offset += pageNavBar->pageSize();
            loadCurrentPage();
        });
        connect(pageNavBar, &PageNavBar::pageSizeChanged, this, [this](int size) {
            pageHelper->setPageSize(size);
            m_offset = 0;
            loadCurrentPage();
        });
        connect(pageNavBar, &PageNavBar::filterChanged, this, [this]() {
            m_offset = 0;
            loadCurrentPage();
        });

        connect(tableView->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this](int section, Qt::SortOrder order) {
            const QString key = downloadFieldToSortKey(tableModel->fieldKey(section));
            if (key.isEmpty())
                return;
            const QString newOrder = (order == Qt::AscendingOrder) ? QStringLiteral("asc") : QStringLiteral("desc");
            if (key == m_sortCol && newOrder == m_sortOrder)
                return;
            m_sortCol = key;
            m_sortOrder = newOrder;
            m_offset = 0;
            loadCurrentPage();
        });
    } else {
        pageNavBar->setError(QStringLiteral("No auth profile"));
        pageNavBar->setPrevEnabled(false);
        pageNavBar->setNextEnabled(false);
    }
}

AxDialogDownloads::~AxDialogDownloads()
{
    if (pageHelper)
        pageHelper->cancel();
}

void AxDialogDownloads::prepare()
{
    selectedData.clear();
    m_offset = 0;
    if (pageHelper)
        loadCurrentPage();
}

void AxDialogDownloads::loadCurrentPage()
{
    if (!pageHelper)
        return;
    pageHelper->setPageSize(pageNavBar->pageSize());
    pageHelper->setParam(QStringLiteral("q"), pageNavBar->filterText());
    pageHelper->setParam(QStringLiteral("sort"), m_sortCol);
    pageHelper->setParam(QStringLiteral("order"), m_sortOrder);
    pageHelper->loadPage(m_offset);
}

void AxDialogDownloads::onPageReady(const QJsonObject& response)
{
    QJsonArray items = response.value(QStringLiteral("items")).toArray();

    QVector<TransferData> page;
    page.reserve(items.size());

    for (const QJsonValue& v : items) {
        QJsonObject obj = v.toObject();
        TransferData t;
        t.FileId        = parseI64(obj, QStringLiteral("t_file_id"));
        t.AgentId       = parseI64(obj, QStringLiteral("t_agent_id"));
        t.AgentName     = obj[QStringLiteral("t_agent_name")].toString();
        t.User          = obj[QStringLiteral("t_user")].toString();
        t.Computer      = obj[QStringLiteral("t_computer")].toString();
        t.Filename      = obj[QStringLiteral("t_remote_path")].toString();
        t.TotalSize     = parseI64(obj, QStringLiteral("t_total_size"));
        t.Progress      = parseI64(obj, QStringLiteral("t_progress"));
        t.DateTimestamp = parseI64(obj, QStringLiteral("t_date"));
        t.Date          = UnixTimestampGlobalToStringLocal(t.DateTimestamp);
        t.State         = obj[QStringLiteral("t_state")].toInt();
        t.Tag           = obj[QStringLiteral("t_tag")].toString();
        t.Kind          = obj[QStringLiteral("t_kind")].toInt();
        page.append(t);
    }

    tableModel->setData(page);

    const int total = response[QStringLiteral("total")].toInt();
    const int shown = page.size();
    const int from  = shown == 0 ? 0 : m_offset + 1;
    const int to    = m_offset + shown;
    pageNavBar->setInfo(from, to, total);
    pageNavBar->setPrevEnabled(m_offset > 0);
    pageNavBar->setNextEnabled(m_offset + shown < total);
}

void AxDialogDownloads::onPageError(const QString& message)
{
    if (message.contains(QStringLiteral("invalid filter"), Qt::CaseInsensitive) || message.startsWith(QStringLiteral("filter:"), Qt::CaseInsensitive))
        return;

    tableModel->setData({});
    pageNavBar->setError(message);
    pageNavBar->setPrevEnabled(false);
    pageNavBar->setNextEnabled(false);
}

QVector<TransferData> AxDialogDownloads::data() { return selectedData; }

void AxDialogDownloads::onClicked()
{
    selectedData.clear();
    const QModelIndexList selected = tableView->selectionModel()->selectedRows();
    for (const auto& index : selected) {
        if (index.isValid())
            selectedData.append(tableModel->getDownload(index.row()));
    }
    this->accept();
}

AxSelectorDownloads::AxSelectorDownloads(const QJSValue &headers, AxScriptEngine* jsEngine, QObject* parent) : QObject(parent), scriptEngine(jsEngine)
{
    dialog = new AxDialogDownloads(headers, profileFromEngine(jsEngine), nullptr);
    prepareSelectorDialog(dialog, "Choose download", jsEngine);
}

void AxSelectorDownloads::setSize(const int w, const int h ) const
{
    if (dialog)
        dialog->resize(w, h);
}

QJSValue AxSelectorDownloads::exec() const
{
    if (!dialog || !scriptEngine || !scriptEngine->engine())
        return emptyJsArray(scriptEngine);

    prepareSelectorDialog(dialog, "Choose download", scriptEngine);
    dialog->prepare();

    QVector<TransferData> vecDownloads;
    if (execSelectorDialog(dialog, scriptEngine) == QDialog::Accepted) {
        vecDownloads = dialog->data();
    }

    QVariantList list;
    for (auto download : vecDownloads) {
        QString state;
        switch (download.State) {
            case TRANSFER_STATE_RUNNING:  state = "running";  break;
            case TRANSFER_STATE_STOPPED:  state = "stopped";  break;
            case TRANSFER_STATE_FINISHED: state = "finished"; break;
            default:                      state = "canceled"; break;
        }

        QVariantMap map;
        map["id"]         = QVariant::fromValue(download.FileId);
        map["agent_id"]   = download.AgentId;
        map["agent_name"] = download.AgentName;
        map["user"]       = download.User;
        map["computer"]   = download.Computer;
        map["filename"]   = download.Filename;
        map["total_size"] = download.TotalSize;
        map["recv_size"]  = download.Progress;
        map["state"]      = state;
        map["date"]       = download.Date;
        list.append(map);
    }
    return scriptEngine->engine()->toScriptValue(list);
}

void AxSelectorDownloads::close() const
{
    if (dialog)
        dialog->close();
}



/// DOCK WIDGET

#include <UI/Widgets/AdaptixWidget.h>
#include <Client/AuthProfile.h>
#include <MainAdaptix.h>
#include <UI/MainUI.h>

AxDockWrapper::AxDockWrapper(AdaptixWidget* w, const QString& id, const QString& title, const QString& location): DockTab(title, w->GetProfile()->GetProject())
{
    adaptixWidget = w;
    QString project = w->GetProfile()->GetProject();

    contentWidget = new QWidget();
    contentWidget->setProperty("Main", "base");

    dockWidget->setWidget(contentWidget);

    dockId = id + "-" + project;
    dockTitle = title;

    if (adaptixWidget) {
        adaptixWidget->AddExtDock(dockId, title, [this]() {
            show();
        });
    }

    if (location == "top") {
        w->PlaceDock(w->get_dockTop() , dockWidget);
        dockWidget->toggleAction()->trigger();
    }
    else if (location == "bottom") {
        w->PlaceDock(w->get_dockBottom() , dockWidget);
        dockWidget->toggleAction()->trigger();
    }
    else {
        dockWidget->open();
    }

    connect(dockWidget, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged, this, [this](bool open) {
        if (!open) {
            Q_EMIT hidden();
        } else {
            Q_EMIT shown();
        }
    });
}

AxDockWrapper::~AxDockWrapper()
{
    if (adaptixWidget)
        adaptixWidget->RemoveExtDock(dockId);

    if (dockWidget) {
        dockWidget->close();
        dockWidget->deleteLater();
    }
}

void AxDockWrapper::setLayout(QObject* layoutWrapper)
{
    if (auto* lw = dynamic_cast<AbstractAxLayout*>(layoutWrapper)) {
        if (contentWidget->layout())
            delete contentWidget->layout();
        contentWidget->setLayout(lw->layout());
    }
}

void AxDockWrapper::setSize(const int w, const int h) const
{
    if (contentWidget)
        contentWidget->resize(w, h);
}

void AxDockWrapper::show()
{
    if (!dockWidget)
        return;

    if ( !dockWidget->isOpen() )
        dockWidget->toggleAction()->trigger();
}

void AxDockWrapper::hide()
{
    if (dockWidget)
        dockWidget->close();
}

void AxDockWrapper::close()
{
    hide();
    Q_EMIT closed();
}

bool AxDockWrapper::isVisible() const
{
    return dockWidget && dockWidget->isOpen();
}

void AxDockWrapper::setTitle(const QString& title)
{
    dockTitle = title;
    if (dockWidget)
        dockWidget->setTitle(title);
}