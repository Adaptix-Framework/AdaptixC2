#ifndef AXSCRIPTWORKER_H
#define AXSCRIPTWORKER_H

#include <QObject>
#include <QThread>
#include <QJSValue>
#include <QMutex>
#include <QWaitCondition>

class AxScriptEngine;
class AxScriptManager;

class AxScriptWorker : public QObject {
    Q_OBJECT

    QThread*         workerThread;
    AxScriptEngine*  scriptEngine;
    AxScriptManager* scriptManager;
    QString          scriptName;

    QMutex           mutex;
    QWaitCondition   condition;
    bool             ready = false;

public:
    explicit AxScriptWorker(AxScriptManager* manager, const QString& name, const QObject* parent = nullptr);
    ~AxScriptWorker() override;

    AxScriptEngine* engine() const;
    QThread* thread() const;

    void executeAsync(const QString& code);
    bool executeSync(const QString& code);

    void waitForReady();

public Q_SLOTS:
    void initialize();
    void doExecute(const QString& code);

Q_SIGNALS:
    void executeRequested(const QString& code);
    void executionFinished(bool success, const QString& error);
    void initialized();
};

#endif
