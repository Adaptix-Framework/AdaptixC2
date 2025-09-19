#ifndef NONBLOCKINGDIALOGS_H
#define NONBLOCKINGDIALOGS_H

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

public:
    // 非阻塞文件对话框
    static void getSaveFileName(QWidget* parent, const QString& caption, 
                               const QString& dir, const QString& filter,
                               std::function<void(const QString&)> callback);
    
    static void getOpenFileName(QWidget* parent, const QString& caption,
                               const QString& dir, const QString& filter,
                               std::function<void(const QString&)> callback);
    
    static void getExistingDirectory(QWidget* parent, const QString& caption,
                                   const QString& dir,
                                   std::function<void(const QString&)> callback);
    
    // 非阻塞输入对话框
    static void getText(QWidget* parent, const QString& title, const QString& label,
                       const QString& text, std::function<void(const QString&, bool)> callback);
    
    static void getInt(QWidget* parent, const QString& title, const QString& label,
                      int value, int min, int max, int step,
                      std::function<void(int, bool)> callback);
    
    // 非阻塞消息对话框
    static void question(QWidget* parent, const QString& title, const QString& text,
                        std::function<void(QMessageBox::StandardButton)> callback);
    
    static void information(QWidget* parent, const QString& title, const QString& text,
                           std::function<void()> callback = nullptr);
    
    static void critical(QWidget* parent, const QString& title, const QString& text,
                        std::function<void()> callback = nullptr);

private:
    // 保持WebSocket连接活跃的定时器
    static void startKeepAlive();
    static void stopKeepAlive();
    static QTimer* keepAliveTimer;
    static int activeDialogs;
};

#endif // NONBLOCKINGDIALOGS_H