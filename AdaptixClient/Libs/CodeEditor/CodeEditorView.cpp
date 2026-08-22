#include <CodeEditorView.h>

#include <CodeEditor.h>
#include <EditorTabWidget.h>
#include <SyntaxStyle.h>
#include <StyleSyntaxHighlighter.h>
#include <FileBrowser.h>
#include <BuildPanel.h>
#include <FindReplace.h>
#include <AxScriptHighlighter.h>
#include <AxScriptCompleter.h>
#include <CXXHighlighter.h>
#include <CXXCompleter.h>
#include <Utils/FontManager.h>

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
#include <QDir>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QFileInfo>
#include <QTimer>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QWidget>
#include <QStackedWidget>
#include <QMenu>
#include <QAction>

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
    if (m_logPanel)
        m_logPanel->setVisible(false);
    if (m_configStack)
        m_configStack->setVisible(false);
}

void CodeEditorView::buildLayout()
{
    m_editorSplitter->addWidget(m_fileBrowser);
    m_editorSplitter->addWidget(m_tabWidget);
    m_editorSplitter->setStretchFactor(0, 0);
    m_editorSplitter->setStretchFactor(1, 1);
    m_editorSplitter->setSizes({250, 750});

    m_configStack = new QStackedWidget(m_bottomSplitter);
    m_configStack->addWidget(m_buildPanel);
    m_customPanelHost = new QWidget(m_configStack);
    m_customPanelLayout = new QVBoxLayout(m_customPanelHost);
    m_customPanelLayout->setContentsMargins(0, 0, 0, 0);
    m_customPanelLayout->setSpacing(0);
    m_configStack->addWidget(m_customPanelHost);
    m_configStack->setCurrentIndex(0);

    m_bottomSplitter->addWidget(m_configStack);
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
    applyTypography();

    m_logPanel->setReadOnly(true);
    m_logPanel->setPlaceholderText("Build and run output will appear here...");
    m_logPanel->setVisible(false);
    m_logPanel->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_logPanel, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (!m_logPanel)
            return;
        QMenu menu(m_logPanel);
        QAction* clearAct = menu.addAction(QStringLiteral("Clear log"));
        clearAct->setEnabled(!m_logPanel->document()->isEmpty());
        menu.addSeparator();
        QAction* copyAct = menu.addAction(QStringLiteral("Copy"));
        copyAct->setEnabled(m_logPanel->textCursor().hasSelection());
        QAction* selectAllAct = menu.addAction(QStringLiteral("Select All"));
        selectAllAct->setEnabled(!m_logPanel->document()->isEmpty());
        QAction* chosen = menu.exec(m_logPanel->mapToGlobal(pos));
        if (chosen == clearAct)
            m_logPanel->clear();
        else if (chosen == copyAct)
            m_logPanel->copy();
        else if (chosen == selectAllAct)
            m_logPanel->selectAll();
    });

    connect(&FontManager::instance(), &FontManager::typographyChanged, this, [this]() {
        applyTypography();
    });

    auto* findAction = new QAction("Find", this);
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &CodeEditorView::showFind);
    addAction(findAction);

    auto* replaceAction = new QAction("Replace", this);
    replaceAction->setShortcut(QKeySequence::Replace);
    connect(replaceAction, &QAction::triggered, this, &CodeEditorView::showReplace);
    addAction(replaceAction);
}

void CodeEditorView::applyTypography()
{
    const AppTypography& ty = FontManager::instance().typography();
    const qreal s = ty.baseSize / 10.0;
    const int icon = qMax(16, qRound(20 * s));
    if (m_toolBar) {
        m_toolBar->setIconSize(QSize(icon, icon));
        m_toolBar->setFixedHeight(qMax(ty.controlHeight + 12, icon + 20));
        m_toolBar->setObjectName(QStringLiteral("CodeEditorToolBar"));
        m_toolBar->setStyleSheet(QStringLiteral(
            "QToolBar#CodeEditorToolBar { spacing: 4px; padding: 4px 6px; border: none; }"
            "QToolBar#CodeEditorToolBar > QToolButton { padding: 4px; margin: 0 1px; }"
        ));
    }
    if (m_logPanel)
        m_logPanel->setFont(ty.mono);

    if (m_tabWidget) {
        for (int i = 0; i < m_tabWidget->count(); ++i) {
            if (auto* ed = m_tabWidget->editorAt(i)) {
                ed->setFont(ty.mono);
                ed->document()->setDefaultFont(ty.mono);
            }
        }
    }
}

void CodeEditorView::buildToolBar()
{
    auto addAction = [this](const QString& iconPath, const QString& label) {
        auto* a = m_toolBar->addAction(QIcon(iconPath), QString());
        a->setToolTip(label);
        a->setData(label);
        return a;
    };

    auto addToggle = [this](const QString& iconPath, const QString& tip, bool checked) {
        auto* a = m_toolBar->addAction(QIcon(iconPath), QString());
        a->setToolTip(tip);
        a->setCheckable(true);
        a->setChecked(checked);
        return a;
    };

    m_newFileAction = addAction(":/icons/new_file", "New File");
    connect(m_newFileAction, &QAction::triggered, this, &CodeEditorView::newFile);

    m_openFileAction = addAction(":/icons/file_open", "Open File");
    connect(m_openFileAction, &QAction::triggered, this, &CodeEditorView::openFile);

    m_openFolderAction = addAction(":/icons/open_folder", "Open Directory");
    connect(m_openFolderAction, &QAction::triggered, this, &CodeEditorView::openFolder);

    m_fileSep1 = m_toolBar->addSeparator();

    m_saveAction = addAction(":/icons/save_as", "Save");
    connect(m_saveAction, &QAction::triggered, this, &CodeEditorView::saveFile);

    m_fileSep2 = m_toolBar->addSeparator();

    m_sidebarAction = addToggle(":/icons/dock_left", "File Explorer", true);
    connect(m_sidebarAction, &QAction::toggled, this, [this](bool on) { setSidebarVisible(on); });

    m_logAction = addToggle(":/icons/dock_bottom", "Build / log panel", false);
    connect(m_logAction, &QAction::toggled, this, [this](bool on) { setLogPanelVisible(on); });

    m_minimapAction = addToggle(":/icons/dock_right", "Minimap", false);
    connect(m_minimapAction, &QAction::toggled, this, [this](bool on) { setMinimapEnabled(on); });

    m_wrapAction = addToggle(":/icons/wrap_text", "Word wrap", false);
    connect(m_wrapAction, &QAction::toggled, this, [this](bool on) { setWrapEnabled(on); });
}

void CodeEditorView::applyFileToolbarFlags(bool newFile, bool openFile, bool openFolder, bool save, bool explorer, bool buildLog, bool minimap, bool wordWrap)
{
    auto setAct = [](QAction* a, bool on) {
        if (a)
            a->setVisible(on);
    };
    setAct(m_newFileAction, newFile);
    setAct(m_openFileAction, openFile);
    setAct(m_openFolderAction, openFolder);
    setAct(m_saveAction, save);
    setAct(m_sidebarAction, explorer);
    setAct(m_logAction, buildLog);
    setAct(m_minimapAction, minimap);
    setAct(m_wrapAction, wordWrap);

    const bool anyFile = newFile || openFile || openFolder;
    if (m_fileSep1)
        m_fileSep1->setVisible(anyFile && save);
    if (m_fileSep2)
        m_fileSep2->setVisible(save || explorer || buildLog || minimap || wordWrap);

    if (!explorer)
        setSidebarVisible(false);
    if (!buildLog)
        setLogPanelVisible(false);
    if (!minimap)
        setMinimapEnabled(false);
}

void CodeEditorView::applyLanguage(const QString& language)
{
    CodeEditor* editor = currentEditor();
    if (!editor)
        return;

    if (StyleSyntaxHighlighter* cur = editor->highlighter()) {
        editor->setHighlighter(nullptr);
        cur->deleteLater();
    }
    const auto axOrphans = editor->findChildren<AxScriptHighlighter*>();
    for (AxScriptHighlighter* h : axOrphans) {
        h->setDocument(nullptr);
        h->deleteLater();
    }
    const auto cxOrphans = editor->findChildren<CXXHighlighter*>();
    for (CXXHighlighter* h : cxOrphans) {
        h->setDocument(nullptr);
        h->deleteLater();
    }

    if (editor->isLargeDocument()) {
        editor->setCodeFoldingEnabled(false);
        return;
    }

    const QString lang = language.toLower();
    const bool wantAx = (lang == QLatin1String("axscript") || lang == QLatin1String("js"));
    const bool wantCpp = (lang == QLatin1String("c") || lang == QLatin1String("cpp") || lang == QLatin1String("cxx") || lang.isEmpty());
    const bool wantPlain = (lang == QLatin1String("plain"));

    if (wantAx) {
        auto* ax = new AxScriptHighlighter(nullptr);
        ax->setParent(editor);
        if (!editor->filePath().isEmpty())
            ax->setFilePath(editor->filePath());
        if (editor->syntaxStyle())
            ax->setSyntaxStyle(editor->syntaxStyle());
        editor->setHighlighter(ax);
        if (auto* oldC = editor->completer())
            oldC->deleteLater();
        editor->setCompleter(new AxScriptCompleter(editor));
    } else if (wantCpp && !wantPlain) {
        auto* cx = new CXXHighlighter(nullptr);
        cx->setParent(editor);
        if (!editor->filePath().isEmpty())
            cx->setFilePath(editor->filePath());
        if (editor->syntaxStyle())
            cx->setSyntaxStyle(editor->syntaxStyle());
        editor->setHighlighter(cx);
        cx->notifyDocumentLoaded();
        if (auto* oldC = qobject_cast<AxScriptCompleter*>(editor->completer())) {
            oldC->deleteLater();
            editor->setCompleter(new CXXCompleter(editor));
        } else if (!editor->completer()) {
            editor->setCompleter(new CXXCompleter(editor));
        }
    } else if (wantPlain) {
        if (auto* oldC = qobject_cast<AxScriptCompleter*>(editor->completer())) {
            oldC->deleteLater();
            editor->setCompleter(new CXXCompleter(editor));
        }
    }
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

bool CodeEditorView::hasConfigPanel() const
{
    return m_configStack && !m_configStack->isHidden();
}

void CodeEditorView::updateBottomAreaVisibility()
{
    const bool show = hasConfigPanel() || isLogPanelVisible();
    if (m_bottomSplitter)
        m_bottomSplitter->setVisible(show);
    if (show)
        fitBuildPanel();
}
QToolBar* CodeEditorView::toolBar() const { return m_toolBar; }
FindReplace* CodeEditorView::findReplace() const { return m_findReplace; }

bool CodeEditorView::isSidebarVisible() const { return m_fileBrowser->isVisible(); }
bool CodeEditorView::isLogPanelVisible() const { return m_logPanel && !m_logPanel->isHidden(); }

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

CodeEditor* CodeEditorView::openContent(const QString& fileName, const QString& content, const QString& documentKey)
{
    QString title = fileName.trimmed();
    if (title.isEmpty())
        title = QStringLiteral("untitled.axs");
    if (!title.contains(QLatin1Char('.')))
        title += QStringLiteral(".axs");

    const QString key = documentKey.trimmed().isEmpty() ? title : documentKey.trimmed();

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto* existing = m_tabWidget->editorAt(i);
        if (!existing || existing->filePath() != key)
            continue;
        m_tabWidget->setCurrentIndex(i);
        if (!existing->isModified() && existing->toPlainText() != content) {
            existing->setPlainText(content);
            existing->markSaved();
            if (auto* cxx = existing->findChild<CXXHighlighter*>())
                cxx->notifyDocumentLoaded();
        }
        if (m_tabWidget->tabText(i) != title)
            m_tabWidget->setTabText(i, title);
        Q_EMIT currentEditorChanged(existing);
        return existing;
    }

    auto* editor = m_tabWidget->newTab(title);
    if (!editor)
        return nullptr;

    editor->setFilePath(key);

    const QString suffix = QFileInfo(title).suffix().toLower();
    if (suffix == "axs" || suffix == "js" || suffix == "javascript" || suffix == "mjs") {
        auto* oldHl = editor->findChild<CXXHighlighter*>();
        if (oldHl) {
            oldHl->setDocument(nullptr);
            oldHl->deleteLater();
        }
        auto* axHl = new AxScriptHighlighter(editor->document());
        axHl->setFilePath(key);
        if (editor->syntaxStyle())
            axHl->setSyntaxStyle(editor->syntaxStyle());
        editor->setHighlighter(axHl);

        auto* oldCompleter = editor->completer();
        if (oldCompleter)
            oldCompleter->deleteLater();
        editor->setCompleter(new AxScriptCompleter(editor));
    }

    editor->setPlainText(content);
    if (editor->document())
        editor->document()->setModified(true);
    if (auto* cxx = editor->findChild<CXXHighlighter*>())
        cxx->notifyDocumentLoaded();

    Q_EMIT fileOpened(key);
    Q_EMIT currentEditorChanged(editor);
    return editor;
}

void CodeEditorView::newFile()
{
    bool ok = false;
    const QString name = QInputDialog::getText( this, tr("New File"), tr("File name (extension determines syntax/profile):"), QLineEdit::Normal, QStringLiteral("untitled.axs"), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    openContent(name.trimmed(), QString());
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
    if (m_sidebarAction && m_sidebarAction->isChecked() != visible) {
        QSignalBlocker b(m_sidebarAction);
        m_sidebarAction->setChecked(visible);
    }
}

void CodeEditorView::setLogPanelVisible(bool visible)
{
    if (m_logPanel)
        m_logPanel->setVisible(visible);
    if (m_logAction && m_logAction->isChecked() != visible) {
        QSignalBlocker b(m_logAction);
        m_logAction->setChecked(visible);
    }
    updateBottomAreaVisibility();
    Q_EMIT logPanelVisibilityChanged(visible);
}

void CodeEditorView::setMinimapEnabled(bool enabled)
{
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto* editor = m_tabWidget->editorAt(i);
        if (editor)
            editor->setMinimapEnabled(enabled);
    }
    if (m_minimapAction && m_minimapAction->isChecked() != enabled) {
        QSignalBlocker b(m_minimapAction);
        m_minimapAction->setChecked(enabled);
    }
}

bool CodeEditorView::isWrapEnabled() const
{
    auto editor = currentEditor();
    return editor ? (editor->lineWrapMode() != QTextEdit::NoWrap) : false;
}

void CodeEditorView::fitBuildPanel()
{
    if (!m_bottomSplitter)
        return;

    const bool configOn = hasConfigPanel();
    const bool logOn = isLogPanelVisible();
    if (!configOn && !logOn)
        return;

    int panelH = 0;
    if (configOn) {
        QWidget* top = nullptr;
        if (m_activeCustomPanel && m_activeCustomPanel->isVisible())
            top = m_activeCustomPanel;
        else if (m_configStack)
            top = m_configStack->currentWidget();
        if (!top)
            top = m_buildPanel;

        panelH = 48;
        if (top) {
            top->updateGeometry();
            panelH = top->sizeHint().height();
            if (panelH <= 0)
                panelH = top->minimumSizeHint().height();
            if (panelH <= 0)
                panelH = top->heightForWidth(top->width() > 0 ? top->width() : 400);
        }
        if (panelH < 36) panelH = 40;
        if (panelH > 260) panelH = 260;

        if (m_configStack) {
            m_configStack->setMinimumHeight(0);
            m_configStack->setMaximumHeight(panelH + 8);
            m_configStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        }
        if (m_customPanelHost) {
            m_customPanelHost->setMinimumHeight(0);
            m_customPanelHost->setMaximumHeight(panelH + 8);
            m_customPanelHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        }
    }

    if (configOn && logOn) {
        const int logH = qMax(m_bottomSplitter->height() - panelH, 80);
        m_bottomSplitter->setSizes({panelH, logH});
    } else if (configOn) {
        m_bottomSplitter->setSizes({panelH, 0});
    } else {
        m_bottomSplitter->setSizes({0, qMax(120, m_bottomSplitter->height())});
    }
}

void CodeEditorView::applyConfigPanel(const QString& mode)
{
    const QString id = mode.trimmed().isEmpty() ? QStringLiteral("none") : mode.trimmed();

    if (m_activeCustomPanel && m_customPanelLayout) {
        m_customPanelLayout->removeWidget(m_activeCustomPanel);
        m_activeCustomPanel->setParent(nullptr);
        m_activeCustomPanel->hide();
        m_activeCustomPanel = nullptr;
    }

    if (id == QLatin1String("build")) {
        if (m_configStack) {
            m_configStack->setMaximumHeight(QWIDGETSIZE_MAX);
            m_configStack->setVisible(true);
            m_configStack->setCurrentIndex(0);
        }
        if (m_customPanelHost)
            m_customPanelHost->setMaximumHeight(QWIDGETSIZE_MAX);
        if (m_buildPanel)
            m_buildPanel->setVisible(true);
        updateBottomAreaVisibility();
        return;
    }

    if (m_configStack) {
        m_configStack->setMaximumHeight(QWIDGETSIZE_MAX);
        m_configStack->setVisible(false);
    }
    if (m_customPanelHost)
        m_customPanelHost->setMaximumHeight(QWIDGETSIZE_MAX);
    updateBottomAreaVisibility();
}

void CodeEditorView::applyConfigPanel(QWidget* widget)
{
    if (!widget) {
        applyConfigPanel(QStringLiteral("none"));
        return;
    }

    if (m_activeCustomPanel && m_customPanelLayout) {
        m_customPanelLayout->removeWidget(m_activeCustomPanel);
        m_activeCustomPanel->setParent(nullptr);
        m_activeCustomPanel->hide();
        m_activeCustomPanel = nullptr;
    }

    if (m_customPanelLayout) {
        widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        m_customPanelLayout->addWidget(widget, 0, Qt::AlignTop);
        widget->show();
        m_activeCustomPanel = widget;
    }
    if (m_configStack) {
        m_configStack->setVisible(true);
        m_configStack->setCurrentIndex(1);
    }
    updateBottomAreaVisibility();
    QTimer::singleShot(0, this, [this]() { fitBuildPanel(); });
}

void CodeEditorView::setWrapEnabled(bool enabled)
{
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto* editor = m_tabWidget->editorAt(i);
        if (editor)
            editor->setLineWrapMode(enabled ? QTextEdit::WidgetWidth : QTextEdit::NoWrap);
    }
    if (m_wrapAction && m_wrapAction->isChecked() != enabled) {
        QSignalBlocker b(m_wrapAction);
        m_wrapAction->setChecked(enabled);
    }
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
    if (!isLogPanelVisible())
        setLogPanelVisible(true);

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
