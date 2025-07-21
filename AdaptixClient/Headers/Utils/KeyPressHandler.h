#ifndef KEYPRESSHANDLER_H
#define KEYPRESSHANDLER_H

#include <main.h>

class KPH_ConsoleInput : public QObject
{
Q_OBJECT
    QLineEdit*  inputLineEdit;
    QTextEdit*  outputTextEdit;
    QString     tmpCommandLine;

    QStringList history;
    int historyIndex;

    QStringList completions;
    int completionIndex;
    QString completionBase;
    QString completionPrefix;
    QString completionSuffix;

public:
    KPH_ConsoleInput(QLineEdit *input, QTextEdit *output, QObject *parent = nullptr) : QObject(parent), inputLineEdit(input), outputTextEdit(output), historyIndex(-1) {
        inputLineEdit->installEventFilter(this);
    }

    void AddToHistory(const QString &command) {
        history.prepend(command);
        historyIndex = -1;
        tmpCommandLine = "";
    }
    
    const QStringList& getHistory() const { return history; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == inputLineEdit && event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

            if (keyEvent->key() == Qt::Key_Up) {
                if ( history.size() == 0 )
                    return true;

                if (historyIndex < history.size() - 1) {
                    if (historyIndex == -1)
                        tmpCommandLine = inputLineEdit->text();
                    historyIndex++;
                    inputLineEdit->setText(history[historyIndex]);
                } else {
                    historyIndex = history.size() - 1;
                    inputLineEdit->setText(history[history.size() - 1]);
                }
                return true;
            }

            if (keyEvent->key() == Qt::Key_Down) {
                if ( history.size() == 0 )
                    return true;
                if (historyIndex > 0) {
                    historyIndex--;
                    inputLineEdit->setText(history[historyIndex]);
                } else {
                    historyIndex = -1;
                    inputLineEdit->setText(tmpCommandLine);
                }
                return true;
            }

            if (keyEvent->key() == Qt::Key_Tab) {
                if (keyEvent->modifiers() & Qt::ControlModifier) {
                    QString filePath = QFileDialog::getOpenFileName(nullptr, "Select file");
                    if (!filePath.isEmpty()) {
                        int cursorPos = inputLineEdit->cursorPosition();
                        QString text = inputLineEdit->text();
                        text.insert(cursorPos, "\"" + filePath + "\"");
                        inputLineEdit->setText(text);
                        inputLineEdit->setCursorPosition(cursorPos + filePath.length());
                    }
                    return true;
                }
                else {
                    if (completionIndex < 0) {
                        QString text = inputLineEdit->text();
                        int cursorPos = inputLineEdit->cursorPosition();

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
                        inputLineEdit->setText(newText);

                        int selBaseOffset = completionPrefix.length();
                        int selOffset = completionBase.length();
                        int selStart = selBaseOffset + selOffset;
                        int selLen = chosen.length() - selOffset;
                        inputLineEdit->setSelection(selStart, selLen);

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
