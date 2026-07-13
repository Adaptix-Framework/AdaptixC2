#include <Utils/CustomElements/PageNavBar.h>
#include <QHBoxLayout>
#include <Client/Settings.h>
#include <MainAdaptix.h>

QList<PageNavBar*> PageNavBar::s_instances;

PageNavBar::PageNavBar(QWidget* parent) : QWidget(parent)
{
    filterInput = new oclero::qlementine::LineEdit(this);
    filterInput->setIcon(QIcon(":/icons/search"));
    filterInput->setPlaceholderText("filter");
    filterInput->setMinimumWidth(180);
    filterInput->setMaximumWidth(320);

    autoCheck = new QCheckBox("Auto", this);
    autoCheck->setToolTip("Auto-apply filter and page size on change.\nWhen off, press Enter to apply.");
    {
        QSignalBlocker blocker(autoCheck);
        autoCheck->setChecked(true);
    }

    agentCombo = new QComboBox(this);
    agentCombo->setMinimumWidth(140);
    agentCombo->setEditable(true);
    agentCombo->setInsertPolicy(QComboBox::NoInsert);
    agentCombo->addItem("All agents");

    loadingSpinner = new oclero::qlementine::LoadingSpinner(this);
    loadingSpinner->setFixedSize(16, 16);
    loadingSpinner->setVisible(false);

    prevBtn = new QPushButton("◀", this);
    nextBtn = new QPushButton("▶", this);
    prevBtn->setToolTip("Previous page");
    nextBtn->setToolTip("Next page");
    prevBtn->setFixedSize(28, 24);
    nextBtn->setFixedSize(28, 24);

    infoLabel = new QLabel("—", this);
    infoLabel->setAlignment(Qt::AlignCenter);
    infoLabel->setMinimumWidth(120);

    pageSizeLabel = new QLabel("Per page:", this);
    pageSizeSpin  = new QSpinBox(this);
    pageSizeSpin->setMinimum(5);
    pageSizeSpin->setMaximum(1000);
    pageSizeSpin->setSingleStep(5);
    pageSizeSpin->setFixedWidth(64);
    pageSizeSpin->setKeyboardTracking(false);
    pageSizeSpin->setToolTip("Items per page");

    int initial = 100;
    if (GlobalClient && GlobalClient->settings) {
        initial = qBound(5, GlobalClient->settings->data.PageSize, 10000);
    }
    {
        QSignalBlocker blocker(pageSizeSpin);
        pageSizeSpin->setValue(initial);
    }
    m_lastAppliedSize = initial;

    filterDebounce = new QTimer(this);
    filterDebounce->setSingleShot(true);
    filterDebounce->setInterval(300);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(6);
    layout->addWidget(filterInput);
    layout->addWidget(autoCheck);
    layout->addWidget(agentCombo);
    layout->addStretch();
    layout->addWidget(loadingSpinner);
    layout->addWidget(prevBtn);
    layout->addWidget(infoLabel);
    layout->addWidget(nextBtn);
    layout->addSpacing(8);
    layout->addWidget(pageSizeLabel);
    layout->addWidget(pageSizeSpin);
    setLayout(layout);

    connect(prevBtn, &QPushButton::clicked, this, &PageNavBar::prevClicked);
    connect(nextBtn, &QPushButton::clicked, this, &PageNavBar::nextClicked);

    connect(pageSizeSpin, &QSpinBox::valueChanged, this, [this](int size) {
        if (!autoCheck->isChecked())
            return;
        if (size == m_lastAppliedSize)
            return;
        m_lastAppliedSize = size;
        onSpinChanged(size);
    });
    connect(pageSizeSpin, &QSpinBox::editingFinished, this, [this]() {
        if (autoCheck->isChecked())
            return;
        int v = pageSizeSpin->value();
        if (v == m_lastAppliedSize)
            return;
        m_lastAppliedSize = v;
        onSpinChanged(v);
    });
    connect(filterInput,  &QLineEdit::textChanged, this, [this](const QString&) {
        if (!autoCheck->isChecked())
            return;
        filterDebounce->start();
    });
    connect(filterInput, &QLineEdit::returnPressed, this, [this]() {
        filterDebounce->stop();
        Q_EMIT filterChanged();
    });
    connect(filterDebounce, &QTimer::timeout, this, [this]() {
        Q_EMIT filterChanged();
    });

    connect(agentCombo, &QComboBox::currentTextChanged, this, [this](const QString&) {
        Q_EMIT agentChanged();
    });
    connect(autoCheck, &QCheckBox::toggled, this, [this](bool on) {
        for (PageNavBar* bar : s_instances) {
            if (bar != this)
                bar->applyAutoState(on);
        }
    });

    setPrevEnabled(false);
    setNextEnabled(false);

    s_instances.append(this);
}

PageNavBar::~PageNavBar()
{
    s_instances.removeAll(this);
}

void PageNavBar::onSpinChanged(int size)
{
    if (GlobalClient && GlobalClient->settings) {
        GlobalClient->settings->data.PageSize = size;
        GlobalClient->settings->SaveToDB();
    }

    for (PageNavBar* bar : s_instances) {
        if (bar != this)
            bar->applySize(size);
    }

    Q_EMIT pageSizeChanged(size);
}

void PageNavBar::applySize(int size)
{
    {
        QSignalBlocker blocker(pageSizeSpin);
        pageSizeSpin->setValue(qBound(5, size, 10000));
    }
    m_lastAppliedSize = pageSizeSpin->value();
    Q_EMIT pageSizeChanged(pageSizeSpin->value());
}

void PageNavBar::applyAutoState(bool on)
{
    QSignalBlocker blocker(autoCheck);
    autoCheck->setChecked(on);
}

void PageNavBar::setInfo(int from, int to, int total)
{
    infoLabel->setStyleSheet(QString());
    infoLabel->setToolTip(QString());
    if (total <= 0)
        infoLabel->setText("No results");
    else
        infoLabel->setText(QString("%1–%2 / %3").arg(from).arg(to).arg(total));
}

void PageNavBar::setError(const QString& message)
{
    infoLabel->setStyleSheet("color: #E34234;");
    // infoLabel->setText(message.isEmpty() ? QStringLiteral("Error") : message);
    infoLabel->setText(QStringLiteral("Error"));
    infoLabel->setToolTip(message);
}

void PageNavBar::setPrevEnabled(bool enabled) { prevBtn->setEnabled(enabled); }
void PageNavBar::setNextEnabled(bool enabled) { nextBtn->setEnabled(enabled); }

int PageNavBar::pageSize() const { return pageSizeSpin->value(); }

void PageNavBar::setLoading(bool loading)
{
    loadingSpinner->setVisible(loading);
    loadingSpinner->setSpinning(loading);
    if (loading) {
        prevBtn->setEnabled(false);
        nextBtn->setEnabled(false);
    }
}

QString PageNavBar::filterText() const
{
    return filterInput->text().trimmed();
}

void PageNavBar::clearFilter()
{
    filterInput->clear();
}

void PageNavBar::setFilterPlaceholder(const QString& placeholder)
{
    filterInput->setPlaceholderText(placeholder);
}

void PageNavBar::focusFilter()
{
    filterInput->setFocus();
    filterInput->selectAll();
}

qint64 PageNavBar::currentAgent() const
{
    const QString text = agentCombo->currentText();
    if (text.isEmpty() || text == "All agents")
        return 0;
    if (agentCombo->findText(text) <= 0)
        return 0;
    return text.toLongLong();
}

void PageNavBar::setCurrentAgent(qint64 agentId)
{
    agentCombo->setCurrentText(agentId == 0 ? "All agents" : QString::number(agentId));
}

void PageNavBar::setAgents(const QList<qint64>& agentIds)
{
    QString current = agentCombo->currentText();
    QSignalBlocker blocker(agentCombo);
    agentCombo->clear();
    agentCombo->addItem("All agents");
    QList<qint64> sorted = agentIds;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    for (qint64 id : sorted)
        agentCombo->addItem(QString::number(id));
    int idx = agentCombo->findText(current);
    agentCombo->setCurrentIndex(idx >= 0 ? idx : 0);
}

void PageNavBar::addAgent(qint64 agentId)
{
    if (agentId == 0) return;
    QString s = QString::number(agentId);
    if (agentCombo->findText(s) == -1)
        agentCombo->addItem(s);
}

void PageNavBar::removeAgent(qint64 agentId)
{
    int idx = agentCombo->findText(QString::number(agentId));
    if (idx > 0)
        agentCombo->removeItem(idx);
}

void PageNavBar::clearAgents()
{
    QSignalBlocker blocker(agentCombo);
    agentCombo->clear();
    agentCombo->addItem("All agents");
}

void PageNavBar::setAgentComboVisible(bool visible)
{
    agentCombo->setVisible(visible);
}

void PageNavBar::setFilterVisible(bool visible)
{
    filterInput->setVisible(visible);
}

void PageNavBar::setAutoVisible(bool visible)
{
    autoCheck->setVisible(visible);
}
