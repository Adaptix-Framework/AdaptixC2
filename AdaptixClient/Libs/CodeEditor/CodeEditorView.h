#pragma once

#include <QWidget>

class EditorTabWidget;
class CodeEditor;
class FileBrowser;
class BuildPanel;
class FindReplace;
class SyntaxStyle;
class QPlainTextEdit;
class QSplitter;
class QToolBar;

namespace oclero::qlementine { class Switch; }

class CodeEditorView : public QWidget
{
Q_OBJECT
    void buildLayout();
    void buildToolBar();

    QToolBar* m_toolBar;
    QSplitter* m_mainSplitter;
    QSplitter* m_editorSplitter;
    QSplitter* m_bottomSplitter;
    FileBrowser* m_fileBrowser;
    EditorTabWidget* m_tabWidget;
    BuildPanel* m_buildPanel;
    FindReplace* m_findReplace;
    QPlainTextEdit* m_logPanel;
    oclero::qlementine::Switch* m_sidebarSwitch;
    oclero::qlementine::Switch* m_logSwitch;
    oclero::qlementine::Switch* m_minimapSwitch;
    oclero::qlementine::Switch* m_wrapSwitch;

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

    void fitBuildPanel();

public Q_SLOTS:
    CodeEditor* loadFile(const QString& filePath);
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
