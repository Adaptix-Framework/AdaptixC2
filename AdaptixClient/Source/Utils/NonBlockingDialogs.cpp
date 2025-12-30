#include <Utils/NonBlockingDialogs.h>

#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTimer>
#include <QApplication>
#include <QEventLoop>
#include <functional>

QTimer* NonBlockingDialogs::keepAliveTimer = nullptr;
int     NonBlockingDialogs::activeDialogs  = 0;

void NonBlockingDialogs::startKeepAlive()
{
    activeDialogs++;
    if (!keepAliveTimer) {
        keepAliveTimer = new QTimer();
        QObject::connect(keepAliveTimer, &QTimer::timeout, [](){
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 50);
        });
    }
    if (!keepAliveTimer->isActive()) {
        keepAliveTimer->start(500);
    }
}

void NonBlockingDialogs::stopKeepAlive()
{
    activeDialogs--;
    if (activeDialogs <= 0 && keepAliveTimer) {
        keepAliveTimer->stop();
        activeDialogs = 0;
    }
}

void NonBlockingDialogs::getSaveFileName(QWidget* parent, const QString& caption, const QString& dir, const QString& filter, std::function<void(const QString&)> callback)
{
    QFileDialog* dialog = new QFileDialog(parent);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setWindowTitle(caption);
    dialog->setDirectory(dir);
    dialog->setNameFilter(filter);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QFileDialog::finished, [dialog, callback](int result) {
        stopKeepAlive();
        if (result == QDialog::Accepted && !dialog->selectedFiles().isEmpty()) {
            callback(dialog->selectedFiles().first());
        } else {
            callback(QString());
        }
    });

    startKeepAlive();
    dialog->open();
}

void NonBlockingDialogs::getOpenFileName(QWidget* parent, const QString& caption, const QString& dir, const QString& filter, std::function<void(const QString&)> callback)
{
    QFileDialog* dialog = new QFileDialog(parent);
    dialog->setAcceptMode(QFileDialog::AcceptOpen);
    dialog->setFileMode(QFileDialog::ExistingFile);
    dialog->setWindowTitle(caption);
    dialog->setDirectory(dir);
    dialog->setNameFilter(filter);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QFileDialog::finished, [dialog, callback](int result) {
        stopKeepAlive();
        if (result == QDialog::Accepted && !dialog->selectedFiles().isEmpty()) {
            callback(dialog->selectedFiles().first());
        } else {
            callback(QString());
        }
    });

    startKeepAlive();
    dialog->open();
}

void NonBlockingDialogs::getExistingDirectory(QWidget* parent, const QString& caption, const QString& dir, std::function<void(const QString&)> callback)
{
    QFileDialog* dialog = new QFileDialog(parent);
    dialog->setAcceptMode(QFileDialog::AcceptOpen);
    dialog->setFileMode(QFileDialog::Directory);
    dialog->setOption(QFileDialog::ShowDirsOnly, true);
    dialog->setWindowTitle(caption);
    dialog->setDirectory(dir);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QFileDialog::finished, [dialog, callback](int result) {
        stopKeepAlive();
        if (result == QDialog::Accepted && !dialog->selectedFiles().isEmpty()) {
            callback(dialog->selectedFiles().first());
        } else {
            callback(QString());
        }
    });

    startKeepAlive();
    dialog->open();
}

void NonBlockingDialogs::getText(QWidget* parent, const QString& title, const QString& label, const QString& text, std::function<void(const QString&, bool)> callback)
{
    QInputDialog* dialog = new QInputDialog(parent);
    dialog->setWindowTitle(title);
    dialog->setLabelText(label);
    dialog->setTextValue(text);
    dialog->setInputMode(QInputDialog::TextInput);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QInputDialog::textValueSelected, [callback](const QString& text) {
        stopKeepAlive();
        callback(text, true);
    });

    QObject::connect(dialog, &QInputDialog::rejected, [callback]() {
        stopKeepAlive();
        callback(QString(), false);
    });

    startKeepAlive();
    dialog->open();
}

void NonBlockingDialogs::getInt(QWidget* parent, const QString& title, const QString& label, int value, int min, int max, int step, std::function<void(int, bool)> callback)
{
    QInputDialog* dialog = new QInputDialog(parent);
    dialog->setWindowTitle(title);
    dialog->setLabelText(label);
    dialog->setIntValue(value);
    dialog->setIntMinimum(min);
    dialog->setIntMaximum(max);
    dialog->setIntStep(step);
    dialog->setInputMode(QInputDialog::IntInput);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(dialog, &QInputDialog::intValueSelected, [callback](int value) {
        stopKeepAlive();
        callback(value, true);
    });

    QObject::connect(dialog, &QInputDialog::rejected, [callback]() {
        stopKeepAlive();
        callback(0, false);
    });

    startKeepAlive();
    dialog->open();
}

void NonBlockingDialogs::question(QWidget* parent, const QString& title, const QString& text, std::function<void(QMessageBox::StandardButton)> callback)
{
    QMessageBox* msgBox = new QMessageBox(parent);
    msgBox->setWindowTitle(title);
    msgBox->setText(text);
    msgBox->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox->setDefaultButton(QMessageBox::No);
    msgBox->setIcon(QMessageBox::Question);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(msgBox, &QMessageBox::buttonClicked, [callback](QAbstractButton* button) {
        stopKeepAlive();
        QMessageBox::StandardButton standardButton = QMessageBox::NoButton;
        if (button) {
            QMessageBox* msgBox = qobject_cast<QMessageBox*>(button->parent());
            if (msgBox) {
                standardButton = msgBox->standardButton(button);
            }
        }
        callback(standardButton);
    });

    startKeepAlive();
    msgBox->open();
}

void NonBlockingDialogs::information(QWidget* parent, const QString& title, const QString& text, std::function<void()> callback)
{
    QMessageBox* msgBox = new QMessageBox(parent);
    msgBox->setWindowTitle(title);
    msgBox->setText(text);
    msgBox->setStandardButtons(QMessageBox::Ok);
    msgBox->setIcon(QMessageBox::Information);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(msgBox, &QMessageBox::finished, [callback](int) {
        stopKeepAlive();
        if (callback) {
            callback();
        }
    });

    startKeepAlive();
    msgBox->open();
}

void NonBlockingDialogs::critical(QWidget* parent, const QString& title, const QString& text, std::function<void()> callback)
{
    QMessageBox* msgBox = new QMessageBox(parent);
    msgBox->setWindowTitle(title);
    msgBox->setText(text);
    msgBox->setStandardButtons(QMessageBox::Ok);
    msgBox->setIcon(QMessageBox::Critical);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);

    QObject::connect(msgBox, &QMessageBox::finished, [callback](int) {
        stopKeepAlive();
        if (callback) {
            callback();
        }
    });

    startKeepAlive();
    msgBox->open();
}