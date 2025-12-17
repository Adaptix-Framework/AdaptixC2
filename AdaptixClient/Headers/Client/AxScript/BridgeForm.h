#ifndef BRIDGEFORM_H
#define BRIDGEFORM_H

#include <QJSEngine>
#include <QJSValue>
#include <QList>
#include <QWidget>

class AxScriptEngine;

class AxUiFactory;

class BridgeForm : public QObject {
Q_OBJECT
    AxScriptEngine* scriptEngine;
    AxUiFactory*    uiFactory;
    QWidget*        localParentWidget;

    QWidget* getParentWidget() const;

public:
    BridgeForm(AxScriptEngine* scriptEngine, QObject* parent = nullptr);
    ~BridgeForm() override;

public Q_SLOTS:
    void connect(QObject* sender, const QString& signal, const QJSValue& handler);

    /// Elements
    QObject* create_vlayout();
    QObject* create_hlayout();
    QObject* create_gridlayout();
    QObject* create_vline();
    QObject* create_hline();
    QObject* create_vspacer();
    QObject* create_hspacer();
    QObject* create_label(const QString &text = "");
    QObject* create_textline(const QString &text = "");
    QObject* create_combo();
    QObject* create_check(const QString& label= "");
    QObject* create_spin();
    QObject* create_dateline(const QString& format = "dd.MM.yyyy");
    QObject* create_timeline(const QString& format = "HH:mm:ss");
    QObject* create_button(const QString& text= "");
    QObject* create_textmulti(const QString& text= "");
    QObject* create_list();
    QObject* create_table(const QJSValue &headers);

    QObject* create_tabs();
    QObject* create_groupbox(const QString& title = "", const bool checkable = false);
    QObject* create_hsplitter();
    QObject* create_vsplitter();
    QObject* create_scrollarea();
    QObject* create_panel();
    QObject* create_stack();
    QObject* create_container();
    QObject* create_dialog(const QString &title) const;

    QObject* create_selector_file();
    QObject* create_selector_credentials(const QJSValue &headers) const;
    QObject* create_selector_agents(const QJSValue &headers) const;

Q_SIGNALS:
    void scriptError(const QString &msg);
};





class SignalProxy : public QObject {
Q_OBJECT
public:
    QJSEngine* engine;
    QJSValue   handler;

    explicit SignalProxy(QJSEngine* engine, QJSValue handler, QObject* parent = nullptr) : QObject(parent), engine(engine), handler(std::move(handler)) {}

public Q_SLOTS:
    void call() const {
        if (handler.isCallable())
            handler.call();
    }

    void callWithArg(const bool &arg) const {
        if (handler.isCallable()) {
            QJSValueList args;
            args << engine->toScriptValue(arg);
            handler.call(args);
        }
    }

    void callWithArg(const int &arg) const {
        if (handler.isCallable()) {
            QJSValueList args;
            args << engine->toScriptValue(arg);
            handler.call(args);
        }
    }

    void callWithArg(const QString& arg) const {
        if (handler.isCallable()) {
            QJSValueList args;
            args << engine->toScriptValue(arg);
            handler.call(args);
        }
    }

    void callWithArgs(const int &arg1, const int &arg2) const {
        if (handler.isCallable()) {
            QJSValueList args;
            args << engine->toScriptValue(arg1);
            args << engine->toScriptValue(arg2);
            handler.call(args);
        }
    }
};

#endif
