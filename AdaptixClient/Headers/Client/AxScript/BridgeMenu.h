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
    QList<AbstractAxMenuItem*> menuItems;

    QList<AbstractAxMenuItem*> items() const;
    void clear();

public:
    explicit BridgeMenu(AxScriptEngine* scriptEngine, QObject* parent = nullptr);
    ~BridgeMenu() override;

    void reg(const QString &type, AbstractAxMenuItem *item, const QJSValue &agents, const QJSValue &os, const QJSValue &listeners);

public Q_SLOTS:
    AxActionWrapper*    create_action(const QString& text, const QJSValue& handler);
    AxMenuWrapper*      create_menu(const QString& title);
    AxSeparatorWrapper* create_separator();

    void add_session_main(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue());
    void add_session_agent(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue());
    void add_session_browser(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue());
    void add_session_access(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue());

    void add_filebrowser(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue());
    void add_processbrowser(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue());

    void add_downloads_running(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue());
    void add_downloads_finished(AbstractAxMenuItem* item, const QJSValue &agents = QJSValue(), const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue());

    void add_tasks(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue());
    void add_tasks_job(AbstractAxMenuItem* item, const QJSValue &agents, const QJSValue &os = QJSValue(), const QJSValue &listeners = QJSValue());

    void add_targets(AbstractAxMenuItem* item, const QString &position = "center");

    void add_credentials(AbstractAxMenuItem* item);

    void add_main(AbstractAxMenuItem* item);
    void add_main_projects(AbstractAxMenuItem* item);
    void add_main_axscript(AbstractAxMenuItem* item);
    void add_main_settings(AbstractAxMenuItem* item);
};

#endif
