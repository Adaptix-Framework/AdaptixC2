#include <Client/AxScript/BridgeForm.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxUiFactory.h>

#include <QMessageBox>
#include <QMetaMethod>
#include <QJsonDocument>
#include <QThread>
#include <QApplication>



BridgeForm::BridgeForm(AxScriptEngine* scriptEngine, QObject* parent) : QObject(parent), scriptEngine(scriptEngine), uiFactory(nullptr), localParentWidget(nullptr)
{
    if (scriptEngine && scriptEngine->manager()) {
        uiFactory = scriptEngine->manager()->GetUiFactory();
    }
    if (!uiFactory) {
        localParentWidget = new QWidget();
    }
}

BridgeForm::~BridgeForm()
{
    delete localParentWidget;
}

QWidget* BridgeForm::getParentWidget() const
{
    if (uiFactory) {
        return uiFactory->getParentWidget();
    }
    return localParentWidget;
}

void BridgeForm::connect(QObject* sender, const QString& signalName, const QJSValue& handler)
{
    if (!sender || !handler.isCallable()) {
        Q_EMIT scriptError("connect -> Invalid sender or handler");
        return;
    }

    const QMetaObject* meta = sender->metaObject();
    for (int i = 0; i < meta->methodCount(); ++i) {
        QMetaMethod method = meta->method(i);
        if (method.methodType() != QMetaMethod::Signal)
            continue;

        QString name = QString::fromLatin1(method.methodSignature());
        if (!name.startsWith(signalName))
            continue;

        int paramCount = method.parameterCount();
        auto* proxy = new SignalProxy(scriptEngine->engine(), handler, sender);

        bool connected = false;

        if (paramCount == 0) {
            connected = QObject::connect(sender, method, proxy, proxy->metaObject()->method(proxy->metaObject()->indexOfSlot("call()")));
        }
        else if (paramCount == 1) {
            QByteArray typeName = method.parameterTypes().value(0);
            if (typeName == "QString")
                connected = QObject::connect(sender, method, proxy, proxy->metaObject()->method(proxy->metaObject()->indexOfSlot("callWithArg(QString)")));
            else if (typeName == "int")
                connected = QObject::connect(sender, method, proxy, proxy->metaObject()->method(proxy->metaObject()->indexOfSlot("callWithArg(int)")));
            else if (typeName == "bool")
                connected = QObject::connect(sender, method, proxy, proxy->metaObject()->method(proxy->metaObject()->indexOfSlot("callWithArg(bool)")));
        }
        else if (paramCount == 2) {
            QByteArray typeName1 = method.parameterTypes().value(0);
            QByteArray typeName2 = method.parameterTypes().value(1);
            if (typeName1 == "int" && typeName2 == "int")
                connected = QObject::connect(sender, method, proxy, proxy->metaObject()->method(proxy->metaObject()->indexOfSlot("callWithArgs(int,int)")));
        }
        else {
            Q_EMIT scriptError("connect -> Signal " + signalName + " has too many parameters (not supported)");
            return;
        }

        if (!connected)
            Q_EMIT scriptError("connect -> Failed to connect signal " + method.methodSignature());

        return;
    }

    Q_EMIT scriptError("connect -> Signal " + signalName + " not found");
}

/// Elements

QObject* BridgeForm::create_vlayout()
{
    auto* wrapper = new AxBoxLayoutWrapper(QBoxLayout::TopToBottom, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_hlayout()
{
    auto* wrapper = new AxBoxLayoutWrapper(QBoxLayout::LeftToRight, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_gridlayout()
{
    auto* wrapper = new AxGridLayoutWrapper(this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_vline()
{
    auto* wrapper = new AxLineWrapper(QFrame::VLine, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_hline()
{
    auto* wrapper = new AxLineWrapper(QFrame::HLine, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_vspacer()
{
    auto* wrapper = new AxSpacerWrapper(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_hspacer()
{
    auto* wrapper = new AxSpacerWrapper(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_label(const QString& text)
{
    auto* label = new QLabel(text, getParentWidget());
    auto* wrapper = new AxLabelWrapper(label, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_textline(const QString &text)
{
    auto* edit = new QLineEdit(text, getParentWidget());
    auto* wrapper = new AxTextLineWrapper(edit, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_combo()
{
    auto* combo = new QComboBox(getParentWidget());
    auto* wrapper = new AxComboBoxWrapper(combo, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_check(const QString& label)
{
    auto* check = new QCheckBox(label, getParentWidget());
    auto* wrapper = new AxCheckBoxWrapper(check, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_spin()
{
    auto* spin = new QSpinBox(getParentWidget());
    auto* wrapper = new AxSpinBoxWrapper(spin, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_dateline(const QString& format)
{
    auto* date_widget = new QDateEdit(getParentWidget());
    auto* wrapper = new AxDateEditWrapper(date_widget, format, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_timeline(const QString& format)
{
    auto* time_widget = new QTimeEdit(getParentWidget());
    auto* wrapper = new AxTimeEditWrapper(time_widget, format, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_button(const QString& text)
{
    auto* btn = new QPushButton(text, getParentWidget());
    auto* wrapper = new AxButtonWrapper(btn, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_textmulti(const QString& text)
{
    auto* textEdit = new QPlainTextEdit(text, getParentWidget());
    auto* wrapper = new AxTextMultiWrapper(textEdit, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_list()
{
    auto* list = new QListWidget(getParentWidget());
    auto* wrapper = new AxListWidgetWrapper(list, scriptEngine->engine(), this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_table(const QJSValue &headers)
{
    auto* table = new QTableWidget(getParentWidget());
    auto* wrapper = new AxTableWidgetWrapper(headers, table, scriptEngine->engine(), this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_selector_file()
{
    auto* lineEdit = new QLineEdit(getParentWidget());
    auto* wrapper = new AxSelectorFile(lineEdit, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_tabs()
{
    auto* tabWidget = new QTabWidget(getParentWidget());
    auto* wrapper = new AxTabWrapper(tabWidget, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_groupbox(const QString &title, const bool checkable)
{
    auto* box = new QGroupBox(title, getParentWidget());
    auto* wrapper = new AxGroupBoxWrapper(checkable, box, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_hsplitter()
{
    auto* splitter = new QSplitter(Qt::Horizontal, getParentWidget());
    auto* wrapper = new AxSplitterWrapper(splitter, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_vsplitter()
{
    auto* splitter = new QSplitter(Qt::Vertical, getParentWidget());
    auto* wrapper = new AxSplitterWrapper(splitter, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_scrollarea()
{
    auto* area = new QScrollArea(getParentWidget());
    auto* wrapper = new AxScrollAreaWrapper(area, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_panel()
{
    auto* panel = new QWidget(getParentWidget());
    panel->setProperty("Main", "base");
    auto* wrapper = new AxPanelWrapper(panel, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_stack()
{
    auto* stack = new QStackedWidget(getParentWidget());
    auto* wrapper = new AxStackedWidgetWrapper(stack, this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_container()
{
    auto* wrapper = new AxContainerWrapper(scriptEngine->engine(), this);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_dialog(const QString& title) const
{
    auto* wrapper = new AxDialogWrapper(title, getParentWidget());
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_selector_credentials(const QJSValue &headers) const
{
    auto* parentW = getParentWidget();
    auto* wrapper = new AxSelectorCreds(headers, scriptEngine, parentW);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_selector_agents(const QJSValue &headers) const
{
    auto* parentW = getParentWidget();
    auto* wrapper = new AxSelectorAgents(headers, scriptEngine, parentW);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_selector_listeners(const QJSValue &headers) const
{
    auto* parentW = getParentWidget();
    auto* wrapper = new AxSelectorListeners(headers, scriptEngine, parentW);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_selector_targets(const QJSValue &headers) const
{
    auto* parentW = getParentWidget();
    auto* wrapper = new AxSelectorTargets(headers, scriptEngine, parentW);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_selector_downloads(const QJSValue &headers) const
{
    auto* parentW = getParentWidget();
    auto* wrapper = new AxSelectorDownloads(headers, scriptEngine, parentW);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}

QObject* BridgeForm::create_dock(const QString &id, const QString &title, const QString &location)
{
    AdaptixWidget* adaptixWidget = scriptEngine->manager()->GetAdaptix();

    auto* wrapper = new AxDockWrapper(adaptixWidget, id, title, location);
    scriptEngine->registerObject(wrapper);
    return wrapper;
}
