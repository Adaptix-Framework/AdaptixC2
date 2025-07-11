#ifndef BRIDGEMENU_H
#define BRIDGEMENU_H

#include <QJSValue>
#include <QObject>

class AxScriptEngine;
class AbstractAxMenuItem;
class AxActionWrapper;
class AxMenuWrapper;
class AxSeparatorWrapper;

class BridgeMenu : public QObject {
Q_OBJECT
    AxScriptEngine* scriptEngine;
    QWidget*        widget;
    QList<AbstractAxMenuItem*> menuItems;

    const QList<AbstractAxMenuItem*> items() const;
    void clear();

public:
    explicit BridgeMenu(AxScriptEngine* scriptEngine, QObject* parent = nullptr);
    ~BridgeMenu() override;

public slots:
    AxActionWrapper*    create_action(const QString& text, const QJSValue& handler);
    AxMenuWrapper*      create_menu(const QString& title);
    AxSeparatorWrapper* create_separator();

    void add_session_main(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue()) const;
    void add_session_agent(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue()) const;
    void add_session_browser(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue()) const;
    void add_session_access(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue()) const;
};

#endif
