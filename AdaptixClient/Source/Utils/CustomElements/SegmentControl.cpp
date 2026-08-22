#include <Utils/CustomElements/SegmentControl.h>
#include <Utils/FontManager.h>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>

#include <QApplication>
#include <QEvent>
#include <QShowEvent>
#include <QSignalBlocker>

SegmentControl::SegmentControl(QWidget* parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("SegmentControl"));
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    m_layout = new QHBoxLayout(this);
    m_layout->setSpacing(2);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);
    connect(m_group, &QButtonGroup::idClicked, this, [this](int id) {
        if (id == m_currentIndex)
            return;
        m_currentIndex = id;
        Q_EMIT currentIndexChanged();
    });

    updateMetrics();
    applyTheme();
    connectThemeSignals();
}

void SegmentControl::connectThemeSignals()
{
    if (m_themeConn)
        disconnect(m_themeConn);

    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(style());
    if (!qs)
        qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    if (qs)
        m_themeConn = connect(qs, &oclero::qlementine::QlementineStyle::themeChanged, this, &SegmentControl::applyTheme, Qt::UniqueConnection);
}

void SegmentControl::changeEvent(QEvent* event)
{
    QFrame::changeEvent(event);
    if (!event || m_applyingTheme)
        return;

    if (event->type() == QEvent::StyleChange) {
        connectThemeSignals();
        updateMetrics();
        applyTheme();
    } else if (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange) {
        updateMetrics();
    }
}

void SegmentControl::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    if (m_applyingTheme)
        return;
    connectThemeSignals();
    applyTheme();
}

void SegmentControl::updateMetrics()
{
    const int ctrlH = FontManager::instance().typography().controlHeight;
    const int pad = 2;
    const int btnH = qMax(18, ctrlH - pad * 2);

    setFixedHeight(ctrlH);
    m_layout->setContentsMargins(pad, pad, pad, pad);

    for (auto* btn : m_buttons) {
        if (!btn)
            continue;
        btn->setFixedHeight(btnH);
        btn->setMinimumWidth(m_minButtonWidth);
    }
}

QPushButton* SegmentControl::makeButton(const QString& text)
{
    const int ctrlH = FontManager::instance().typography().controlHeight;
    const int btnH = qMax(18, ctrlH - 4);

    auto* btn = new QPushButton(text, this);
    btn->setCheckable(true);
    btn->setAutoExclusive(true);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedHeight(btnH);
    btn->setMinimumWidth(m_minButtonWidth);
    btn->setObjectName(QStringLiteral("SegmentControlBtn"));
    btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    return btn;
}

void SegmentControl::reindexButtons()
{
    if (!m_group)
        return;
    const auto buttons = m_group->buttons();
    for (auto* b : buttons)
        m_group->removeButton(b);
    for (int i = 0; i < m_buttons.size(); ++i) {
        if (m_buttons[i])
            m_group->addButton(m_buttons[i], i);
    }
}

int SegmentControl::addItem(const QString& text)
{
    auto* btn = makeButton(text);
    m_buttons.append(btn);
    m_layout->addWidget(btn);
    const int index = m_buttons.size() - 1;
    m_group->addButton(btn, index);

    applyTheme();

    if (m_currentIndex < 0) {
        m_currentIndex = 0;
        QSignalBlocker b(btn);
        btn->setChecked(true);
        Q_EMIT currentIndexChanged();
    }
    return index;
}

void SegmentControl::addItems(const QStringList& texts)
{
    for (const QString& t : texts)
        addItem(t);
}

void SegmentControl::removeItem(int index)
{
    if (index < 0 || index >= m_buttons.size())
        return;

    QPushButton* btn = m_buttons.takeAt(index);
    if (btn) {
        m_group->removeButton(btn);
        m_layout->removeWidget(btn);
        btn->deleteLater();
    }
    reindexButtons();

    int newIndex = m_currentIndex;
    if (m_buttons.isEmpty()) {
        newIndex = -1;
    } else if (m_currentIndex == index) {
        newIndex = qMin(index, m_buttons.size() - 1);
    } else if (m_currentIndex > index) {
        newIndex = m_currentIndex - 1;
    }

    if (newIndex != m_currentIndex) {
        m_currentIndex = newIndex;
        if (m_currentIndex >= 0 && m_currentIndex < m_buttons.size()) {
            QSignalBlocker b(m_buttons[m_currentIndex]);
            m_buttons[m_currentIndex]->setChecked(true);
        }
        Q_EMIT currentIndexChanged();
    } else if (m_currentIndex >= 0 && m_currentIndex < m_buttons.size()) {
        QSignalBlocker b(m_buttons[m_currentIndex]);
        m_buttons[m_currentIndex]->setChecked(true);
    }
}

void SegmentControl::clear()
{
    while (!m_buttons.isEmpty())
        removeItem(m_buttons.size() - 1);
}

void SegmentControl::setItemText(int index, const QString& text)
{
    if (index < 0 || index >= m_buttons.size() || !m_buttons[index])
        return;
    m_buttons[index]->setText(text);
}

QString SegmentControl::itemText(int index) const
{
    if (index < 0 || index >= m_buttons.size() || !m_buttons[index])
        return {};
    return m_buttons[index]->text();
}

void SegmentControl::setItemToolTip(int index, const QString& tip)
{
    if (index < 0 || index >= m_buttons.size() || !m_buttons[index])
        return;
    m_buttons[index]->setToolTip(tip);
}

void SegmentControl::setCurrentIndex(int index)
{
    if (m_buttons.isEmpty()) {
        if (m_currentIndex != -1) {
            m_currentIndex = -1;
            Q_EMIT currentIndexChanged();
        }
        return;
    }
    if (index < 0 || index >= m_buttons.size())
        return;
    if (index == m_currentIndex) {
        if (m_buttons[index] && !m_buttons[index]->isChecked()) {
            QSignalBlocker b(m_buttons[index]);
            m_buttons[index]->setChecked(true);
        }
        return;
    }

    m_currentIndex = index;
    if (m_buttons[index]) {
        QSignalBlocker b(m_buttons[index]);
        m_buttons[index]->setChecked(true);
    }
    Q_EMIT currentIndexChanged();
}

QString SegmentControl::currentText() const
{
    return itemText(m_currentIndex);
}

void SegmentControl::setMinimumButtonWidth(int width)
{
    m_minButtonWidth = qMax(0, width);
    for (auto* btn : m_buttons) {
        if (btn)
            btn->setMinimumWidth(m_minButtonWidth);
    }
}

void SegmentControl::applyTheme()
{
    if (m_applyingTheme)
        return;
    m_applyingTheme = true;

    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(style());
    if (!qs)
        qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    const auto& t = qs ? qs->theme() : oclero::qlementine::Theme();

    const QString trackBg  = t.borderColor.name(QColor::HexRgb);
    const QString selBg    = t.primaryColor.name(QColor::HexRgb);
    const QString selBgHov = t.primaryColorHovered.name(QColor::HexRgb);
    const QString selFg    = t.primaryColorForeground.name(QColor::HexRgb);
    const QString selFgHov = t.primaryColorForegroundHovered.name(QColor::HexRgb);
    const QString unselFg  = t.secondaryColorHovered.name(QColor::HexRgb);
    const QString hoverBg  = t.neutralColor.name(QColor::HexRgb);
    const QString unselBg  = QStringLiteral("rgba(%1,%2,%3,0.45)")
        .arg(t.neutralColor.red())
        .arg(t.neutralColor.green())
        .arg(t.neutralColor.blue());

    setStyleSheet(QStringLiteral(
        "QFrame#SegmentControl {"
        "  background: %1;"
        "  border: none;"
        "  border-radius: 6px;"
        "}"
        "QFrame#SegmentControl > QPushButton#SegmentControlBtn {"
        "  background: %2;"
        "  border: none;"
        "  border-radius: 4px;"
        "  color: %3;"
        "  font-weight: 600;"
        "  padding: 2px 12px;"
        "}"
        "QFrame#SegmentControl > QPushButton#SegmentControlBtn:hover:!checked {"
        "  background: %4;"
        "  color: %3;"
        "}"
        "QFrame#SegmentControl > QPushButton#SegmentControlBtn:checked {"
        "  background: %5;"
        "  color: %6;"
        "  font-weight: 600;"
        "}"
        "QFrame#SegmentControl > QPushButton#SegmentControlBtn:checked:hover {"
        "  background: %7;"
        "  color: %8;"
        "}"
        "QFrame#SegmentControl > QPushButton#SegmentControlBtn:disabled {"
        "  color: %9;"
        "}"
    ).arg(trackBg, unselBg, unselFg, hoverBg, selBg, selFg, selBgHov, selFgHov,
          t.secondaryColorDisabled.name(QColor::HexRgb)));

    m_applyingTheme = false;
}
