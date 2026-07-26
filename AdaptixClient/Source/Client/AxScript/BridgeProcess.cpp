#include <Client/AxScript/BridgeProcess.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/BridgeApp.h>

#include <QJSEngine>
#include <QProcess>
#include <QDir>

BridgeProcess::BridgeProcess(AxScriptEngine* engine, QObject* parent) : QObject(parent), scriptEngine(engine)
{
}

void BridgeProcess::set_workdir(const QString& path)
{
    m_workdir = path;
}

QString BridgeProcess::workdir() const
{
    return m_workdir;
}

QJSValue BridgeProcess::exec(const QString& command, const int timeoutMs) const
{
    QJSEngine* js = scriptEngine ? scriptEngine->engine() : nullptr;
    if (!js)
        return {};

    auto makeResult = [&](bool ok, int code, const QString& out, const QString& err, bool timedOut) {
        QJSValue obj = js->newObject();
        obj.setProperty(QStringLiteral("ok"), ok);
        obj.setProperty(QStringLiteral("exitCode"), code);
        obj.setProperty(QStringLiteral("stdout"), out);
        obj.setProperty(QStringLiteral("stderr"), err);
        obj.setProperty(QStringLiteral("timedOut"), timedOut);
        return obj;
    };

    if (!scriptEngine || !scriptEngine->allowProcess()) {
        const QString msg = QStringLiteral("process.exec is only available for Code Editor toolbar actions (and only when enabled in Settings)");
        if (scriptEngine && scriptEngine->app())
            scriptEngine->app()->log_error(msg);
        return makeResult(false, -1, {}, msg, false);
    }

    if (command.trimmed().isEmpty())
        return makeResult(false, -1, {}, QStringLiteral("empty command"), false);

    QProcess proc;
    if (!m_workdir.isEmpty()) {
        QString wd = m_workdir;
        QString err;
        if (!scriptEngine->resolveFsPath(wd, false, &err)) {
            if (scriptEngine->app())
                scriptEngine->app()->log_error(err.isEmpty() ? QStringLiteral("invalid workdir") : err);
            return makeResult(false, -1, {}, err.isEmpty() ? QStringLiteral("invalid workdir") : err, false);
        }
        proc.setWorkingDirectory(wd);
    }

#ifdef Q_OS_WIN
    proc.setProgram(QStringLiteral("cmd.exe"));
    proc.setArguments({QStringLiteral("/c"), command});
#else
    proc.setProgram(QStringLiteral("sh"));
    proc.setArguments({QStringLiteral("-c"), command});
#endif

    proc.start();
    if (!proc.waitForStarted(5000)) {
        return makeResult(false, -1, {}, QStringLiteral("failed to start process"), false);
    }

    const int wait = timeoutMs < 0 ? 30000 : timeoutMs;
    bool timedOut = false;
    if (wait == 0) {
        proc.waitForFinished(-1);
    } else if (!proc.waitForFinished(wait)) {
        timedOut = true;
        proc.kill();
        proc.waitForFinished(3000);
    }

    const QString out = QString::fromLocal8Bit(proc.readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(proc.readAllStandardError());
    const int code = timedOut ? -1 : proc.exitCode();
    const bool ok = !timedOut && proc.exitStatus() == QProcess::NormalExit && code == 0;
    return makeResult(ok, code, out, err, timedOut);
}
