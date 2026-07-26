#ifndef ADAPTIXCLIENT_PAGENAVBAR_H
#define ADAPTIXCLIENT_PAGENAVBAR_H

#include <main.h>
#include <oclero/qlementine/widgets/LoadingSpinner.hpp>
#include <oclero/qlementine/widgets/LineEdit.hpp>

class PageNavBar : public QWidget
{
Q_OBJECT
    oclero::qlementine::LoadingSpinner* loadingSpinner = nullptr;
    oclero::qlementine::LineEdit*       filterInput    = nullptr;
    QCheckBox*   autoCheck     = nullptr;
    QComboBox*   agentCombo    = nullptr;
    QPushButton* prevBtn       = nullptr;
    QLabel*      infoLabel     = nullptr;
    QPushButton* nextBtn       = nullptr;
    QLabel*      pageSizeLabel = nullptr;
    QSpinBox*    pageSizeSpin  = nullptr;
    QTimer*      filterDebounce = nullptr;
    int          m_lastAppliedSize = 100;
    bool         m_isolated        = false;

    static QList<PageNavBar*> s_instances;

    void onSpinChanged(int size);
    void applySize(int size);
    void applyAutoState(bool on);

public:
    explicit PageNavBar(QWidget* parent = nullptr);
    ~PageNavBar() override;

    void setInfo(int from, int to, int total);
    void setError(const QString& message);
    void setPrevEnabled(bool enabled);
    void setNextEnabled(bool enabled);
    void setLoading(bool loading);

    int  pageSize() const;
    void setPageSize(int size, bool syncGlobal = true);
    void setIsolated(bool isolated);

    QString filterText() const;
    void    clearFilter();
    void    setFilterPlaceholder(const QString& placeholder);
    void    focusFilter();

    qint64  currentAgent() const;
    void    setCurrentAgent(qint64 agentId);
    void    setAgents(const QList<qint64>& agentIds);
    void    addAgent(qint64 agentId);
    void    removeAgent(qint64 agentId);
    void    clearAgents();
    void    setAgentComboVisible(bool visible);
    void    setFilterVisible(bool visible);
    void    setAutoVisible(bool visible);

Q_SIGNALS:
    void prevClicked();
    void nextClicked();
    void pageSizeChanged(int size);
    void filterChanged();
    void agentChanged();
};

#endif
