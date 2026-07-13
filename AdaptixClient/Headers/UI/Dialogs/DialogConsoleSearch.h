#ifndef ADAPTIXCLIENT_DIALOGCONSOLESEARCH_H
#define ADAPTIXCLIENT_DIALOGCONSOLESEARCH_H

#include <main.h>
#include <QPointer>

class AuthProfile;
class TextEditConsole;

class DialogConsoleSearch : public QDialog
{
Q_OBJECT

    qint64       agentId = 0;
    AuthProfile* profile = nullptr;

    QLineEdit*   queryEdit     = nullptr;
    QPushButton* searchButton  = nullptr;
    QLabel*      statusLabel   = nullptr;
    QListWidget* resultsList   = nullptr;
    TextEditConsole* contextView = nullptr;
    QSpinBox*    contextSpin   = nullptr;
    QPushButton* expandButton  = nullptr;
    QPushButton* openButton    = nullptr;
    QPushButton* closeButton   = nullptr;

    QVector<qint64> hitIds;
    int             hitTotal = 0;
    qint64          currentCenterId = 0;
    int             contextLimit = 4;
    bool            searching = false;
    bool            loadingContext = false;

    void createUI();
    void runSearch();
    void loadContext(qint64 centerId);
    void renderPackets(const QJsonArray& items);
    void setBusy(bool busy);

public:
    explicit DialogConsoleSearch(qint64 agentId, AuthProfile* profile, QWidget* parent = nullptr);
    ~DialogConsoleSearch() override;

    void setInitialQuery(const QString& query);

    Q_SIGNALS:
        void openInConsole(qint64 centerId, int contextLimit);
};

#endif
