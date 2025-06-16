#ifndef KEYPRESSHANDLER_H
#define KEYPRESSHANDLER_H

#include <main.h>

// class KPH_ConsoleInput : public QObject {
//     QTextEdit *textEdit;
//
// public:
//     KPH_ConsoleInput(QTextEdit *textEdit) : targetLineEdit(lineEdit) {}
//
// protected:
//     bool eventFilter(QObject *obj, QEvent *event) override {
//         if (event->type() == QEvent::KeyPress) {
//             QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
//             QLineEdit *lineEdit = qobject_cast<QTextEdit *>(obj);
//
//             if (lineEdit && lineEdit->hasFocus()) {
//                 if (keyEvent->key() == Qt::Key_L && keyEvent->modifiers() & Qt::ControlModifier) {
//                     textEdit->clear();
//                     return true;
//                 }
//             }
//         }
//         return QObject::eventFilter(obj, event);
//     }
// };

// class TableSearchHandler : public QObject {
//     Q_OBJECT
//
// public:
//     TableSearchHandler(QTableWidget *table, QWidget *parent = nullptr) : QObject(parent), tableWidget(table) {
//
//         searchLineEdit = new QLineEdit(parent);
//         searchLineEdit->setPlaceholderText("Search...");
//         searchLineEdit->hide();
//
//         QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(parent->layout());
//         if (mainLayout) {
//             mainLayout->addWidget(searchLineEdit);
//         }
//
//         connect(searchLineEdit, &QLineEdit::textChanged, this, &TableSearchHandler::filterTable);
//
//         tableWidget->installEventFilter(this);
//     }
//
// protected:
//     bool eventFilter(QObject *watched, QEvent *event) override {
//         if (watched == tableWidget && event->type() == QEvent::KeyPress) {
//             QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
//
//             if (keyEvent->key() == Qt::Key_F && keyEvent->modifiers() & Qt::ControlModifier) {
//                 toggleSearchField();
//                 return true;
//             }
//         }
//         return QObject::eventFilter(watched, event);
//     }
//
// private:
//     QTableWidget *tableWidget;
//     QLineEdit *searchLineEdit;
//     QWidget *searchWidget;
//
//     void toggleSearchField() {
//         if (searchLineEdit->isVisible()) {
//             searchLineEdit->hide();
//         } else {
//             searchLineEdit->show();
//             searchLineEdit->setFocus();
//         }
//     }
//
//     void filterTable(const QString &text) {
//         for (int row = 0; row < tableWidget->rowCount(); ++row) {
//             bool matchFound = false;
//             for (int col = 0; col < tableWidget->columnCount(); ++col) {
//                 QTableWidgetItem *item = tableWidget->item(row, col);
//                 if (item && item->text().contains(text, Qt::CaseInsensitive)) {
//                     matchFound = true;
//                     break;
//                 }
//             }
//             tableWidget->setRowHidden(row, !matchFound);
//         }
//     }
// };

// class KPH_UpTable : public QObject
// {
// Q_OBJECT
//     QTableWidget *tableWidget;
//
// public:
//     KPH_UpTable(QTableWidget *table, QObject *parent = nullptr) : QObject(parent), tableWidget(table) {
//         tableWidget->installEventFilter(this);
//     }
//
// protected:
//     bool eventFilter(QObject *watched, QEvent *event) override {
//         if (watched == tableWidget && event->type() == QEvent::KeyPress) {
//             QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
//             int row = tableWidget->currentRow();
//             int rowCount = tableWidget->rowCount();
//
//             if (keyEvent->key() == Qt::Key_Up) {
//                 if (row > 0) tableWidget->setCurrentCell(row - 1, 0);
//                 return true;
//             }
//             if (keyEvent->key() == Qt::Key_Down) {
//                 if (row < rowCount - 1) tableWidget->setCurrentCell(row + 1, 0);
//                 return true;
//             }
//             if (keyEvent->key() == Qt::Key_A && keyEvent->modifiers() & Qt::ControlModifier) {
//                 tableWidget->selectAll();
//                 return true;
//             }
//         }
//         return QObject::eventFilter(watched, event);
//     }
// };

class KPH_ConsoleInput : public QObject
{
Q_OBJECT
    QLineEdit *inputLineEdit;
    QTextEdit *outputTextEdit;
    QString tmpCommandLine;
    QStringList history;
    int historyIndex;

public:
    KPH_ConsoleInput(QLineEdit *input, QTextEdit *output, QObject *parent = nullptr) : QObject(parent), inputLineEdit(input), outputTextEdit(output), historyIndex(-1) {
        inputLineEdit->installEventFilter(this);
    }

    void AddToHistory(const QString &command) {
        history.prepend(command);
        historyIndex = -1;
        tmpCommandLine = "";
    }

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
        }
        return QObject::eventFilter(watched, event);
    }
};

#endif //KEYPRESSHANDLER_H
