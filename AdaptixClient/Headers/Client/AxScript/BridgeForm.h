#ifndef BRIDGEFORM_H
#define BRIDGEFORM_H

#include <QJSEngine>
#include <QJSValue>
#include <QList>
#include <QWidget>

class AxScriptEngine;

class BridgeForm : public QObject {
Q_OBJECT
    AxScriptEngine* scriptEngine;
    QWidget*        widget;

public:
    BridgeForm(AxScriptEngine* scriptEngine, QObject* parent = nullptr);
    ~BridgeForm() override;

public slots:
    void connect(QObject* sender, const QString& signal, const QJSValue& handler);

    void show_message(const QString &title, const QString &text);

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
    QObject* create_file_selector();
    QObject* create_tabs();
    QObject* create_panel();

    QObject* create_container();
    QObject* create_dialog(const QString &title) const;

signals:
    void scriptError(const QString &msg);
};





class SignalProxy : public QObject {
Q_OBJECT
public:
    QJSEngine* engine;
    QJSValue   handler;

    explicit SignalProxy(QJSEngine* engine, QJSValue handler, QObject* parent = nullptr) : QObject(parent), engine(engine), handler(std::move(handler)) {}

public slots:
    void call() const {
        if (handler.isCallable())
            handler.call();
    }

    void callWithArg(const QVariant &arg) const {
        if (handler.isCallable()) {
            QJSValueList args;
            args << engine->toScriptValue(arg);
            handler.call(args);
        }
    }

    void callWithArgs(const QVariant &arg1, const QVariant &arg2) const {
        if (handler.isCallable()) {
            QJSValueList args;
            args << engine->toScriptValue(arg1);
            args << engine->toScriptValue(arg2);
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
};

#endif
