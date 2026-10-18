#ifndef KEYPRESSHANDLER_H
#define KEYPRESSHANDLER_H

#include <main.h>
#include <MainAdaptix.h>
#include <UI/MainUI.h>
#include <Client/AuthProfile.h>
#include <QAbstractItemView>
#include <QPlainTextEdit>
#include <QTextCursor>

class KPH_SearchInput : public QObject
{
Q_OBJECT
     QLineEdit*  inputLineEdit;

public:
     KPH_SearchInput(QLineEdit *input, QObject *parent = nullptr) : QObject(parent), inputLineEdit(input) {
         inputLineEdit->installEventFilter(this);
     }

Q_SIGNALS:
    void escPressed();

protected:
     bool eventFilter(QObject *watched, QEvent *event) override {
         if (watched == inputLineEdit && event->type() == QEvent::KeyPress) {
             QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

             if ( keyEvent->key() == Qt::Key_Escape ) {
                 Q_EMIT escPressed();
             }
         }
         return QObject::eventFilter(watched, event);
     }
};



class KPH_ConsoleInput : public QObject
{
Q_OBJECT
    QPlainTextEdit* inputLineEdit;
    QPlainTextEdit* outputTextEdit;
    QCompleter* completer = nullptr;
    bool expanded = false;
    QStringListModel* commandModel = nullptr;
    QString    tmpCommandLine;

    QStringList history;
    int historyIndex;

    QStringList completions;
    int completionIndex = -1;
    QString completionBase;
    QString completionPrefix;
    QString completionSuffix;

public:
    KPH_ConsoleInput(QPlainTextEdit *input, QPlainTextEdit *output, QObject *parent = nullptr) : QObject(parent), inputLineEdit(input), outputTextEdit(output), historyIndex(-1) {
        inputLineEdit->installEventFilter(this);
    }

    void setCommandModel(QStringListModel* model) { commandModel = model; }
    void setCompleter(QCompleter* c) { completer = c; }
    void setExpanded(bool on) { expanded = on; }

    QString editText() const { return inputLineEdit->toPlainText(); }
    void setEditText(const QString& s) { inputLineEdit->setPlainText(s); }
    int editCursor() const { return inputLineEdit->textCursor().position(); }
    void setEditCursor(int pos) {
        QTextCursor c = inputLineEdit->textCursor();
        c.setPosition(qBound(0, pos, editText().size()));
        inputLineEdit->setTextCursor(c);
    }
    void setEditSelection(int start, int len) {
        QTextCursor c = inputLineEdit->textCursor();
        const int n = editText().size();
        start = qBound(0, start, n);
        const int end = qBound(start, start + len, n);
        c.setPosition(start);
        c.setPosition(end, QTextCursor::KeepAnchor);
        inputLineEdit->setTextCursor(c);
    }

    void setCompleterPopup(QAbstractItemView* popup) {
        if (popup)
            popup->installEventFilter(this);
    }

    void AddToHistory(const QString &command) {
        history.prepend(command);
        historyIndex = -1;
        tmpCommandLine = "";
    }
    
    const QStringList& getHistory() const { return history; }

Q_SIGNALS:
    void showHistory();
    void showHelp();
    void toggleExpand();
    void submitInput();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        // Intercept Tab on QCompleter popup (Tab events go to popup, not InputLineEdit)
        if (watched != inputLineEdit && event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
                if (completer && completer->popup() && completer->popup()->isVisible()) {
                    QModelIndex idx = completer->popup()->currentIndex();
                    if (idx.isValid()) {
                        inputLineEdit->blockSignals(true);
                        setEditText(idx.data().toString());
                        setEditCursor(editText().size());
                        inputLineEdit->blockSignals(false);
                    }
                    completer->popup()->hide();
                    completionIndex = -1;
                    inputLineEdit->setFocus();
                    return true;
                }
            }
        }

        if (watched == inputLineEdit && (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride)) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

            const Qt::KeyboardModifiers mods = keyEvent->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
            if (keyEvent->key() == Qt::Key_H && (mods == Qt::ControlModifier || mods == (Qt::ControlModifier | Qt::ShiftModifier))) {
                if (event->type() == QEvent::ShortcutOverride) {
                    keyEvent->accept();
                    return true;
                }
                if (mods == Qt::ControlModifier)
                    Q_EMIT showHistory();
                else
                    Q_EMIT showHelp();
                return true;
            }
        }

        if (watched == inputLineEdit && event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            const Qt::KeyboardModifiers mods = keyEvent->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);

            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                if (completer && completer->popup() && completer->popup()->isVisible()) {
                    QModelIndex idx = completer->popup()->currentIndex();
                    if (idx.isValid()) {
                        inputLineEdit->blockSignals(true);
                        setEditText(idx.data().toString());
                        setEditCursor(editText().size());
                        inputLineEdit->blockSignals(false);
                    }
                    completer->popup()->hide();
                    completionIndex = -1;
                    return true;
                }
                Q_EMIT submitInput();
                return true;
            }

            if (keyEvent->key() == Qt::Key_Up) {
                if ( history.size() == 0 )
                    return true;

                if (historyIndex < history.size() - 1) {
                    if (historyIndex == -1)
                        tmpCommandLine = editText();
                    historyIndex++;
                    setEditText(history[historyIndex]);
                } else {
                    historyIndex = history.size() - 1;
                    setEditText(history[history.size() - 1]);
                }
                return true;
            }

            if (keyEvent->key() == Qt::Key_Down) {
                if ( history.size() == 0 )
                    return true;
                if (historyIndex > 0) {
                    historyIndex--;
                    setEditText(history[historyIndex]);
                } else {
                    historyIndex = -1;
                    setEditText(tmpCommandLine);
                }
                return true;
            }

            if (keyEvent->key() == Qt::Key_Tab) {
                if (keyEvent->modifiers() & Qt::ControlModifier) {
                    QString baseDir;
                    if (GlobalClient && GlobalClient->mainUI) {
                        if (auto profile = GlobalClient->mainUI->GetCurrentProfile())
                            baseDir = profile->GetProjectDir();
                    }
                    QString filePath = QFileDialog::getOpenFileName(nullptr, "Select file", baseDir);
                    if (!filePath.isEmpty()) {
                        int cursorPos = editCursor();
                        QString text = editText();
                        text.insert(cursorPos, "\"" + filePath + "\"");
                        setEditText(text);
                        setEditCursor(cursorPos + filePath.length());
                    }
                    return true;
                }
                else {
                    QString text = editText();
                    int cursorPos = editCursor();
                    QString leftOfCursor = text.left(cursorPos).trimmed();
                    bool isFirstWord = !leftOfCursor.contains(' ');

                    if (isFirstWord && commandModel) {
                        // Command name completion
                        if (completionIndex < 0) {
                            completionBase = leftOfCursor;
                            completionSuffix = text.mid(cursorPos);
                            if (completionBase.isEmpty()) {
                                completionIndex = -1;
                                return true;
                            }

                            completions.clear();
                            completionIndex = 0;
                            const QStringList commands = commandModel->stringList();
                            for (const QString &cmd : commands) {
                                if (cmd.startsWith(completionBase, Qt::CaseInsensitive))
                                    completions << cmd;
                            }
                            completions.sort(Qt::CaseInsensitive);
                        }

                        if (!completions.isEmpty()) {
                            QString chosen = completions[completionIndex];
                            QString newText = chosen + completionSuffix;

                            inputLineEdit->blockSignals(true);
                            setEditText(newText);
                            int selStart = completionBase.length();
                            int selLen = chosen.length() - selStart;
                            setEditSelection(selStart, selLen);
                            inputLineEdit->blockSignals(false);

                            completionIndex = (completionIndex + 1) % completions.size();
                        }
                        return true;
                    }

                    // File path completion
                    if (completionIndex < 0) {
                        bool inQuotes = false;
                        int start = cursorPos;
                        while (start > 0) {
                            QChar prev = text.at(start - 1);
                            if (prev == '"' && (start < 2 || text.at(start - 2) != '\\'))
                                inQuotes = !inQuotes;
                            if (!inQuotes && prev.isSpace())
                                break;
                            start--;
                        }
                        completionPrefix = text.left(start);
                        completionBase = text.mid(start, cursorPos - start);
                        completionSuffix = text.mid(cursorPos);

                        if (completionBase.startsWith('"') && completionBase.endsWith('"'))
                            completionBase = completionBase.mid(1, completionBase.length() - 2);

                        QString expanded = completionBase;
                        if (expanded.startsWith("~/"))
                            expanded = QDir::homePath() + expanded.mid(1);

                        if (expanded == "") {
                            completionIndex = -1;
                            return true;
                        }

                        completions.clear();
                        completionIndex = 0;
                        QFileInfo fi(expanded);
                        QString dirPath = fi.path().isEmpty() ? QDir::currentPath() : fi.path();
                        QString nameStart = fi.fileName();
                        QDir dir(dirPath);

                        QStringList nameFilters{ nameStart + '*' };
                        QFileInfoList infos = dir.entryInfoList( nameFilters, QDir::NoDotAndDotDot | QDir::AllEntries, QDir::Name );

                        const QString home = QDir::homePath();
                        for (const QFileInfo &item : infos) {
                            QString path = item.absoluteFilePath();
                            if (path.startsWith(home))
                                path = '~' + path.mid(home.length());
                            if (item.isDir())
                                path += QDir::separator();
#ifdef Q_OS_WIN
                            path.replace('/', '\\');
#endif
                            if (path.contains(' '))
                                path = '"' + path + '"';
                            completions << path;
                        }
                    }

                    if (!completions.isEmpty()) {
                        QString chosen = completions[completionIndex];
                        QString newText = completionPrefix + chosen + completionSuffix;

                        inputLineEdit->blockSignals(true);
                        setEditText(newText);
                        int selBaseOffset = completionPrefix.length();
                        int selOffset = completionBase.length();
                        int selStart = selBaseOffset + selOffset;
                        int selLen = chosen.length() - selOffset;
                        setEditSelection(selStart, selLen);
                        inputLineEdit->blockSignals(false);

                        completionIndex = (completionIndex + 1) % completions.size();
                    }
                    return true;
                }
            }
            completionIndex = -1;
        }
        return QObject::eventFilter(watched, event);
    }
};

#endif
