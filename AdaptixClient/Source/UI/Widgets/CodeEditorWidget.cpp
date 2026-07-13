#include <UI/Widgets/CodeEditorWidget.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <UI/Widgets/DockWidgetRegister.h>
#include <Client/AuthProfile.h>
#include <Client/Settings.h>
#include <Client/Extender.h>
#include <Client/CodeEditorProfileManager.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxScriptEngine.h>
#include <MainAdaptix.h>

#include <CodeEditorView.h>
#include <EditorTabWidget.h>
#include <CodeEditor.h>
#include <BuildPanel.h>
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

static QAction* addToolBarAction(QToolBar* tb, const QIcon& icon, const QString& label)
{
    auto* a = tb->addAction(icon, QString());
    a->setToolTip(label);
    a->setData(label);
    return a;
}

CodeEditorWidget::CodeEditorWidget(AdaptixWidget* w) : DockTab("Code Editor", w->GetProfile()->GetProject(), ":/icons/code")
{
    m_buildProcess = new QProcess(this);
    m_runProcess   = new QProcess(this);
    m_profiles     = new CodeEditorProfileManager(this);
    m_profiles->load();

    createUI();
    setupConnections();
    refreshProfileCombo();

    this->dockWidget->setWidget(this);
}

CodeEditorWidget::~CodeEditorWidget() = default;

void CodeEditorWidget::createUI()
{
    m_editor = new CodeEditorView(this);

    auto* tb = m_editor->toolBar();

    m_profileCombo = new QComboBox(tb);
    m_profileCombo->setMinimumWidth(140);
    m_profileCombo->setToolTip("Build profile");
    tb->addWidget(m_profileCombo);

    m_newProfileAction    = addToolBarAction(tb, QIcon(":/icons/plus"),  "New Profile");
    m_deleteProfileAction = addToolBarAction(tb, QIcon(":/icons/delete"), "Delete Profile");

    tb->addSeparator();

    m_buildAction = addToolBarAction(tb, QIcon(":/icons/build"), "Build");

    m_runAction = addToolBarAction(tb, QIcon(":/icons/start"), "Run");
    m_runAction->setEnabled(false);

    m_stopAction = addToolBarAction(tb, QIcon(":/icons/stop"), "Stop");
    m_stopAction->setEnabled(false);

    tb->addSeparator();

    m_loadScriptAction = addToolBarAction(tb, QIcon(":/icons/upload"), "Load to Scripts");
    m_loadScriptAction->setEnabled(false);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_editor);
}

void CodeEditorWidget::setupConnections()
{
    connect(m_buildProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        m_editor->appendLog(QString::fromLocal8Bit(m_buildProcess->readAllStandardOutput()));
    });
    connect(m_buildProcess, &QProcess::readyReadStandardError, this, [this]() {
        m_editor->appendLog(QString::fromLocal8Bit(m_buildProcess->readAllStandardError()), QColor(0xf44747));
    });
    connect(m_buildProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        if (code == 0 && status == QProcess::NormalExit) {
            m_editor->appendLog("Build succeeded.\n", QColor(0x6a9955));
            m_runAction->setEnabled(true);
        } else {
            m_editor->appendLog(QString("Build failed (exit code %1).\n").arg(code), QColor(0xf44747));
        }
        m_buildAction->setEnabled(true);
        m_stopAction->setEnabled(false);
    });
    connect(m_runProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        m_editor->appendLog(QString::fromLocal8Bit(m_runProcess->readAllStandardOutput()));
    });
    connect(m_runProcess, &QProcess::readyReadStandardError, this, [this]() {
        m_editor->appendLog(QString::fromLocal8Bit(m_runProcess->readAllStandardError()), QColor(0xf44747));
    });
    connect(m_runProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [this](int code, QProcess::ExitStatus) {
        m_editor->appendLog(QString("\nProcess finished (exit code %1).\n").arg(code), code == 0 ? QColor(0x6a9955) : QColor(0xf44747));
        m_runAction->setEnabled(true);
        m_stopAction->setEnabled(false);
    });

    if (auto* bp = m_editor->buildPanel())
        connect(bp, &BuildPanel::configurationChanged, this, &CodeEditorWidget::onBuildPanelChanged);

    connect(m_profiles, &CodeEditorProfileManager::profilesChanged, this, [this]() { refreshProfileCombo(); });
    connect(m_profiles, &CodeEditorProfileManager::currentChanged, this, [this](const BuildProfile& p) {
        if (auto* bp = m_editor->buildPanel()) {
            bp->setBuildCommand(p.buildCommand);
            bp->setRunCommand(p.runCommand);
            bp->setDefines(p.defines);
            bp->setMainEngineChecked(p.mainEngine);
            QVector<BuildPanel::Param> bpParams;
            bpParams.reserve(p.params.size());
            for (const auto& pp : p.params)
                bpParams.append({pp.name, pp.type, pp.value});
            bp->setParameters(bpParams);
        }
        updateActionAvailability(p);
        if (p.profileType() == ProfileCustom) {
            m_runAction->setEnabled(false);
            m_stopAction->setEnabled(false);
        }
    });

    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CodeEditorWidget::onProfileChanged);

    connect(m_newProfileAction,    &QAction::triggered, this, &CodeEditorWidget::onNewProfile);
    connect(m_deleteProfileAction, &QAction::triggered, this, &CodeEditorWidget::onDeleteProfile);
    connect(m_buildAction,         &QAction::triggered, this, &CodeEditorWidget::runBuild);
    connect(m_runAction,           &QAction::triggered, this, &CodeEditorWidget::runRun);
    connect(m_stopAction,          &QAction::triggered, this, &CodeEditorWidget::stopProcess);

    connect(m_loadScriptAction, &QAction::triggered, this, [this]() {
        auto* ed = m_editor ? m_editor->currentEditor() : nullptr;
        if (!ed || ed->filePath().isEmpty())
            return;

        if (GlobalClient && GlobalClient->extender) {
            if (GlobalClient->extender->IsLoaded(ed->filePath())) {
                m_editor->appendLog(QString("[%1] Script already loaded: %2\n").arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(ed->filePath()), QColor(0xd29922));
                return;
            }
            m_editor->saveFile();
            GlobalClient->extender->LoadFromFile(ed->filePath(), true);
            m_editor->appendLog(QString("[%1] Loaded to Scripts: %2\n").arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(ed->filePath()), QColor(0x6a9955));
        }
    });
    connect(m_editor, &CodeEditorView::currentEditorChanged, this, [this](CodeEditor* ed) {
        if (m_loadScriptAction)
            m_loadScriptAction->setEnabled(ed && ed->filePath().endsWith(".axs", Qt::CaseInsensitive));
    });

    connect(m_editor, &CodeEditorView::fileOpened, this, [this](const QString& filePath) {
        const QString suggested = BuildProfile::profileNameForFile(filePath);
        if (!suggested.isEmpty())
            m_profiles->setCurrent(suggested);
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

void CodeEditorWidget::connectConsoleSignals(AxScriptManager* sm)
{
    if (!sm)
        return;

    m_sm = sm;

    connect(sm, &AxScriptManager::consoleMessage, this, [this](const QString& msg) {
        if (m_editor)
            m_editor->appendLog(msg + "\n");
    });
    connect(sm, &AxScriptManager::consoleError, this, [this](const QString& msg) {
        if (m_editor)
            m_editor->appendLog(msg + "\n", QColor(0xf44747));
    });

    applyTheme();
}

CodeEditor* CodeEditorWidget::currentEditor() const
{
    return m_editor ? m_editor->currentEditor() : nullptr;
}

CodeEditor* CodeEditorWidget::loadFile(const QString& filePath)
{
    if (!m_editor)
        return nullptr;
    return m_editor->loadFile(filePath);
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

void CodeEditorWidget::refreshProfileCombo()
{
    if (!m_profileCombo || !m_profiles)
        return;

    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();
    const QStringList names = m_profiles->profileNames();
    for (const auto& n : names)
        m_profileCombo->addItem(n);
    const QString cur = m_profiles->currentName();
    const int idx = m_profileCombo->findText(cur);
    if (idx >= 0)
        m_profileCombo->setCurrentIndex(idx);
    m_profileCombo->blockSignals(false);

    if (const BuildProfile* p = m_profiles->current()) {
        if (auto* bp = m_editor->buildPanel()) {
            bp->setBuildCommand(p->buildCommand);
            bp->setRunCommand(p->runCommand);
            bp->setDefines(p->defines);
            bp->setMainEngineChecked(p->mainEngine);
            {
                QVector<BuildPanel::Param> bpParams;
                bpParams.reserve(p->params.size());
                for (const auto& pp : p->params)
                    bpParams.append({pp.name, pp.type, pp.value});
                bp->setParameters(bpParams);
            }
        }
        updateActionAvailability(*p);
    }
}

void CodeEditorWidget::captureBuildPanelToProfile()
{
    if (!m_editor || !m_profiles)
        return;

    auto* bp = m_editor->buildPanel();
    if (!bp)
        return;

    const BuildProfile* cur = m_profiles->current();
    if (!cur)
        return;

    BuildProfile p = *cur;
    p.buildCommand = bp->buildCommand();
    p.runCommand   = bp->runCommand();
    p.defines      = bp->defines();
    p.mainEngine   = bp->useMainEngine();
    const auto bpParams = bp->parameters();
    p.params.clear();
    p.params.reserve(bpParams.size());
    for (const auto& pp : bpParams)
        p.params.append({pp.name, pp.type, pp.value});
    m_profiles->updateCurrent(p);
}

void CodeEditorWidget::updateActionAvailability(const BuildProfile& p)
{
    if (!m_buildAction || !m_runAction || !m_stopAction)
        return;

    if (m_deleteProfileAction)
        m_deleteProfileAction->setEnabled(p.profileType() == ProfileCustom);

    auto* bp = m_editor ? m_editor->buildPanel() : nullptr;

    if (p.profileType() == ProfileCustom) {
        m_buildAction->setVisible(true);
        m_buildAction->setEnabled(true);
        m_runAction->setVisible(true);
        m_runAction->setEnabled(false);
        m_stopAction->setVisible(true);
        m_stopAction->setEnabled(false);
        if (bp) {
            bp->setBuildRowVisible(true);
            bp->setRunRowVisible(true);
            bp->setDefinesRowVisible(true);
            bp->setParamsVisible(true);
            bp->setMainEngineVisible(false);
        }
        if (m_editor)
            m_editor->fitBuildPanel();
        return;
    }

    if (p.profileType() == ProfileAxScript) {
        m_buildAction->setVisible(false);
        m_runAction->setVisible(true);
        m_runAction->setEnabled(true);
        m_stopAction->setVisible(true);
        m_stopAction->setEnabled(false);
        if (bp) {
            bp->setBuildRowVisible(false);
            bp->setRunRowVisible(false);
            bp->setDefinesRowVisible(false);
            bp->setParamsVisible(false);
            bp->setMainEngineVisible(true);
        }
        if (m_editor)
            m_editor->fitBuildPanel();
        return;
    }

    if (p.profileType() == ProfileBOF) {
        m_buildAction->setVisible(true);
        m_buildAction->setEnabled(true);
        m_runAction->setVisible(false);
        m_stopAction->setVisible(false);
        if (bp) {
            bp->setBuildRowVisible(true);
            bp->setRunRowVisible(false);
            bp->setDefinesRowVisible(false);
            bp->setParamsVisible(false);
            bp->setMainEngineVisible(false);
        }
        if (m_editor)
            m_editor->fitBuildPanel();
        return;
    }
}

void CodeEditorWidget::onProfileChanged(int index)
{
    if (index < 0 || !m_profileCombo)
        return;

    const QString name = m_profileCombo->itemText(index);
    m_profiles->setCurrent(name);
}

void CodeEditorWidget::onNewProfile()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, "New Profile", "Profile name:", QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    m_profiles->addProfile(name.trimmed());
}

void CodeEditorWidget::onDeleteProfile()
{
    if (!m_profiles)
        return;

    const QString name = m_profiles->currentName();
    if (name.isEmpty())
        return;

    const auto reply = QMessageBox::question(this, "Delete Profile", QString("Delete profile '%1'?").arg(name), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    m_profiles->removeProfile(name);
}

void CodeEditorWidget::onBuildPanelChanged()
{
    captureBuildPanelToProfile();
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

void CodeEditorWidget::runBuild()
{
    m_editor->setLogPanelVisible(true);

    QString buildCmd = m_editor->buildPanel()->buildCommand();
    if (buildCmd.isEmpty()) {
        m_editor->appendLog("No build command configured. Set it in the Build panel.\n", QColor(0xfbc064));
        return;
    }

    m_editor->saveFile();

    const QString defines = expandDefines(m_editor->buildPanel()->defines());
    if (!defines.isEmpty())
        buildCmd = buildCmd + " " + defines;

    m_editor->appendLogSeparator();
    m_editor->appendLog(QString("[%1] Build: %2\n").arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(buildCmd), QColor(0x3daee9));

    m_buildAction->setEnabled(false);
    m_runAction->setEnabled(false);
    m_stopAction->setEnabled(true);

    const QString workDir = workDirectoryForCurrentEditor();
    if (!workDir.isEmpty())
        m_buildProcess->setWorkingDirectory(workDir);

    m_buildProcess->start("sh", {"-c", buildCmd});
}

void CodeEditorWidget::runRun()
{
    m_editor->setLogPanelVisible(true);

    const BuildProfile* prof = m_profiles ? m_profiles->current() : nullptr;
    if (prof && prof->profileType() == ProfileAxScript) {
        auto* ed = m_editor ? m_editor->currentEditor() : nullptr;
        if (!ed) {
            m_editor->appendLog("No active editor.\n", QColor(0xfbc064));
            return;
        }
        const QString code = ed->toPlainText();
        const QString tabKey = ed->filePath().isEmpty() ? QStringLiteral("untitled") : ed->filePath();

        m_editor->appendLogSeparator();
        m_editor->appendLog(QString("[%1] Run AxScript (%2)\n").arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(m_editor->buildPanel()->useMainEngine() ? "Main engine" : "isolated engine"), QColor(0x3daee9));

        if (m_editor->buildPanel()->useMainEngine())
            executeInMain(code);
        else
            executeDev(tabKey, code);
        return;
    }

    QString runCmd = m_editor->buildPanel()->formattedRunCommand();
    if (runCmd.isEmpty()) {
        m_editor->appendLog("No run command configured. Set it in the Build panel.\n", QColor(0xfbc064));
        return;
    }

    m_editor->appendLogSeparator();
    m_editor->appendLog(QString("[%1] Run: %2\n").arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(runCmd), QColor(0x3daee9));

    m_runAction->setEnabled(false);
    m_stopAction->setEnabled(true);

    const QString workDir = workDirectoryForCurrentEditor();
    if (!workDir.isEmpty())
        m_runProcess->setWorkingDirectory(workDir);

    m_runProcess->start("sh", {"-c", runCmd});
}

void CodeEditorWidget::stopProcess()
{
    if (m_buildProcess->state() != QProcess::NotRunning) {
        m_editor->appendLog("Stopping build...\n", QColor(0xfbc064));
        m_buildProcess->kill();
    }
    if (m_runProcess->state() != QProcess::NotRunning) {
        m_editor->appendLog("Stopping process...\n", QColor(0xfbc064));
        m_runProcess->kill();
    }
    m_stopAction->setEnabled(false);
}

bool CodeEditorWidget::ensureDevEngine(const QString& tabKey)
{
    if (!m_sm)
        return false;

    if (m_devEngines.contains(tabKey))
        return true;

    auto* engine = new AxScriptEngine(m_sm, tabKey, this);
    m_devEngines.insert(tabKey, engine);
    return true;
}

void CodeEditorWidget::executeDev(const QString& tabKey, const QString& code)
{
    if (!ensureDevEngine(tabKey))
        return;

    auto* engine = m_devEngines.value(tabKey);
    if (!engine)
        return;

    engine->execute(code);
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
    if (!m_sm)
        return;

    QJSEngine* mainEngine = m_sm->MainScriptEngine();
    if (!mainEngine)
        return;

    QJSValue result = mainEngine->evaluate(code);
    if (result.isError())
        m_sm->consolePrintError(result.toString());
    else if (!result.isUndefined()) {
        QString msg = result.toString();
        if (!msg.isEmpty())
            m_sm->consolePrintMessage(msg);
    }
}
