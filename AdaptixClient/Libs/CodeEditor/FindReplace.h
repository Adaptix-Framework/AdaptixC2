#pragma once

#include <QWidget>

class QLineEdit;
class QToolButton;
class QLabel;
class QTextEdit;

class FindReplace : public QWidget
{
Q_OBJECT
    QTextEdit* m_editor;
    QLineEdit* m_findEdit;
    QLineEdit* m_replaceEdit;
    QToolButton* m_caseBtn;
    QToolButton* m_regexBtn;
    QLabel* m_matchLabel;
    QToolButton* m_closeBtn;
    QWidget* m_replaceRow;

    void findNext();
    void findPrev();
    void replaceOne();
    void replaceAll();
    void highlightAll();
    void clearHighlights();
    void updateMatchCount();

public:
    explicit FindReplace(QWidget* parent = nullptr);

    void setEditor(QTextEdit* editor);
    void showFind();
    void showReplace();

protected:
    void keyPressEvent(QKeyEvent* e) override;

};
