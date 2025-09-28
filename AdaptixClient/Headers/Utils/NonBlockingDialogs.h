#ifndef ADAPTIXCLIENT_NONBLOCKINGDIALOGS_H
#define ADAPTIXCLIENT_NONBLOCKINGDIALOGS_H

#include <QObject>
#include <QTimer>
#include <QApplication>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <functional>

class NonBlockingDialogs : public QObject
{
Q_OBJECT
    static QTimer* keepAliveTimer;
    static int     activeDialogs;

    static void startKeepAlive();
    static void stopKeepAlive();

public:
    static void getSaveFileName(QWidget* parent, const QString& caption, const QString& dir, const QString& filter, std::function<void(const QString&)> callback);

    static void getOpenFileName(QWidget* parent, const QString& caption, const QString& dir, const QString& filter, std::function<void(const QString&)> callback);

    static void getExistingDirectory(QWidget* parent, const QString& caption, const QString& dir, std::function<void(const QString&)> callback);

    static void getText(QWidget* parent, const QString& title, const QString& label, const QString& text, std::function<void(const QString&, bool)> callback);

    static void getInt(QWidget* parent, const QString& title, const QString& label, int value, int min, int max, int step, std::function<void(int, bool)> callback);

    static void question(QWidget* parent, const QString& title, const QString& text, std::function<void(QMessageBox::StandardButton)> callback);

    static void information(QWidget* parent, const QString& title, const QString& text, std::function<void()> callback = nullptr);

    static void critical(QWidget* parent, const QString& title, const QString& text, std::function<void()> callback = nullptr);
};

#endif