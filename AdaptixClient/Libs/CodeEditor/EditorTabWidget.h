#pragma once

#include <QTabWidget>

class CodeEditor;
class SyntaxStyle;

class EditorTabWidget : public QTabWidget
{
Q_OBJECT
    SyntaxStyle* m_syntaxStyle;

    void updateTabTitle(int index);
    CodeEditor* createEditor();

public:
    explicit EditorTabWidget(QWidget* parent = nullptr);

    void setSyntaxStyle(SyntaxStyle* style);
    SyntaxStyle* syntaxStyle() const;

    CodeEditor* newTab(const QString& title = "Untitled");
    CodeEditor* openFile(const QString& filePath);
    CodeEditor* currentEditor() const;
    CodeEditor* editorAt(int index) const;

    bool closeTab(int index);
    bool saveCurrentTab();
    bool saveCurrentTabAs();
    bool saveAllTabs();

    int modifiedCount() const;

Q_SIGNALS:
    void fileOpened(const QString& filePath);
    void fileSaved(const QString& filePath);
    void currentTabChanged(int index);

protected:
    void tabInserted(int index) override;
    void tabRemoved(int index) override;
};
