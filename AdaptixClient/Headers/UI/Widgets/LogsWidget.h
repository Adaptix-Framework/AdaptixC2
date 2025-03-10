#ifndef ADAPTIXCLIENT_LOGSWIDGET_H
#define ADAPTIXCLIENT_LOGSWIDGET_H

#include <main.h>

class LogsWidget : public QWidget
{
    QGridLayout* mainGridLayout      = nullptr;
    QGridLayout* logsGridLayout      = nullptr;
    QGridLayout* todoGridLayout      = nullptr;
    QTextEdit*   logsConsoleTextEdit = nullptr;
    QLabel*      logsLabel           = nullptr;
    QLabel*      todoLabel           = nullptr;
    QSplitter*   mainHSplitter       = nullptr;
    QWidget*     logsWidget          = nullptr;
    QWidget*     todoWidget          = nullptr;

    void createUI();

public:
    explicit LogsWidget();
    ~LogsWidget() override;

     void AddLogs( int type, qint64 time, const QString &Message) const;
     void Clear() const;
};

#endif //ADAPTIXCLIENT_LOGSWIDGET_H
