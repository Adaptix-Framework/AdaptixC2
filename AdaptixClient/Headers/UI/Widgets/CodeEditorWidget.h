#ifndef ADAPTIXCLIENT_CODEEDITORWIDGET_H
#define ADAPTIXCLIENT_CODEEDITORWIDGET_H

#include <UI/Widgets/AbstractDock.h>
#include <Client/CodeEditorProfileManager.h>

#include <QHash>

class CodeEditorView;
class CodeEditor;
class QProcess;
class QAction;
class QComboBox;
class CodeEditorProfileManager;
class AxScriptManager;
class AxScriptEngine;
class AdaptixWidget;

class CodeEditorWidget : public DockTab
{
Q_OBJECT
    CodeEditorView* m_editor = nullptr;
    QProcess* m_buildProcess = nullptr;
    QProcess* m_runProcess   = nullptr;

    CodeEditorProfileManager* m_profiles = nullptr;
    QComboBox* m_profileCombo        = nullptr;
    QAction*   m_newProfileAction    = nullptr;
    QAction*   m_deleteProfileAction = nullptr;
    QAction*   m_buildAction         = nullptr;
    QAction*   m_runAction           = nullptr;
    QAction*   m_stopAction          = nullptr;
    QAction*   m_loadScriptAction    = nullptr;

    AxScriptManager* m_sm = nullptr;
    QHash<QString, AxScriptEngine*> m_devEngines;

    void createUI();
    void setupConnections();
    void updateActionAvailability(const BuildProfile& p);
    QString workDirectoryForCurrentEditor() const;
    static QString expandDefines(const QString& defines);

    void refreshProfileCombo();
    void captureBuildPanelToProfile();

    bool ensureDevEngine(const QString& tabKey);
    void executeDev(const QString& tabKey, const QString& code);
    void removeDevEngine(const QString& tabKey);
    void executeInMain(const QString& code);

private Q_SLOTS:
    void runBuild();
    void runRun();
    void stopProcess();
    void onProfileChanged(int index);
    void onNewProfile();
    void onDeleteProfile();
    void onBuildPanelChanged();

public:
    explicit CodeEditorWidget(AdaptixWidget* w);
    ~CodeEditorWidget() override;

    CodeEditorView* editor() const { return m_editor; }
    CodeEditor* currentEditor() const;

    CodeEditor* loadFile(const QString& filePath);

    void openScript(const QString& filePath);

    void connectConsoleSignals(AxScriptManager* sm);

public Q_SLOTS:
    void applyTheme();
};

#endif
