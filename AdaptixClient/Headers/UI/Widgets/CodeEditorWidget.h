#ifndef ADAPTIXCLIENT_CODEEDITORWIDGET_H
#define ADAPTIXCLIENT_CODEEDITORWIDGET_H

#include <UI/Widgets/AbstractDock.h>
#include <Client/CodeEditorProfileManager.h>

#include <QHash>
#include <QVariant>
#include <QVariantMap>
#include <QPointer>
#include <QJSValue>
#include <QColor>

class CodeEditorView;
class CodeEditor;
class QProcess;
class QAction;
class QComboBox;
class AxScriptManager;
class AxScriptEngine;
class AxContainerWrapper;
class AdaptixWidget;
class Agent;

class CodeEditorActionApi : public QObject
{
Q_OBJECT
    class CodeEditorWidget* m_host = nullptr;
public:
    explicit CodeEditorActionApi(CodeEditorWidget* host, QObject* parent = nullptr);
public Q_SLOTS:
    void save();

    QString file() const;
    QString content() const;
    QString agent_id() const;

    QVariant get_panel_data(const QString& key = QString()) const;

    QString expand(const QString& tmpl) const;
    void log(const QString& text);

    bool eval(const QString& code, const QJSValue& opts = QJSValue());

    QString job_start(const QString& cmd, const QJSValue& opts = QJSValue());
    bool job_stop(const QString& id = QString());
    bool job_running(const QString& id = QString()) const;
    void job_stop_all();
};

class CodeEditorWidget : public DockTab
{
Q_OBJECT
    friend class CodeEditorActionApi;

    CodeEditorView* m_editor = nullptr;

    QHash<QString, QProcess*> m_jobs;
    QString m_lastJobId;
    int     m_jobSeq = 0;

    CodeEditorProfileManager* m_profiles = nullptr;
    QComboBox* m_profileCombo        = nullptr;
    QAction*   m_profileComboAction  = nullptr;
    QList<QAction*> m_profileActions;
    QAction* m_actPrimary = nullptr;
    QAction* m_actStop    = nullptr;

    AdaptixWidget*   m_adaptix = nullptr;
    Agent*           m_agent   = nullptr;
    qint64           m_agentId = 0;
    int              m_instanceId = 0;
    QString          m_sessionProfileId;
    bool             m_restrictProfiles = false;
    QStringList      m_allowedProfiles;

    AxScriptManager* m_sm = nullptr;
    QHash<QString, AxScriptEngine*> m_devEngines;

    AxScriptEngine* m_panelEngine = nullptr;
    QWidget*        m_panelWidget = nullptr;
    AxContainerWrapper* m_panelContainer = nullptr;
    bool            m_applyingProfile = false;

    static QList<QPointer<CodeEditorWidget>> s_instances;
    static int s_nextInstanceId;

    void createUI();
    void setupConnections();
    void updateActionAvailability(const BuildProfile& p);
    void rebuildProfileToolbarActions(const BuildProfile& p);
    void rebuildAxScriptPanel(const BuildProfile& p);
    void clearAxScriptPanel();
    void runProfileActionScript(const QString& script);
    void updateActionEnabled();
    void captureAxScriptPanelToProfile();
    void applySessionProfile();
    void setSessionProfile(const QString& idOrName, bool persistGlobal = false);
    const BuildProfile* sessionProfile() const;
    QList<BuildProfile> sessionVisibleProfiles() const;
    QString workDirectoryForCurrentEditor() const;
    static QString expandDefines(const QString& defines);
    static QString jsQuote(const QString& s);
    static QString expandCommandTemplate(const QString& tmpl, const QString& filePath, const QString& defines);

    QString startJob(const QString& cmd, const QString& jobId, const QString& cwd);
    bool stopJob(const QString& jobId);
    bool isJobRunning(const QString& jobId) const;
    void stopAllJobs();
    void wireJobProcess(const QString& jobId, QProcess* proc);
    void appendJobLog(const QString& jobId, const QString& text, const QColor& color);

    QVariant panelFieldValue(const QString& id) const;
    QVariantMap panelStateMap() const;
    QString panelDefinesRaw() const;

    void refreshProfileCombo();

    bool ensureDevEngine(const QString& tabKey);
    void executeDev(const QString& tabKey, const QString& code);
    void removeDevEngine(const QString& tabKey);
    void executeInMain(const QString& code);
    bool evalScript(const QString& code, bool useMain);

private Q_SLOTS:
    void onProfileChanged(int index);
    void onScriptConsoleError(const QString& message);

public:
    static QString defaultPanelScriptTemplate();
    static void postLog(const QString& text, const QColor& color = QColor(0xd4d4d4));
    static CodeEditorWidget* activeInstance();

    explicit CodeEditorWidget(AdaptixWidget* w, Agent* agent = nullptr);
    ~CodeEditorWidget() override;

    CodeEditor* currentEditor() const;

    qint64 agentId() const { return m_agentId; }
    bool isAgentBound() const { return m_agentId != 0; }
    void clearAgent() { m_agent = nullptr; }

    void openScript(const QString& filePath);
    void openScriptContent(const QString& fileName, const QString& content, const QString& documentKey = QString());

    void selectProfile(const QString& idOrName);
    void applyOpenOptions(const CodeEditorOpenOptions& opts);

    void connectConsoleSignals(AxScriptManager* sm);
    void appendToLog(const QString& text, const QColor& color = QColor(0xd4d4d4));

public Q_SLOTS:
    void applyTheme();
};

#endif
