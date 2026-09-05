#ifndef ADAPTIXCLIENT_DIALOGCONSOLEHELP_H
#define ADAPTIXCLIENT_DIALOGCONSOLEHELP_H

#include <main.h>
#include <QPointer>

class Commander;

class DialogConsoleHelp : public QDialog
{
Q_OBJECT

    Commander*    commander    = nullptr;
    qint64        agentId      = 0;
    QComboBox*    commandCombo = nullptr;
    QPlainTextEdit* helpView   = nullptr;
    QPushButton*  closeButton  = nullptr;
    bool          refreshing   = false;

    void createUI();
    void refreshHelp();
    void applyTheme();
    static QString stripHelpPrefix(QString text);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

public:
    explicit DialogConsoleHelp(Commander* commander, qint64 agentId, QWidget* parent = nullptr);
    ~DialogConsoleHelp() override;

    void setQuery(const QString& text);
    void reloadCatalog();

    Q_SIGNALS:
        void insertCommand(const QString& command);
};

#endif
