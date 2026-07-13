#include <CodeEditorView.h>

#include <CodeEditor.h>
#include <EditorTabWidget.h>
#include <SyntaxStyle.h>
#include <FileBrowser.h>
#include <BuildPanel.h>
#include <FindReplace.h>
#include <AxScriptHighlighter.h>
#include <AxScriptCompleter.h>
#include <CXXHighlighter.h>

#include <oclero/qlementine/widgets/Switch.hpp>

#include <QSplitter>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QKeySequence>
#include <QStyle>
#include <QPlainTextEdit>
#include <QFontDatabase>
#include <QVBoxLayout>
#include <QLabel>
#include <QDir>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QFileInfo>

CodeEditorView::CodeEditorView(QWidget* parent) :
    QWidget(parent),
    m_toolBar(nullptr),
    m_mainSplitter(new QSplitter(Qt::Vertical, this)),
    m_editorSplitter(new QSplitter(Qt::Horizontal, m_mainSplitter)),
    m_bottomSplitter(new QSplitter(Qt::Vertical, m_mainSplitter)),
    m_fileBrowser(new FileBrowser(m_editorSplitter)),
    m_tabWidget(new EditorTabWidget(m_editorSplitter)),
    m_buildPanel(new BuildPanel(m_bottomSplitter)),
    m_findReplace(nullptr),
    m_logPanel(new QPlainTextEdit(m_bottomSplitter)),
    m_sidebarSwitch(new oclero::qlementine::Switch(this)),
    m_logSwitch(new oclero::qlementine::Switch(this)),
    m_minimapSwitch(new oclero::qlementine::Switch(this)),
    m_wrapSwitch(new oclero::qlementine::Switch(this)),
    m_projectPath()
{
    buildLayout();
    buildToolBar();

    m_tabWidget->newTab("Untitled");

    m_tabWidget->setFocus();

    connect(m_fileBrowser, &FileBrowser::fileSelected,   this, &CodeEditorView::loadFile);
    connect(m_tabWidget,   &EditorTabWidget::fileOpened, this, &CodeEditorView::fileOpened);
    connect(m_tabWidget,   &EditorTabWidget::fileSaved,  this, &CodeEditorView::fileSaved);
    connect(m_tabWidget, &EditorTabWidget::currentTabChanged, this, [this](int) {
        auto editor = m_tabWidget->currentEditor();
        m_findReplace->setEditor(editor);
        Q_EMIT currentEditorChanged(editor);
    });

    m_fileBrowser->setVisible(true);
    m_bottomSplitter->setVisible(false);
}

void CodeEditorView::buildLayout()
{
    m_editorSplitter->addWidget(m_fileBrowser);
    m_editorSplitter->addWidget(m_tabWidget);
    m_editorSplitter->setStretchFactor(0, 0);
    m_editorSplitter->setStretchFactor(1, 1);
    m_editorSplitter->setSizes({250, 750});

    m_bottomSplitter->addWidget(m_buildPanel);
    m_bottomSplitter->addWidget(m_logPanel);
    m_bottomSplitter->setStretchFactor(0, 0);
    m_bottomSplitter->setStretchFactor(1, 1);
    m_bottomSplitter->setSizes({120, 200});

    m_mainSplitter->addWidget(m_editorSplitter);
    m_mainSplitter->addWidget(m_bottomSplitter);
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setSizes({600, 320});

    m_findReplace = new FindReplace(this);
    m_findReplace->setVisible(false);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_toolBar = new QToolBar(this));
    m_toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    root->addWidget(m_findReplace);
    root->addWidget(m_mainSplitter);

    m_toolBar->setMovable(false);
    m_toolBar->setIconSize(QSize(18, 18));

    m_logPanel->setReadOnly(true);
    m_logPanel->setPlaceholderText("Build and run output will appear here...");
    m_logPanel->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    auto* findAction = new QAction("Find", this);
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &CodeEditorView::showFind);
    addAction(findAction);

    auto* replaceAction = new QAction("Replace", this);
    replaceAction->setShortcut(QKeySequence::Replace);
    connect(replaceAction, &QAction::triggered, this, &CodeEditorView::showReplace);
    addAction(replaceAction);
}

void CodeEditorView::buildToolBar()
{
    auto addAction = [this](const QString& iconPath, const QString& label) {
        auto* a = m_toolBar->addAction(QIcon(iconPath), QString());
        a->setToolTip(label);
        a->setData(label);
        return a;
    };

    auto* newAction = addAction(":/icons/new_file", "New File");
    connect(newAction, &QAction::triggered, this, &CodeEditorView::newFile);

    auto* openAction = addAction(":/icons/file_open", "Open File");
    connect(openAction, &QAction::triggered, this, &CodeEditorView::openFile);

    auto* openFolderAction = addAction(":/icons/open_folder", "Open Directory");
    connect(openFolderAction, &QAction::triggered, this, &CodeEditorView::openFolder);

    m_toolBar->addSeparator();

    auto* saveAction = addAction(":/icons/save_as", "Save");
    connect(saveAction, &QAction::triggered, this, &CodeEditorView::saveFile);

    m_toolBar->addSeparator();

    auto makeLabel = [this](const QString& text) {
        auto* lbl = new QLabel(text, this);
        lbl->setForegroundRole(QPalette::WindowText);
        return lbl;
    };

    m_toolBar->addWidget(makeLabel(" Explorer "));
    m_toolBar->addWidget(m_sidebarSwitch);
    m_sidebarSwitch->setChecked(true);
    connect(m_sidebarSwitch, &oclero::qlementine::Switch::clicked, this, [this]() { setSidebarVisible(m_sidebarSwitch->isChecked()); });

    m_toolBar->addSeparator();

    m_toolBar->addWidget(makeLabel(" Build "));
    m_toolBar->addWidget(m_logSwitch);
    m_logSwitch->setChecked(false);
    connect(m_logSwitch, &oclero::qlementine::Switch::clicked, this, [this]() { setLogPanelVisible(m_logSwitch->isChecked()); });

    m_toolBar->addSeparator();

    m_toolBar->addWidget(makeLabel(" Map "));
    m_toolBar->addWidget(m_minimapSwitch);
    m_minimapSwitch->setChecked(false);
    connect(m_minimapSwitch, &oclero::qlementine::Switch::clicked, this, [this]() { setMinimapEnabled(m_minimapSwitch->isChecked()); });

    m_toolBar->addSeparator();

    m_toolBar->addWidget(makeLabel(" Wrap "));
    m_toolBar->addWidget(m_wrapSwitch);
    m_wrapSwitch->setChecked(false);
    connect(m_wrapSwitch, &oclero::qlementine::Switch::clicked, this, [this]() { setWrapEnabled(m_wrapSwitch->isChecked()); });

    auto* spring = new QWidget(this);
    spring->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolBar->addWidget(spring);
}

void CodeEditorView::setSyntaxStyle(SyntaxStyle* style)
{
    m_tabWidget->setSyntaxStyle(style);
}

SyntaxStyle* CodeEditorView::syntaxStyle() const
{
    return m_tabWidget->syntaxStyle();
}

void CodeEditorView::setProjectPath(const QString& path)
{
    if (m_projectPath == path)
        return;

    m_projectPath = path;
    Q_EMIT projectPathChanged(path);
}

QString CodeEditorView::projectPath() const
{
    return m_projectPath;
}

EditorTabWidget* CodeEditorView::tabWidget() const { return m_tabWidget; }
CodeEditor* CodeEditorView::currentEditor() const { return m_tabWidget->currentEditor(); }
FileBrowser* CodeEditorView::fileBrowser() const { return m_fileBrowser; }
BuildPanel* CodeEditorView::buildPanel() const { return m_buildPanel; }
QPlainTextEdit* CodeEditorView::logPanel() const { return m_logPanel; }
QToolBar* CodeEditorView::toolBar() const { return m_toolBar; }
FindReplace* CodeEditorView::findReplace() const { return m_findReplace; }

bool CodeEditorView::isSidebarVisible() const { return m_fileBrowser->isVisible(); }
bool CodeEditorView::isLogPanelVisible() const { return m_bottomSplitter->isVisible(); }

bool CodeEditorView::isMinimapEnabled() const
{
    auto editor = currentEditor();
    return editor ? editor->isMinimapEnabled() : false;
}

CodeEditor* CodeEditorView::loadFile(const QString& filePath)
{
    auto editor = m_tabWidget->openFile(filePath);
    if (!editor)
        QMessageBox::warning(this, "Error", QString("Cannot open file: %1").arg(filePath));
    return editor;
}

void CodeEditorView::newFile()
{
    bool ok = false;
    const QString name = QInputDialog::getText( this, tr("New File"), tr("File name (extension determines syntax/profile):"), QLineEdit::Normal, QStringLiteral("untitled.axs"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    const QString trimmed = name.trimmed();
    auto* editor = m_tabWidget->newTab(trimmed);

    editor->setFilePath(trimmed);

    const QString suffix = QFileInfo(trimmed).suffix().toLower();
    if (suffix == "axs" || suffix == "js" || suffix == "javascript" || suffix == "mjs") {
        auto* oldHl = editor->findChild<CXXHighlighter*>();
        if (oldHl) {
            oldHl->setDocument(nullptr);
            oldHl->deleteLater();
        }
        auto* axHl = new AxScriptHighlighter(editor->document());
        axHl->setFilePath(trimmed);
        if (editor->syntaxStyle())
            axHl->setSyntaxStyle(editor->syntaxStyle());
        editor->setHighlighter(axHl);

        auto* oldCompleter = editor->completer();
        if (oldCompleter)
            oldCompleter->deleteLater();
        editor->setCompleter(new AxScriptCompleter(editor));
    }
    if (editor->highlighter())
        editor->highlighter()->rehighlight();

    Q_EMIT fileOpened(trimmed);
}

void CodeEditorView::openFile()
{
    QFileDialog dialog(this, "Open File", m_projectPath, "C/C++ Files (*.c *.cpp *.h *.hpp *.cc *.hh);;All Files (*)");
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);

    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty())
        loadFile(dialog.selectedFiles().first());
}

void CodeEditorView::openFolder()
{
    QFileDialog dialog(this, "Open Project Folder", m_projectPath);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly, true);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);

    if (dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty()) {
        auto dir = dialog.selectedFiles().first();
        setProjectPath(dir);
        m_fileBrowser->setRootPath(dir);
        setSidebarVisible(true);
    }
}

void CodeEditorView::saveFile()
{
    m_tabWidget->saveCurrentTab();
}

void CodeEditorView::saveFileAs()
{
    m_tabWidget->saveCurrentTabAs();
}

void CodeEditorView::saveAll()
{
    m_tabWidget->saveAllTabs();
}

void CodeEditorView::closeCurrentTab()
{
    auto index = m_tabWidget->currentIndex();
    if (index >= 0)
        m_tabWidget->closeTab(index);
}

void CodeEditorView::setSidebarVisible(bool visible)
{
    m_fileBrowser->setVisible(visible);
    m_sidebarSwitch->setChecked(visible);
}

void CodeEditorView::setLogPanelVisible(bool visible)
{
    m_bottomSplitter->setVisible(visible);
    m_logSwitch->setChecked(visible);
    Q_EMIT logPanelVisibilityChanged(visible);
}

void CodeEditorView::setMinimapEnabled(bool enabled)
{
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto* editor = m_tabWidget->editorAt(i);
        if (editor)
            editor->setMinimapEnabled(enabled);
    }
    m_minimapSwitch->setChecked(enabled);
}

bool CodeEditorView::isWrapEnabled() const
{
    auto editor = currentEditor();
    return editor ? (editor->lineWrapMode() != QTextEdit::NoWrap) : false;
}

void CodeEditorView::fitBuildPanel()
{
    if (!m_buildPanel || !m_bottomSplitter)
        return;
    int panelH = m_buildPanel->sizeHint().height();
    if (panelH < 10) panelH = 10;
    int logH = qMax(m_bottomSplitter->height() - panelH, 50);
    m_bottomSplitter->setSizes({panelH, logH});
}

void CodeEditorView::setWrapEnabled(bool enabled)
{
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto* editor = m_tabWidget->editorAt(i);
        if (editor)
            editor->setLineWrapMode(enabled ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
    }
    m_wrapSwitch->setChecked(enabled);
}

void CodeEditorView::showFind()
{
    m_findReplace->setEditor(currentEditor());
    m_findReplace->showFind();
}

void CodeEditorView::showReplace()
{
    m_findReplace->setEditor(currentEditor());
    m_findReplace->showReplace();
}

void CodeEditorView::appendLog(const QString& msg, const QColor& color)
{
    if (!m_bottomSplitter->isVisible())
    {
        m_bottomSplitter->setVisible(true);
        m_logSwitch->setChecked(true);
        Q_EMIT logPanelVisibilityChanged(true);
    }

    QTextCursor cursor(m_logPanel->document());
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat fmt;
    fmt.setForeground(color);
    cursor.setCharFormat(fmt);
    cursor.insertText(msg);

    m_logPanel->setTextCursor(cursor);
    m_logPanel->ensureCursorVisible();
}

void CodeEditorView::appendLogSeparator()
{
    appendLog(QString(60, QChar(0x2500)) + "\n", QColor(0x808080));
}
