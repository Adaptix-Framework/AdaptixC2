#include <Client/AxScript/AxScriptWorker.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxScriptManager.h>

AxScriptWorker::AxScriptWorker(AxScriptManager* manager, const QString& name, const QObject* parent)
    : QObject(nullptr)
    , scriptEngine(nullptr)
    , scriptManager(manager)
    , scriptName(name)
{
    Q_UNUSED(parent);
    
    workerThread = new QThread();

    this->moveToThread(workerThread);

    connect(workerThread, &QThread::started, this, &AxScriptWorker::initialize);
    connect(this, &AxScriptWorker::executeRequested, this, &AxScriptWorker::doExecute);

    workerThread->start();
}

AxScriptWorker::~AxScriptWorker()
{
    if (workerThread && workerThread->isRunning()) {
        workerThread->quit();
        workerThread->wait();
    }

    if (scriptEngine) {
        delete scriptEngine;
        scriptEngine = nullptr;
    }

    delete workerThread;
    workerThread = nullptr;
}

AxScriptEngine* AxScriptWorker::engine() const
{
    return scriptEngine;
}

QThread* AxScriptWorker::thread() const
{
    return workerThread;
}

void AxScriptWorker::initialize()
{
    scriptEngine = new AxScriptEngine(scriptManager, scriptName, nullptr);

    QMutexLocker locker(&mutex);
    ready = true;
    condition.wakeAll();

    Q_EMIT initialized();
}

void AxScriptWorker::waitForReady()
{
    QMutexLocker locker(&mutex);
    while (!ready) {
        condition.wait(&mutex);
    }
}

void AxScriptWorker::executeAsync(const QString& code)
{
    Q_EMIT executeRequested(code);
}

bool AxScriptWorker::executeSync(const QString& code)
{
    QMutex syncMutex;
    QWaitCondition syncCondition;
    bool success = false;

    QMetaObject::Connection conn = connect(this, &AxScriptWorker::executionFinished,
        [&syncMutex, &syncCondition, &success](bool result, const QString&) {
            QMutexLocker locker(&syncMutex);
            success = result;
            syncCondition.wakeAll();
        });

    Q_EMIT executeRequested(code);

    QMutexLocker locker(&syncMutex);
    syncCondition.wait(&syncMutex);

    disconnect(conn);
    return success;
}

void AxScriptWorker::doExecute(const QString& code)
{
    if (!scriptEngine) {
        Q_EMIT executionFinished(false, "Script engine not initialized");
        return;
    }

    bool success = scriptEngine->execute(code);
    QString error;

    if (!success) {
        QJSValue result = scriptEngine->context.scriptObject;
        if (result.isError()) {
            error = QString("%1 at line %2")
                .arg(result.toString())
                .arg(result.property("lineNumber").toInt());
        }
    }

    Q_EMIT executionFinished(success, error);
}
