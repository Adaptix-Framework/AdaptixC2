#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QStackedWidget>
#include <QVBoxLayout>

class EditorTabWidget;
class CodeEditor;
class FileBrowser;
class BuildPanel;
class FindReplace;
class SyntaxStyle;

class CodeEditorView : public QWidget
{
Q_OBJECT
    void buildLayout();
    void buildToolBar();
    void applyTypography();
    void updateBottomAreaVisibility();
    bool hasConfigPanel() const;

    QToolBar* m_toolBar;
    QSplitter* m_mainSplitter;
    QSplitter* m_editorSplitter;
    QSplitter* m_bottomSplitter;
    FileBrowser* m_fileBrowser;
    EditorTabWidget* m_tabWidget;
    BuildPanel* m_buildPanel;
    QStackedWidget* m_configStack = nullptr;
    QWidget* m_customPanelHost = nullptr;
    QVBoxLayout* m_customPanelLayout = nullptr;
    QWidget* m_activeCustomPanel = nullptr;
    FindReplace* m_findReplace;
    QPlainTextEdit* m_logPanel;
    QAction* m_newFileAction = nullptr;
    QAction* m_openFileAction = nullptr;
    QAction* m_openFolderAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_sidebarAction = nullptr;
    QAction* m_logAction = nullptr;
    QAction* m_minimapAction = nullptr;
    QAction* m_wrapAction = nullptr;
    QAction* m_fileSep1 = nullptr;
    QAction* m_fileSep2 = nullptr;

    QString m_projectPath;

public:
    explicit CodeEditorView(QWidget* parent = nullptr);

    void setSyntaxStyle(SyntaxStyle* style);
    SyntaxStyle* syntaxStyle() const;

    void setProjectPath(const QString& path);
    QString projectPath() const;

    EditorTabWidget* tabWidget() const;
    CodeEditor* currentEditor() const;
    FileBrowser* fileBrowser() const;
    BuildPanel* buildPanel() const;
    QPlainTextEdit* logPanel() const;
    QToolBar* toolBar() const;
    FindReplace* findReplace() const;

    bool isSidebarVisible() const;
    bool isLogPanelVisible() const;
    bool isMinimapEnabled() const;
    bool isWrapEnabled() const;

    void applyFileToolbarFlags(bool newFile, bool openFile, bool openFolder, bool save, bool explorer, bool buildLog, bool minimap, bool wordWrap);
    void applyLanguage(const QString& language);

    void applyConfigPanel(const QString& mode);
    void applyConfigPanel(QWidget* widget);

    void fitBuildPanel();

public Q_SLOTS:
    CodeEditor* loadFile(const QString& filePath);
    CodeEditor* openContent(const QString& fileName, const QString& content, const QString& documentKey = QString());
    void newFile();
    void openFile();
    void openFolder();
    void saveFile();
    void saveFileAs();
    void saveAll();
    void closeCurrentTab();

    void setSidebarVisible(bool visible);
    void setLogPanelVisible(bool visible);
    void setMinimapEnabled(bool enabled);
    void setWrapEnabled(bool enabled);

    void showFind();
    void showReplace();

    void appendLog(const QString& msg, const QColor& color = Qt::white);
    void appendLogSeparator();

Q_SIGNALS:
    void currentEditorChanged(CodeEditor* editor);
    void fileOpened(const QString& filePath);
    void fileSaved(const QString& filePath);
    void projectPathChanged(const QString& path);
    void logPanelVisibilityChanged(bool visible);
};
