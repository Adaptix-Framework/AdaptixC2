#ifndef ADAPTIXCLIENT_ABSTRACTDOCK_H
#define ADAPTIXCLIENT_ABSTRACTDOCK_H

#include <kddockwidgets/qtwidgets/views/DockWidget.h>
#include <kddockwidgets/qtwidgets/views/MainWindow.h>
#include <kddockwidgets/qtwidgets/views/TabBar.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/core/Group.h>
#include <kddockwidgets/core/Stack.h>
#include <kddockwidgets/core/TabBar.h>
#include <kddockwidgets/core/Controller.h>
#include <kddockwidgets/qtcommon/View.h>
#include <QTableWidget>
#include <QTableView>
#include <QTreeWidget>
#include <QTreeView>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QAbstractItemModel>
#include <QTimer>
#include <QScrollBar>
#include <QSet>
#include <MainAdaptix.h>
#include <Client/Settings.h>
#include <typeinfo>
#include <cstdlib>
#ifdef __GNUC__
#include <cxxabi.h>
#endif

/**
 * @brief Base class for all dock widgets with tab blinking support on updates.
 *
 * AUTOMATIC BLINK:
 * - Enabled by default for QTableWidget, QTableView, QTreeWidget, QTreeView, QTextEdit, QPlainTextEdit
 * - Triggers automatically when rows are inserted into model or text changes
 * - 100ms debounce prevents too frequent triggers
 * - blink clears when user scrolls to see new content (not just on tab switch)
 *
 * DISABLE FOR SPECIFIC WIDGET:
 * @code
 *   // In widget constructor:
 *   setAutoBlinkEnabled(false);  // Completely disable auto-blinks
 * @endcode
 *
 * TEMPORARY DISABLE (e.g., during bulk data loading):
 * @code
 *   void MyWidget::loadBulkData() {
 *       AutoBlinkGuard guard(this);  // Disables blinks
 *       // ... bulk loading ...
 *   }  // Blinks are re-enabled automatically
 * @endcode
 *
 * MANUAL CALL (for custom widgets):
 * @code
 *   blinkNewContent();  // Highlights the tab if inactive
 * @endcode
 */

class DockTab : public QWidget {
Q_OBJECT
protected:
    KDDockWidgets::QtWidgets::DockWidget* dockWidget;
    mutable QString m_cachedClassName;
    bool m_autoBlinkEnabled = true;
    QTimer* m_debounceTimer = nullptr;

    QSet<int> m_newTableRows;
    int m_newTextPosition = -1;
    QSet<QAbstractItemView*> m_trackedViews;
    QSet<QAbstractScrollArea*> m_trackedTextEdits;

    QString getClassName() const {
        if (!m_cachedClassName.isEmpty())
            return m_cachedClassName;
#ifdef __GNUC__
        int status;
        char* demangled = abi::__cxa_demangle(typeid(*this).name(), nullptr, nullptr, &status);
        m_cachedClassName = (status == 0 && demangled) ? QString(demangled) : QString(typeid(*this).name());
        free(demangled);
#else
        m_cachedClassName = QString(typeid(*this).name());
#endif
        return m_cachedClassName;
    }

public:
    DockTab(const QString &tabName, const QString &projectName, const QString &icon = "") {
        dockWidget = new KDDockWidgets::QtWidgets::DockWidget(tabName + ":Dock-" + projectName, KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
        dockWidget->setTitle(tabName);
        if (!icon.isEmpty())
            dockWidget->setIcon(QIcon(icon), KDDockWidgets::IconPlace::TabBar);

        connect(dockWidget, &KDDockWidgets::QtWidgets::DockWidget::isCurrentTabChanged, this, &DockTab::onCurrentTabChanged);

        QTimer::singleShot(0, this, &DockTab::setupAutoBlink);
    };

    ~DockTab() override { dockWidget->deleteLater(); };

    KDDockWidgets::QtWidgets::DockWidget* dock() { return this->dockWidget; };

    void setAutoBlinkEnabled(bool enabled) { m_autoBlinkEnabled = enabled; }

    bool isAutoBlinkEnabled() const { return m_autoBlinkEnabled; }

    void blinkNewContent() {
        if (GlobalClient && GlobalClient->settings) {
            auto& data = GlobalClient->settings->data;
            if (!data.TabBlinkEnabled)
                return;

            QString className = getClassName();
            if (data.BlinkWidgets.contains(className) && !data.BlinkWidgets[className])
                return;
        }

        auto* coreDw = dockWidget->dockWidget();
        if (!coreDw) return;

        if (!coreDw->isCurrentTab()) {
            if (auto tabBar = getTabBar()) {
                int index = getTabIndex();
                if (index >= 0) {
                    tabBar->setTabHighlighted(index, true);
                }
            }
        }
    }

protected:
    KDDockWidgets::QtWidgets::TabBar* getTabBar() const {
        auto* group = dockWidget->group();
        if (!group) return nullptr;

        auto* stack = group->stack();
        if (!stack) return nullptr;

        auto* coreTabBar = stack->tabBar();
        if (!coreTabBar) return nullptr;

        return static_cast<KDDockWidgets::QtWidgets::TabBar*>(
            KDDockWidgets::QtCommon::View_qt::asQWidget(static_cast<KDDockWidgets::Core::Controller*>(coreTabBar))
        );
    }

    int getTabIndex() const {
        auto* coreDw = dockWidget->dockWidget();
        if (!coreDw) return -1;

        auto* group = dockWidget->group();
        if (!group) return -1;

        auto* stack = group->stack();
        if (!stack) return -1;

        return stack->tabBar()->indexOfDockWidget(coreDw);
    }

    void clearHighlight() {
        if (auto tabBar = getTabBar()) {
            int index = getTabIndex();
            if (index >= 0 && tabBar->isTabHighlighted(index)) {
                tabBar->setTabHighlighted(index, false);
            }
        }
        m_newTableRows.clear();
        m_newTextPosition = -1;
    }

    bool hasNewContent() const {
        return !m_newTableRows.isEmpty() || m_newTextPosition >= 0;
    }

private Q_SLOTS:
    void onCurrentTabChanged(bool isCurrent) {
        if (isCurrent) {
            clearHighlight();
        }
    }

    void setupAutoBlink() {
        connectChildWidgets(this);
    }

    void onTableRowsInserted(const QModelIndex &parent, int first, int last) {
        Q_UNUSED(parent)
        if (!m_autoBlinkEnabled) return;

        for (int i = first; i <= last; ++i) {
            m_newTableRows.insert(i);
        }
        triggerBlink();
    }

    void onTextChanged() {
        if (!m_autoBlinkEnabled) return;

        if (auto* textEdit = qobject_cast<QTextEdit*>(sender())) {
            m_newTextPosition = textEdit->document()->blockCount() - 1;
        } else if (auto* plainTextEdit = qobject_cast<QPlainTextEdit*>(sender())) {
            m_newTextPosition = plainTextEdit->document()->blockCount() - 1;
        }
        triggerBlink();
    }

    void triggerBlink() {
        if (!m_debounceTimer) {
            m_debounceTimer = new QTimer(this);
            m_debounceTimer->setSingleShot(true);
            connect(m_debounceTimer, &QTimer::timeout, this, [this]() {
                blinkNewContent();
            });
        }

        if (!m_debounceTimer->isActive()) {
            m_debounceTimer->start(100);
        }
    }

    void onScroll() {
        checkNewContentVisibility();
    }

    void checkNewContentVisibility() {
        auto* coreDw = dockWidget->dockWidget();
        if (!coreDw || !coreDw->isCurrentTab())
            return;

        if (!hasNewContent()) {
            clearHighlight();
            return;
        }

        bool hasVisibleViews = false;

        if (!m_newTableRows.isEmpty()) {
            for (auto* view : m_trackedViews) {
                if (!view->isVisible()) continue;
                hasVisibleViews = true;

                QSet<int> stillHidden;
                for (int row : m_newTableRows) {
                    QModelIndex idx = view->model()->index(row, 0);
                    QRect rect = view->visualRect(idx);
                    if (!view->viewport()->rect().intersects(rect)) {
                        stillHidden.insert(row);
                    }
                }
                m_newTableRows = stillHidden;
            }

            if (!hasVisibleViews && !m_trackedViews.isEmpty()) {
                m_newTableRows.clear();
            }
        }

        if (m_newTextPosition >= 0) {
            bool hasVisibleEdits = false;
            for (auto* scrollArea : m_trackedTextEdits) {
                if (!scrollArea->isVisible()) continue;
                hasVisibleEdits = true;

                QScrollBar* vbar = scrollArea->verticalScrollBar();
                if (vbar && vbar->value() >= vbar->maximum() - 10) {
                    m_newTextPosition = -1;
                    break;
                }
            }

            if (!hasVisibleEdits && !m_trackedTextEdits.isEmpty()) {
                m_newTextPosition = -1;
            }
        }

        if (!hasNewContent()) {
            clearHighlight();
        }
    }

private:
    template<typename T>
    void connectItemView(T* view) {
        if (!view) return;
        m_trackedViews.insert(view);
        auto* model = view->model();
        if (model && model->metaObject()) {
            connect(model, &QAbstractItemModel::rowsInserted,
                    this, &DockTab::onTableRowsInserted, Qt::UniqueConnection);
        }
        auto* vbar = view->verticalScrollBar();
        if (vbar && vbar->metaObject()) {
            connect(vbar, &QScrollBar::valueChanged,
                    this, &DockTab::onScroll, Qt::UniqueConnection);
        }
    }

    template<typename T, typename Signal>
    void connectTextEdit(T* edit, Signal signal) {
        if (!edit || !edit->metaObject()) return;
        m_trackedTextEdits.insert(edit);
        connect(edit, signal, this, &DockTab::onTextChanged, Qt::UniqueConnection);
        auto* vbar = edit->verticalScrollBar();
        if (vbar && vbar->metaObject()) {
            connect(vbar, &QScrollBar::valueChanged,
                    this, &DockTab::onScroll, Qt::UniqueConnection);
        }
    }

    void connectChildWidgets(QWidget* parent) {
        for (auto* w : parent->findChildren<QTableWidget*>())
            connectItemView(w);

        for (auto* w : parent->findChildren<QTableView*>())
            if (!qobject_cast<QTableWidget*>(w)) connectItemView(w);

        for (auto* w : parent->findChildren<QTreeWidget*>())
            connectItemView(w);

        for (auto* w : parent->findChildren<QTreeView*>())
            if (!qobject_cast<QTreeWidget*>(w)) connectItemView(w);

        for (auto* w : parent->findChildren<QTextEdit*>())
            connectTextEdit(w, &QTextEdit::textChanged);

        for (auto* w : parent->findChildren<QPlainTextEdit*>())
            connectTextEdit(w, &QPlainTextEdit::textChanged);
    }
};



/// RAII class for temporarily disabling auto-blinks
/// Usage:
///   {
///       AutoBlinkGuard guard(this);  // disables blinks
///       // ... data loading ...
///   }  // blinks are re-enabled automatically
class AutoBlinkGuard {
public:
    explicit AutoBlinkGuard(DockTab* tab) : m_tab(tab), m_wasEnabled(tab->isAutoBlinkEnabled()) {
        m_tab->setAutoBlinkEnabled(false);
    }
    ~AutoBlinkGuard() {
        m_tab->setAutoBlinkEnabled(m_wasEnabled);
    }
    AutoBlinkGuard(const AutoBlinkGuard&) = delete;
    AutoBlinkGuard& operator=(const AutoBlinkGuard&) = delete;
private:
    DockTab* m_tab;
    bool m_wasEnabled;
};

#endif //ADAPTIXCLIENT_ABSTRACTDOCK_H