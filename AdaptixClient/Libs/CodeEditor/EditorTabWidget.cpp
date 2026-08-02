#include <EditorTabWidget.h>
#include <CodeEditor.h>
#include <CodeFolding.h>
#include <SyntaxStyle.h>
#include <StyleSyntaxHighlighter.h>
#include <CXXHighlighter.h>
#include <CXXCompleter.h>
#include <AxScriptCompleter.h>
#include <AxScriptHighlighter.h>

#include <QTabBar>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <algorithm>

EditorTabWidget::EditorTabWidget(QWidget* parent) : QTabWidget(parent), m_syntaxStyle(nullptr)
{
    setMovable(true);

    connect(this, &QTabWidget::tabCloseRequested, this, [this](int index) {
        closeTab(index);
    });

    connect(this, &QTabWidget::currentChanged, this, [this](int index) {
        Q_UNUSED(index);
        Q_EMIT currentTabChanged(index);
    });

    setTabsClosable(true);
}

void EditorTabWidget::setSyntaxStyle(SyntaxStyle* style)
{
    m_syntaxStyle = style;
    for (int i = 0; i < count(); ++i) {
        auto editor = editorAt(i);
        if (editor)
            editor->setSyntaxStyle(style);
    }
}

SyntaxStyle* EditorTabWidget::syntaxStyle() const
{
    return m_syntaxStyle;
}

CodeEditor* EditorTabWidget::createEditor()
{
    auto editor = new CodeEditor();

    auto highlighter = new CXXHighlighter(editor->document());
    editor->setHighlighter(highlighter);

    auto completer = new CXXCompleter(editor);
    editor->setCompleter(completer);

    if (m_syntaxStyle)
        editor->setSyntaxStyle(m_syntaxStyle);

    editor->setCodeFoldingEnabled(true);
    editor->setMinimapEnabled(false);
    editor->setLineWrapMode(QTextEdit::NoWrap);

    connect(editor->document(), &QTextDocument::contentsChanged,
            this, [this, editor]() {
                for (int i = 0; i < count(); ++i) {
                    if (editorAt(i) == editor) {
                        updateTabTitle(i);
                        break;
                    }
                }
            });
    return editor;
}

CodeEditor* EditorTabWidget::newTab(const QString& title)
{
    auto editor = createEditor();
    int index = addTab(editor, title);
    setTabsClosable(true);
    setCurrentIndex(index);
    editor->markSaved();
    updateTabTitle(index);

    if (auto* cxx = editor->findChild<CXXHighlighter*>())
        cxx->notifyDocumentLoaded();

    return editor;
}

CodeEditor* EditorTabWidget::openFile(const QString& filePath)
{
    for (int i = 0; i < count(); ++i) {
        auto editor = editorAt(i);
        if (editor && editor->filePath() == filePath) {
            setCurrentIndex(i);
            return editor;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return nullptr;

    const QByteArray raw = file.readAll();
    file.close();
    const QString content = QString::fromUtf8(raw);

    const int approxLines = int(std::count(raw.begin(), raw.end(), '\n')) + 1;
    const bool largeDoc = raw.size() >= CXXHighlighter::kLargeFileChars || approxLines >= CXXHighlighter::kLargeFileBlocks;

    auto editor = createEditor();
    editor->setFilePath(filePath);
    editor->setUpdatesEnabled(false);

    if (auto* hl = editor->highlighter())
        hl->setDocument(nullptr);

    if (editor->document())
        editor->document()->setUndoRedoEnabled(!largeDoc);

    editor->setPlainText(content);

    QFileInfo info(filePath);
    const QString suffix = info.suffix().toLower();
    const bool wantAx = (suffix == QLatin1String("axs") || suffix == QLatin1String("js") || suffix == QLatin1String("javascript") || suffix == QLatin1String("mjs"));

    if (largeDoc) {
        if (auto* hl = editor->highlighter()) {
            editor->setHighlighter(nullptr);
            hl->deleteLater();
        }
        for (auto* h : editor->findChildren<CXXHighlighter*>()) {
            h->setDocument(nullptr);
            h->deleteLater();
        }
        editor->setCodeFoldingEnabled(false);
    } else if (wantAx) {
        if (auto* old = editor->findChild<CXXHighlighter*>()) {
            old->setDocument(nullptr);
            old->deleteLater();
        }
        auto* axHighlighter = new AxScriptHighlighter(editor->document());
        axHighlighter->setFilePath(filePath);
        if (editor->syntaxStyle())
            axHighlighter->setSyntaxStyle(editor->syntaxStyle());
        editor->setHighlighter(axHighlighter);

        if (auto* oldCompleter = editor->completer())
            oldCompleter->deleteLater();
        editor->setCompleter(new AxScriptCompleter(editor));
    } else if (auto* cxxHl = editor->findChild<CXXHighlighter*>()) {
        cxxHl->setFilePath(filePath);
        editor->setHighlighter(cxxHl);
        cxxHl->notifyDocumentLoaded();
    }

    editor->markSaved();
    editor->setUpdatesEnabled(true);

    const int index = addTab(editor, largeDoc ? info.fileName() + QStringLiteral(" [plain]") : info.fileName());
    setTabToolTip(index, largeDoc ? filePath + QStringLiteral("\n(Large file: syntax highlighting disabled for performance)") : filePath);
    setTabsClosable(true);
    setCurrentIndex(index);

    if (!largeDoc) {
        QTimer::singleShot(100, editor, [editor]() {
            if (editor->isCodeFoldingEnabled() && editor->codeFolding())
                editor->codeFolding()->updateFoldingData();
        });
    }

    Q_EMIT fileOpened(filePath);
    return editor;
}

CodeEditor* EditorTabWidget::currentEditor() const
{
    return qobject_cast<CodeEditor*>(currentWidget());
}

CodeEditor* EditorTabWidget::editorAt(int index) const
{
    return qobject_cast<CodeEditor*>(widget(index));
}

bool EditorTabWidget::closeTab(int index)
{
    if (index < 0 || index >= count())
        return false;

    CodeEditor* editor = editorAt(index);

    if (editor && editor->isModified()) {
        QString name;
        if (editor->filePath().isEmpty()) {
            name = tabText(index);
            if (name.startsWith('*')) name = name.mid(1);
        }
        else {
            name = QFileInfo(editor->filePath()).fileName();
        }

        auto result = QMessageBox::question( this, "Save Changes", QString("Save changes to \"%1\"?").arg(name), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Discard );

        if (result == QMessageBox::Save) {
            bool saved = false;
            if (editor->filePath().isEmpty()) {
                auto filePath = QFileDialog::getSaveFileName( this, "Save File", "", "C/C++ Files (*.c *.cpp *.h *.hpp *.cc *.hh);;All Files (*)" );
                if (filePath.isEmpty())
                    return false;

                editor->setFilePath(filePath);

                QFile file(filePath);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&file);
                    out << editor->toPlainText();
                    file.close();
                    editor->markSaved();
                    saved = true;
                    Q_EMIT fileSaved(filePath);
                }
            }
            else {
                QFile file(editor->filePath());
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&file);
                    out << editor->toPlainText();
                    file.close();
                    editor->markSaved();
                    saved = true;
                    Q_EMIT fileSaved(editor->filePath());
                }
            }
            if (!saved) {
                QMessageBox::warning(this, "Error", "Failed to save file.");
                return false;
            }
        }
        else if (result == QMessageBox::Cancel) {
            return false;
        }
    }

    removeTab(index);
    if (editor)
        delete editor;
    return true;
}

bool EditorTabWidget::saveCurrentTab()
{
    auto editor = currentEditor();
    if (!editor)
        return false;

    const QString path = editor->filePath();
    if (path.isEmpty() || QFileInfo(path).isRelative())
        return saveCurrentTabAs();

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, "Error", QString("Cannot write to file: %1").arg(path));
        return false;
    }

    QTextStream out(&file);
    out << editor->toPlainText();
    file.close();

    editor->markSaved();
    Q_EMIT fileSaved(path);
    return true;
}

bool EditorTabWidget::saveCurrentTabAs()
{
    auto editor = currentEditor();
    if (!editor)
        return false;

    auto filePath = QFileDialog::getSaveFileName(this, "Save File", "", "AxScript Files (*.axs);;C/C++ Files (*.c *.cpp *.h *.hpp *.cc *.hh);;All Files (*)");

    if (filePath.isEmpty())
        return false;

    editor->setFilePath(filePath);
    QFileInfo info(filePath);
    setTabText(currentIndex(), info.fileName());
    setTabToolTip(currentIndex(), filePath);

    return saveCurrentTab();
}

bool EditorTabWidget::saveAllTabs()
{
    for (int i = 0; i < count(); ++i) {
        auto editor = editorAt(i);
        if (editor && editor->isModified()) {
            setCurrentIndex(i);
            if (!saveCurrentTab())
                return false;
        }
    }
    return true;
}

int EditorTabWidget::modifiedCount() const
{
    int cnt = 0;
    for (int i = 0; i < count(); ++i) {
        auto editor = editorAt(i);
        if (editor && editor->isModified())
            ++cnt;
    }
    return cnt;
}

void EditorTabWidget::tabInserted(int index)
{
    QTabWidget::tabInserted(index);
    updateTabTitle(index);
}

void EditorTabWidget::tabRemoved(int index)
{
    Q_UNUSED(index);
}

void EditorTabWidget::updateTabTitle(int index)
{
    auto editor = editorAt(index);
    if (!editor)
        return;

    QString title = tabText(index);
    bool modified = editor->isModified();

    if (modified && !title.startsWith("*"))
        setTabText(index, "*" + title);
    else if (!modified && title.startsWith("*"))
        setTabText(index, title.mid(1));
}
