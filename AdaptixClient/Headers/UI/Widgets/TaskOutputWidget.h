#ifndef ADAPTIXCLIENT_TASKOUTPUTWIDGET_H
#define ADAPTIXCLIENT_TASKOUTPUTWIDGET_H

#include <main.h>
#include <Client/ConsoleTheme.h>

class TaskOutputWidget : public QWidget
{
Q_OBJECT
    QGridLayout* mainGridLayout = nullptr;
    QLabel*      label          = nullptr;
    QLineEdit*   inputMessage   = nullptr;
    QTextEdit*   outputTextEdit = nullptr;

    bool m_suppressPaletteGuard = false;

    void createUI();
    void forceThemeColors();
    ConsoleThemeData getActiveTheme() const;

public Q_SLOTS:
    void applyTheme();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

public:
    explicit TaskOutputWidget();
    ~TaskOutputWidget() override;

    void SetConten(const QString &message, const QString &text) const;
};

#endif
