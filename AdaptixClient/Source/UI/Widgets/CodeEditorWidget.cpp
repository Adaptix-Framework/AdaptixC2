#include <UI/Widgets/CodeEditorWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/ScriptsWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Agent/Agent.h>
#include <Client/AuthProfile.h>
#include <Client/Settings.h>
#include <Client/Extender.h>
#include <Client/CodeEditorProfileManager.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Client/Requestor.h>
#include <MainAdaptix.h>

#include <QPointer>
#include <QFile>
#include <QIcon>
#include <QJSValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

#include <CodeEditorView.h>
#include <EditorTabWidget.h>
#include <CodeEditor.h>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QWidget>
#include <FileBrowser.h>
#include <StyleSyntaxHighlighter.h>
#include <EditorTheme.hpp>
#include <oclero/qlementine/style/QlementineStyle.hpp>

#include <QVBoxLayout>
#include <QProcess>
#include <QAction>
#include <QStyle>
#include <QToolBar>
#include <QComboBox>
#include <QFileInfo>
#include <QDateTime>
#include <QTimer>
#include <QKeySequence>
#include <QInputDialog>
#include <QMessageBox>

REGISTER_DOCK_WIDGET(CodeEditorWidget, "Code Editor", false)

QList<QPointer<CodeEditorWidget>> CodeEditorWidget::s_instances;
int CodeEditorWidget::s_nextInstanceId = 1;

CodeEditorActionApi::CodeEditorActionApi(CodeEditorWidget* host, QObject* parent): QObject(parent), m_host(host)
{
}

void CodeEditorActionApi::save()
{
    if (m_host && m_host->m_editor)
        m_host->m_editor->saveFile();
}

QString CodeEditorActionApi::file() const
{
    if (!m_host)
        return {};
    if (auto* ed = m_host->currentEditor())
        return ed->filePath();
    return {};
}

QString CodeEditorActionApi::content() const
{
    if (!m_host)
        return {};
    if (auto* ed = m_host->currentEditor())
        return ed->toPlainText();
    return {};
}

QString CodeEditorActionApi::agent_id() const
{
    if (!m_host || m_host->agentId() == 0)
        return {};
    return QString::number(m_host->agentId());
}

QVariant CodeEditorActionApi::get_panel_data(const QString& key) const
{
    if (!m_host)
        return {};
    if (key.isEmpty())
        return m_host->panelStateMap();
    return m_host->panelFieldValue(key);
}

QString CodeEditorActionApi::expand(const QString& tmpl) const
{
    if (!m_host)
        return tmpl;
    QString filePath;
    if (auto* ed = m_host->currentEditor())
        filePath = ed->filePath();
    return CodeEditorWidget::expandCommandTemplate(
        tmpl, filePath, CodeEditorWidget::expandDefines(m_host->panelDefinesRaw()));
}

void CodeEditorActionApi::log(const QString& text)
{
    if (m_host)
        m_host->appendToLog(text);
}

bool CodeEditorActionApi::eval(const QString& code, const QJSValue& opts)
{
    if (!m_host)
        return false;
    bool useMain = false;
    if (opts.isObject() && opts.hasProperty(QStringLiteral("main")))
        useMain = opts.property(QStringLiteral("main")).toBool();
    return m_host->evalScript(code, useMain);
}

QString CodeEditorActionApi::job_start(const QString& cmd, const QJSValue& opts)
{
    if (!m_host)
        return {};
    QString id;
    QString cwd;
    if (opts.isObject()) {
        if (opts.hasProperty(QStringLiteral("id")))
            id = opts.property(QStringLiteral("id")).toString();
        if (opts.hasProperty(QStringLiteral("cwd")))
            cwd = opts.property(QStringLiteral("cwd")).toString();
    }
    return m_host->startJob(cmd, id, cwd);
}

bool CodeEditorActionApi::job_stop(const QString& id)
{
    return m_host && m_host->stopJob(id);
}

bool CodeEditorActionApi::job_running(const QString& id) const
{
    return m_host && m_host->isJobRunning(id);
}

void CodeEditorActionApi::job_stop_all()
{
    if (m_host)
        m_host->stopAllJobs();
}

CodeEditorWidget::CodeEditorWidget(AdaptixWidget* w, Agent* agent) : DockTab(agent ? QStringLiteral("Code Editor [%1]").arg(agent->data.Id) : QStringLiteral("Code Editor"), w->GetProfile()->GetProject(), ":/icons/code")
{
    m_adaptix    = w;
    m_agent      = agent;
    m_agentId    = agent ? agent->data.Id : 0;
    m_instanceId = s_nextInstanceId++;
    if (w && w->ScriptManager)
        m_sm = w->ScriptManager;
    m_profiles = CodeEditorProfileManager::instance();
    s_instances.append(this);

    createUI();
    setupConnections();

    m_sessionProfileId = QStringLiteral("system.axscript");
    if (!isAgentBound() && m_profiles)
        m_profiles->setCurrent(m_sessionProfileId);
    refreshProfileCombo();

    this->dockWidget->setWidget(this);

    if (dockWidget) {
        connect(dockWidget, &KDDockWidgets::QtWidgets::DockWidget::isCurrentTabChanged,
                this, [this](bool current) {
                    if (!current)
                        return;
                    s_instances.removeAll(this);
                    s_instances.append(this);
                });
    }
}

CodeEditorWidget::~CodeEditorWidget()
{
    m_applyingProfile = true;
    stopAllJobs();
    clearAxScriptPanel();
    s_instances.removeAll(this);
}

CodeEditorWidget* CodeEditorWidget::activeInstance()
{
    for (int i = s_instances.size() - 1; i >= 0; --i) {
        if (s_instances[i])
            return s_instances[i].data();
    }
    return nullptr;
}

void CodeEditorWidget::postLog(const QString& text, const QColor& color)
{
    if (auto* w = activeInstance())
        w->appendToLog(text, color);
}

void CodeEditorWidget::appendToLog(const QString& text, const QColor& color)
{
    if (!m_editor)
        return;
    m_editor->setLogPanelVisible(true);
    QString line = text;
    if (!line.endsWith(QLatin1Char('\n')))
        line += QLatin1Char('\n');
    m_editor->appendLog(line, color);
}

void CodeEditorWidget::appendJobLog(const QString& jobId, const QString& text, const QColor& color)
{
    const QString prefix = jobId.isEmpty() ? QString() : QStringLiteral("[%1] ").arg(jobId);
    for (const QString& part : text.split(QLatin1Char('\n'))) {
        if (part.isEmpty())
            continue;
        appendToLog(prefix + part, color);
    }
}

QString CodeEditorWidget::startJob(const QString& cmd, const QString& jobId, const QString& cwd)
{
    if (cmd.trimmed().isEmpty()) {
        appendToLog(QStringLiteral("job_start: empty command"), QColor(0xfbc064));
        return {};
    }

    QString id = jobId.trimmed();
    if (id.isEmpty())
        id = QStringLiteral("job_%1").arg(++m_jobSeq);

    if (m_jobs.contains(id) && m_jobs[id]
        && m_jobs[id]->state() != QProcess::NotRunning) {
        appendToLog(QStringLiteral("job_start: '%1' still running — stop it first").arg(id), QColor(0xfbc064));
        return {};
    }

    if (m_jobs.contains(id)) {
        auto* old = m_jobs.take(id);
        if (old) {
            old->disconnect();
            old->deleteLater();
        }
    }

    auto* proc = new QProcess(this);
    m_jobs.insert(id, proc);
    m_lastJobId = id;

    QString workDir = cwd;
    if (workDir.isEmpty())
        workDir = workDirectoryForCurrentEditor();
    if (!workDir.isEmpty())
        proc->setWorkingDirectory(workDir);

    wireJobProcess(id, proc);

    appendToLog(QStringLiteral("[%1] start: %2").arg(id, cmd), QColor(0x3daee9));
    proc->start(QStringLiteral("sh"), {QStringLiteral("-c"), cmd});
    if (!proc->waitForStarted(5000)) {
        appendToLog(QStringLiteral("[%1] failed to start").arg(id), QColor(0xf44747));
        m_jobs.remove(id);
        proc->deleteLater();
        return {};
    }
    updateActionEnabled();
    return id;
}

void CodeEditorWidget::wireJobProcess(const QString& jobId, QProcess* proc)
{
    connect(proc, &QProcess::readyReadStandardOutput, this, [this, jobId, proc]() {
        appendJobLog(jobId, QString::fromLocal8Bit(proc->readAllStandardOutput()), QColor(0xd4d4d4));
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, jobId, proc]() {
        appendJobLog(jobId, QString::fromLocal8Bit(proc->readAllStandardError()), QColor(0xf44747));
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, jobId, proc](int code, QProcess::ExitStatus status) {
                const bool ok = (status == QProcess::NormalExit && code == 0);
                appendToLog(QStringLiteral("[%1] finished exit=%2").arg(jobId).arg(code),
                            ok ? QColor(0x6a9955) : QColor(0xf44747));
                if (m_jobs.value(jobId) == proc)
                    m_jobs.remove(jobId);
                proc->deleteLater();
                updateActionEnabled();
            });
}

bool CodeEditorWidget::stopJob(const QString& jobId)
{
    QString id = jobId.trimmed();
    if (id.isEmpty())
        id = m_lastJobId;
    if (id.isEmpty() || !m_jobs.contains(id))
        return false;
    QProcess* proc = m_jobs.value(id);
    if (!proc || proc->state() == QProcess::NotRunning)
        return false;
    appendToLog(QStringLiteral("[%1] stopping…").arg(id), QColor(0xfbc064));
    proc->kill();
    return true;
}

bool CodeEditorWidget::isJobRunning(const QString& jobId) const
{
    QString id = jobId.trimmed();
    if (id.isEmpty())
        id = m_lastJobId;
    QProcess* proc = m_jobs.value(id);
    return proc && proc->state() != QProcess::NotRunning;
}

void CodeEditorWidget::stopAllJobs()
{
    const auto ids = m_jobs.keys();
    for (const QString& id : ids)
        stopJob(id);
    updateActionEnabled();
}

void CodeEditorWidget::createUI()
{
    m_editor = new CodeEditorView(this);

    auto* tb = m_editor->toolBar();

    auto* spring = new QWidget(tb);
    spring->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spring);

    m_profileCombo = new QComboBox(tb);
    m_profileCombo->setMinimumWidth(120);
    m_profileCombo->setMaximumWidth(200);
    m_profileCombo->setToolTip(QStringLiteral("Editor profile (edit in Settings → Code Editor)"));
    m_profileComboAction = tb->addWidget(m_profileCombo);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_editor);
}

void CodeEditorWidget::setupConnections()
{
    connect(m_profiles, &CodeEditorProfileManager::profilesChanged, this, [this]() { refreshProfileCombo(); });
    connect(m_profiles, &CodeEditorProfileManager::currentChanged, this, [this](const BuildProfile& p) {
        if (p.id == m_sessionProfileId || p.name == m_sessionProfileId)
            updateActionAvailability(p);
        if (!isAgentBound() && m_profileCombo) {
            int idx = -1;
            for (int i = 0; i < m_profileCombo->count(); ++i) {
                if (m_profileCombo->itemData(i).toString() == p.id || m_profileCombo->itemText(i) == p.name) {
                    idx = i;
                    break;
                }
            }
            if (idx >= 0 && m_profileCombo->currentIndex() != idx) {
                QSignalBlocker b(m_profileCombo);
                m_profileCombo->setCurrentIndex(idx);
            }
        }
    });

    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CodeEditorWidget::onProfileChanged);

    if (m_editor) {
        connect(m_editor, &CodeEditorView::currentEditorChanged, this, [this](CodeEditor*) {
            if (const BuildProfile* p = sessionProfile())
                m_editor->applyLanguage(p->language);
        });
    }
    connect(m_editor, &CodeEditorView::currentEditorChanged, this, [this](CodeEditor*) {
        updateActionEnabled();
    });

    connect(m_editor, &CodeEditorView::fileOpened, this, [this](const QString& filePath) {
        const QString suggested = BuildProfile::profileNameForFile(filePath);
        if (!suggested.isEmpty())
            setSessionProfile(suggested, /*persistGlobal=*/!isAgentBound());
    });

    if (auto* tw = m_editor->tabWidget()) {
        connect(tw, &EditorTabWidget::tabCloseRequested, this, [this](int index) {
            auto* ed = m_editor->tabWidget()->editorAt(index);
            if (ed) {
                const QString key = ed->filePath().isEmpty() ? QStringLiteral("untitled") : ed->filePath();
                removeDevEngine(key);
            }
        });
    }
}

void CodeEditorWidget::onScriptConsoleError(const QString& message)
{
    if (!m_editor)
        return;
    m_editor->appendLog(message.endsWith(QLatin1Char('\n')) ? message : message + QLatin1Char('\n'), QColor(0xf44747));
}

void CodeEditorWidget::connectConsoleSignals(AxScriptManager* sm)
{
    if (!sm)
        return;

    m_sm = sm;
    connect(sm, &AxScriptManager::consoleError, this, &CodeEditorWidget::onScriptConsoleError, Qt::UniqueConnection);
    applyTheme();
}

CodeEditor* CodeEditorWidget::currentEditor() const
{
    return m_editor ? m_editor->currentEditor() : nullptr;
}

void CodeEditorWidget::openScript(const QString& filePath)
{
    if (!m_editor)
        return;

    const QString dir = QFileInfo(filePath).absolutePath();
    if (!dir.isEmpty()) {
        m_editor->setProjectPath(dir);
        if (auto* fb = m_editor->fileBrowser())
            fb->setRootPath(dir);
        m_editor->setSidebarVisible(true);
    }

    m_editor->loadFile(filePath);
}

void CodeEditorWidget::openScriptContent(const QString& fileName, const QString& content, const QString& documentKey)
{
    if (!m_editor)
        return;
    m_editor->openContent(fileName, content, documentKey);
    updateActionEnabled();
}

void CodeEditorWidget::applyTheme()
{
    if (!m_editor)
        return;

    auto* style = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp->style());
    SyntaxStyle* syntaxStyle = style
        ? EditorTheme::createFromQlementine(style->theme(), this)
        : EditorTheme::createDarkTheme(this);
    m_editor->setSyntaxStyle(syntaxStyle);
}

QList<BuildProfile> CodeEditorWidget::sessionVisibleProfiles() const
{
    if (!m_profiles)
        return {};
    CodeEditorOpenOptions opts;
    opts.restrictProfiles = m_restrictProfiles;
    opts.profiles = m_allowedProfiles;
    return m_profiles->visibleProfiles(opts);
}

void CodeEditorWidget::refreshProfileCombo()
{
    if (!m_profileCombo || !m_profiles)
        return;

    const QList<BuildProfile> visible = sessionVisibleProfiles();

    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();

    QHash<QString, int> nameCount;
    for (const auto& p : visible)
        nameCount[p.name] = nameCount.value(p.name, 0) + 1;

    int selectIdx = -1;
    for (const auto& p : visible) {
        QString label = p.name;
        if (nameCount.value(p.name, 0) > 1)
            label = QStringLiteral("%1 (%2)").arg(p.name, p.id);
        m_profileCombo->addItem(label, p.id);
        if (p.id == m_sessionProfileId || p.name == m_sessionProfileId)
            selectIdx = m_profileCombo->count() - 1;
    }

    if (selectIdx < 0 && m_profileCombo->count() > 0) {
        selectIdx = 0;
        m_sessionProfileId = m_profileCombo->itemData(0).toString();
    } else if (selectIdx >= 0) {
        m_sessionProfileId = m_profileCombo->itemData(selectIdx).toString();
    } else {
        m_sessionProfileId.clear();
    }

    if (selectIdx >= 0)
        m_profileCombo->setCurrentIndex(selectIdx);
    m_profileCombo->blockSignals(false);

    applySessionProfile();
}

const BuildProfile* CodeEditorWidget::sessionProfile() const
{
    if (!m_profiles)
        return nullptr;
    if (const BuildProfile* p = m_profiles->profile(m_sessionProfileId))
        return p;
    return m_profiles->current();
}

void CodeEditorWidget::applySessionProfile()
{
    if (const BuildProfile* p = sessionProfile())
        updateActionAvailability(*p);
}

void CodeEditorWidget::setSessionProfile(const QString& idOrName, bool persistGlobal)
{
    if (idOrName.isEmpty() || !m_profiles)
        return;
    const BuildProfile* p = m_profiles->profile(idOrName);
    if (!p)
        return;

    if (m_restrictProfiles) {
        bool allowed = false;
        for (const QString& key : m_allowedProfiles) {
            if (key == p->id || key == p->name) {
                allowed = true;
                break;
            }
        }
        if (!allowed)
            return;
    }

    const bool changed = (m_sessionProfileId != p->id);
    m_sessionProfileId = p->id;

    if (m_profileCombo) {
        int idx = -1;
        for (int i = 0; i < m_profileCombo->count(); ++i) {
            if (m_profileCombo->itemData(i).toString() == p->id) {
                idx = i;
                break;
            }
        }
        if (idx >= 0 && m_profileCombo->currentIndex() != idx) {
            QSignalBlocker b(m_profileCombo);
            m_profileCombo->setCurrentIndex(idx);
        }
    }

    if (changed)
        applySessionProfile();

    if (persistGlobal)
        m_profiles->setCurrent(p->id);
}

void CodeEditorWidget::selectProfile(const QString& idOrName)
{
    setSessionProfile(idOrName, /*persistGlobal=*/!isAgentBound());
}

void CodeEditorWidget::applyOpenOptions(const CodeEditorOpenOptions& opts)
{
    m_restrictProfiles = opts.restrictProfiles;
    m_allowedProfiles  = opts.profiles;
    refreshProfileCombo();
    if (!opts.profile.isEmpty())
        selectProfile(opts.profile);
    else if (m_restrictProfiles && !m_allowedProfiles.isEmpty())
        selectProfile(m_allowedProfiles.first());
}

QString CodeEditorWidget::defaultPanelScriptTemplate()
{
    return BuildProfile::defaultCustomPanelScript();
}

QString CodeEditorWidget::jsQuote(const QString& s)
{
    QString out = s;
    out.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    out.replace(QLatin1Char('"'), QLatin1String("\\\""));
    out.replace(QLatin1Char('\n'), QLatin1String("\\n"));
    out.replace(QLatin1Char('\r'), QLatin1String("\\r"));
    return QStringLiteral("\"%1\"").arg(out);
}

QString CodeEditorWidget::expandCommandTemplate(const QString& tmpl, const QString& filePath, const QString& defines)
{
    QString out = tmpl;
    QString outPath = filePath;
    if (!filePath.isEmpty()) {
        QFileInfo fi(filePath);
        outPath = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".o");
    }
    out.replace(QStringLiteral("%f"), filePath);
    out.replace(QStringLiteral("%o"), outPath);
    out.replace(QStringLiteral("%d"), defines);
    return out;
}

void CodeEditorWidget::clearAxScriptPanel()
{
    QWidget* panelW = m_panelWidget;
    m_panelWidget = nullptr;
    m_panelContainer = nullptr;

    if (m_editor)
        m_editor->applyConfigPanel(QStringLiteral("none"));

    if (panelW) {
        panelW->hide();
        panelW->setParent(nullptr);
        panelW->deleteLater();
    }

    if (m_panelEngine) {
        m_panelEngine->deleteLater();
        m_panelEngine = nullptr;
    }
}

QVariant CodeEditorWidget::panelFieldValue(const QString& id) const
{
    if (!m_panelContainer || !m_panelContainer->contains(id))
        return {};
    QObject* obj = m_panelContainer->get(id);
    if (!obj)
        return {};
    if (auto* line = dynamic_cast<AxTextLineWrapper*>(obj))
        return line->text();
    if (auto* multi = dynamic_cast<AxTextMultiWrapper*>(obj))
        return multi->text();
    if (auto* sw = dynamic_cast<AxSwitchWrapper*>(obj))
        return sw->isChecked();
    if (auto* check = dynamic_cast<AxCheckBoxWrapper*>(obj))
        return check->isChecked();
    if (auto* combo = dynamic_cast<AxComboBoxWrapper*>(obj))
        return combo->jsonMarshal();
    if (auto* spin = dynamic_cast<AxSpinBoxWrapper*>(obj))
        return spin->jsonMarshal();
    if (auto* el = dynamic_cast<AbstractAxElement*>(obj))
        return el->jsonMarshal();
    return {};
}

QVariantMap CodeEditorWidget::panelStateMap() const
{
    QVariantMap out;
    if (!m_panelContainer)
        return out;
    const QString json = m_panelContainer->toJson();
    const QJsonObject obj = QJsonDocument::fromJson(json.toUtf8()).object();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        out.insert(it.key(), it.value().toVariant());
    return out;
}

QString CodeEditorWidget::panelDefinesRaw() const
{
    const QVariant def = panelFieldValue(QStringLiteral("defines"));
    if (def.isValid())
        return def.toString();
    if (const BuildProfile* p = sessionProfile()) {
        const QJsonValue v = p->panelState.value(QStringLiteral("defines"));
        if (!v.isUndefined() && !v.isNull())
            return v.toString();
    }
    return {};
}

void CodeEditorWidget::captureAxScriptPanelToProfile()
{
    if (!m_profiles || !m_panelContainer)
        return;
    const BuildProfile* cur = sessionProfile();
    if (!cur)
        return;

    BuildProfile p = *cur;
    const QString json = m_panelContainer->toJson();
    p.panelState = QJsonDocument::fromJson(json.toUtf8()).object();
    m_profiles->updateProfile(p);
}

void CodeEditorWidget::rebuildAxScriptPanel(const BuildProfile& p)
{
    clearAxScriptPanel();
    if (!m_editor || !m_sm)
        return;
    if (p.panelScript.trimmed().isEmpty()) {
        m_editor->appendLog(QStringLiteral("AxScript panel: panel script is empty.\n"), QColor(0xd29922));
        m_editor->applyConfigPanel(QStringLiteral("none"));
        return;
    }

    m_panelEngine = new AxScriptEngine(m_sm, QStringLiteral("editor_panel_%1_%2").arg(m_instanceId).arg(p.id.isEmpty() ? p.name : p.id), this, AxScriptTrust::CodeEditor);

    QJsonObject seed = p.panelState;
    const QString stateJson = QString::fromUtf8(QJsonDocument(seed).toJson(QJsonDocument::Compact));
    const QString agentIdJs = m_agentId != 0 ? QString::number(m_agentId) : QStringLiteral("null");
    const QString preamble = QStringLiteral(
        "var __state = %1;\n"
        "var __profileName = %2;\n"
        "var __language = %3;\n"
        "var __agentId = %4;\n"
    ).arg(
        stateJson.isEmpty() ? QStringLiteral("{}") : stateJson,
        jsQuote(p.name),
        jsQuote(p.language),
        agentIdJs
    );

    if (!m_panelEngine->execute(preamble + p.panelScript)) {
        m_editor->appendLog(QStringLiteral("Panel script failed to execute.\n"), QColor(0xf44747));
        clearAxScriptPanel();
        return;
    }

    QJSValue gen = m_panelEngine->engine()->globalObject().property(QStringLiteral("GeneratePanel"));
    if (!gen.isCallable()) {
        m_editor->appendLog(QStringLiteral("Panel script must define GeneratePanel().\n"), QColor(0xf44747));
        clearAxScriptPanel();
        return;
    }

    QJSValue result = gen.call();
    if (result.isError()) {
        m_editor->appendLog(QStringLiteral("GeneratePanel error: %1\n").arg(result.toString()), QColor(0xf44747));
        clearAxScriptPanel();
        return;
    }
    if (!result.isObject()) {
        m_editor->appendLog(QStringLiteral("GeneratePanel must return an object { ui_panel, ... }.\n"), QColor(0xf44747));
        clearAxScriptPanel();
        return;
    }

    QJSValue ui_panel = result.property(QStringLiteral("ui_panel"));
    if (ui_panel.isUndefined() || !ui_panel.isQObject()) {
        m_editor->appendLog(QStringLiteral("GeneratePanel: ui_panel missing.\n"), QColor(0xf44747));
        clearAxScriptPanel();
        return;
    }

    QObject* objPanel = ui_panel.toQObject();
    auto* formElement = dynamic_cast<AbstractAxVisualElement*>(objPanel);
    if (!formElement || !formElement->widget()) {
        m_editor->appendLog(QStringLiteral("GeneratePanel: ui_panel is not a form widget.\n"), QColor(0xf44747));
        clearAxScriptPanel();
        return;
    }

    QJSValue ui_container = result.property(QStringLiteral("ui_container"));
    if (ui_container.isQObject())
        m_panelContainer = dynamic_cast<AxContainerWrapper*>(ui_container.toQObject());

    if (m_panelContainer && !seed.isEmpty())
        m_panelContainer->fromJson(QString::fromUtf8(QJsonDocument(seed).toJson(QJsonDocument::Compact)));

    m_panelWidget = formElement->widget();
    m_editor->applyConfigPanel(m_panelWidget);
}

void CodeEditorWidget::rebuildProfileToolbarActions(const BuildProfile& p)
{
    auto* tb = m_editor ? m_editor->toolBar() : nullptr;
    if (!tb)
        return;

    for (QAction* a : m_profileActions) {
        tb->removeAction(a);
        a->deleteLater();
    }
    m_profileActions.clear();
    m_actPrimary = m_actStop = nullptr;

    for (const BuildProfileAction& ca : p.customActions) {
        if (ca.label.trimmed().isEmpty() && ca.script.trimmed().isEmpty() && ca.icon.isEmpty())
            continue;
        QIcon icon;
        if (!ca.icon.isEmpty())
            icon = QIcon(ca.icon);
        const QString tip = ca.label.isEmpty() ? ca.id : ca.label;
        auto* act = tb->addAction(icon, QString());
        act->setToolTip(tip.isEmpty() ? QStringLiteral("Action") : tip);

        const QString script = ca.script;
        connect(act, &QAction::triggered, this, [this, script]() {
            runProfileActionScript(script);
        });
        m_profileActions.append(act);

        if (ca.id == QLatin1String("run") || ca.id == QLatin1String("build") || ca.id == QLatin1String("register") || ca.id == QLatin1String("test_local")) {
            if (!m_actPrimary)
                m_actPrimary = act;
        } else if (ca.id == QLatin1String("stop")) {
            m_actStop = act;
        }
    }

    if (m_profileComboAction) {
        tb->removeAction(m_profileComboAction);
        tb->addAction(m_profileComboAction);
    }
    updateActionEnabled();
}

void CodeEditorWidget::runProfileActionScript(const QString& script)
{
    if (script.trimmed().isEmpty() || !m_sm)
        return;
    captureAxScriptPanelToProfile();
    auto* engine = new AxScriptEngine(m_sm, QStringLiteral("editor_action"), this, AxScriptTrust::CodeEditorAction);
    auto* api = new CodeEditorActionApi(this, engine);
    if (QJSEngine* js = engine->engine())
        js->globalObject().setProperty(QStringLiteral("editor"), js->newQObject(api));

    if (!engine->execute(script) && m_editor)
        m_editor->appendLog(QStringLiteral("Custom action script failed.\n"), QColor(0xf44747));
    engine->deleteLater();
}

void CodeEditorWidget::updateActionEnabled()
{
    bool busy = false;
    for (auto* p : m_jobs) {
        if (p && p->state() != QProcess::NotRunning) {
            busy = true;
            break;
        }
    }
    const bool hasEd = currentEditor() != nullptr;

    if (m_actPrimary)
        m_actPrimary->setEnabled(!busy && hasEd);
    if (m_actStop)
        m_actStop->setEnabled(busy);
}

void CodeEditorWidget::updateActionAvailability(const BuildProfile& p)
{
    if (!m_editor || m_applyingProfile)
        return;

    m_applyingProfile = true;

    const BuildProfileToolbar& tb = p.toolbar;

    m_editor->applyFileToolbarFlags(tb.newFile, tb.openFile, tb.openFolder, tb.save, tb.explorer, tb.buildLog, tb.minimap, tb.wordWrap);

    if (p.profileType() == ProfileEventHandler) {
        m_editor->setSidebarVisible(false);
        m_editor->setMinimapEnabled(false);
        m_editor->setLogPanelVisible(false);
    }

    m_editor->applyLanguage(p.language);

    rebuildProfileToolbarActions(p);

    QString panel = tb.panel.isEmpty() ? QStringLiteral("axscript") : tb.panel;
    if (panel == QLatin1String("axscript")) {
        rebuildAxScriptPanel(p);
    } else if (panel == QLatin1String("build")) {
        clearAxScriptPanel();
        m_editor->applyConfigPanel(QStringLiteral("build"));
    } else {
        clearAxScriptPanel();
        m_editor->applyConfigPanel(QStringLiteral("none"));
    }

    m_applyingProfile = false;
}

void CodeEditorWidget::onProfileChanged(int index)
{
    if (index < 0 || !m_profileCombo)
        return;

    const QString id = m_profileCombo->itemData(index).toString();
    const QString key = id.isEmpty() ? m_profileCombo->itemText(index) : id;
    setSessionProfile(key, /*persistGlobal=*/!isAgentBound());
}

QString CodeEditorWidget::workDirectoryForCurrentEditor() const
{
    QString workDir = m_editor->projectPath();
    if (workDir.isEmpty()) {
        auto* editor = m_editor->currentEditor();
        if (editor && !editor->filePath().isEmpty())
            workDir = QFileInfo(editor->filePath()).absolutePath();
    }
    return workDir;
}

QString CodeEditorWidget::expandDefines(const QString& defines)
{
    QStringList result;
    for (const auto& def : defines.split(';', Qt::SkipEmptyParts)) {
        QString trimmed = def.trimmed();
        if (!trimmed.isEmpty())
            result.append("-D" + trimmed);
    }
    return result.join(" ");
}

bool CodeEditorWidget::evalScript(const QString& code, const bool useMain)
{
    if (!m_editor)
        return false;
    m_editor->setLogPanelVisible(true);
    captureAxScriptPanelToProfile();

    if (code.trimmed().isEmpty()) {
        appendToLog(QStringLiteral("eval: empty code"), QColor(0xfbc064));
        return false;
    }

    auto* ed = currentEditor();
    const QString tabKey = (ed && !ed->filePath().isEmpty()) ? ed->filePath() : QStringLiteral("untitled");

    m_editor->appendLogSeparator();
    appendToLog(QStringLiteral("[%1] eval (%2)").arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")), useMain ? QStringLiteral("main engine") : QStringLiteral("isolated")), QColor(0x3daee9));

    if (useMain)
        executeInMain(code);
    else
        executeDev(tabKey, code);
    return true;
}

bool CodeEditorWidget::ensureDevEngine(const QString& tabKey)
{
    if (!m_sm && m_adaptix)
        m_sm = m_adaptix->ScriptManager;
    if (!m_sm)
        return false;

    if (m_devEngines.contains(tabKey))
        return true;

    auto* engine = new AxScriptEngine(m_sm, tabKey, this, AxScriptTrust::CodeEditor);
    m_devEngines.insert(tabKey, engine);
    return true;
}

void CodeEditorWidget::executeDev(const QString& tabKey, const QString& code)
{
    if (!m_sm && m_adaptix)
        m_sm = m_adaptix->ScriptManager;

    if (!ensureDevEngine(tabKey)) {
        if (m_editor)
            m_editor->appendLog("Failed to create isolated AxScript engine (no ScriptManager?).\n", QColor(0xf44747));
        return;
    }

    auto* engine = m_devEngines.value(tabKey);
    if (!engine) {
        if (m_editor)
            m_editor->appendLog("Isolated engine missing for this tab.\n", QColor(0xf44747));
        return;
    }

    const bool ok = engine->execute(code);
    if (m_editor) {
        if (ok)
            m_editor->appendLog(QString("[%1] Script finished.\n").arg(QDateTime::currentDateTime().toString("hh:mm:ss")), QColor(0x6a9955));
        else
            m_editor->appendLog(QString("[%1] Script failed (see errors above).\n").arg(QDateTime::currentDateTime().toString("hh:mm:ss")), QColor(0xf44747));
    }
}

void CodeEditorWidget::removeDevEngine(const QString& tabKey)
{
    auto it = m_devEngines.find(tabKey);
    if (it != m_devEngines.end()) {
        delete it.value();
        m_devEngines.erase(it);
    }
}

void CodeEditorWidget::executeInMain(const QString& code)
{
    if (!m_sm && m_adaptix)
        m_sm = m_adaptix->ScriptManager;
    if (!m_sm) {
        if (m_editor)
            m_editor->appendLog("No ScriptManager — cannot run on main engine.\n", QColor(0xf44747));
        return;
    }

    QJSEngine* mainEngine = m_sm->MainScriptEngine();
    if (!mainEngine) {
        if (m_editor)
            m_editor->appendLog("Main AxScript engine is not available.\n", QColor(0xf44747));
        return;
    }

    QJSValue result = mainEngine->evaluate(code);
    if (result.isError()) {
        m_sm->consolePrintError(result.toString());
        if (m_editor)
            m_editor->appendLog(QString("[%1] Script failed (see errors above).\n").arg(QDateTime::currentDateTime().toString("hh:mm:ss")), QColor(0xf44747));
    } else {
        if (!result.isUndefined()) {
            QString msg = result.toString();
            if (!msg.isEmpty())
                m_sm->consolePrintMessage(msg);
        }
        if (m_editor)
            m_editor->appendLog(QString("[%1] Script finished.\n").arg(QDateTime::currentDateTime().toString("hh:mm:ss")), QColor(0x6a9955));
    }
}
