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

/**
 * @brief Base class for all dock widgets with tab blinking support on updates.
 * 
 * AUTOMATIC NOTIFICATION:
 * - Enabled by default for QTableWidget, QTableView, QTreeWidget, QTreeView, QTextEdit, QPlainTextEdit
 * - Triggers automatically when rows are inserted into model or text changes
 * - 100ms debounce prevents too frequent triggers
 * - Notification clears when user scrolls to see new content (not just on tab switch)
 * 
 * DISABLE FOR SPECIFIC WIDGET:
 * @code
 *   // In widget constructor:
 *   setAutoNotifyEnabled(false);  // Completely disable auto-notifications
 * @endcode
 * 
 * TEMPORARY DISABLE (e.g., during bulk data loading):
 * @code
 *   void MyWidget::loadBulkData() {
 *       AutoNotifyGuard guard(this);  // Disables notifications
 *       // ... bulk loading ...
 *   }  // Notifications are re-enabled automatically
 * @endcode
 * 
 * MANUAL CALL (for custom widgets):
 * @code
 *   notifyNewContent();  // Highlights the tab if inactive
 * @endcode
 */
class DockTab : public QWidget {
Q_OBJECT
protected:
    KDDockWidgets::QtWidgets::DockWidget* dockWidget;
    bool m_autoNotifyEnabled = true;
    QTimer* m_debounceTimer = nullptr;
    
    QSet<int> m_newTableRows;
    int m_newTextPosition = -1;
    QSet<QAbstractItemView*> m_trackedViews;
    QSet<QAbstractScrollArea*> m_trackedTextEdits;

public:
    DockTab(const QString &tabName, const QString &projectName, const QString &icon = "") {
        dockWidget = new KDDockWidgets::QtWidgets::DockWidget(tabName + ":Dock-" + projectName, KDDockWidgets::DockWidgetOption_None, KDDockWidgets::LayoutSaverOption::None);
        dockWidget->setTitle(tabName);
        if (!icon.isEmpty())
            dockWidget->setIcon(QIcon(icon), KDDockWidgets::IconPlace::TabBar);
        
        connect(dockWidget, &KDDockWidgets::QtWidgets::DockWidget::isCurrentTabChanged,
                this, &DockTab::onCurrentTabChanged);
        
        QTimer::singleShot(0, this, &DockTab::setupAutoNotify);
    };

    ~DockTab() override { dockWidget->deleteLater(); };

    KDDockWidgets::QtWidgets::DockWidget* dock() { return this->dockWidget; };

    /// Enable/disable automatic notification for this widget
    void setAutoNotifyEnabled(bool enabled) { m_autoNotifyEnabled = enabled; }
    /// Check if automatic notification is enabled
    bool isAutoNotifyEnabled() const { return m_autoNotifyEnabled; }

    void notifyNewContent() {
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
        if (isCurrent && hasNewContent()) {
            QTimer::singleShot(150, this, &DockTab::checkNewContentVisibility);
        }
    }
    
    void setupAutoNotify() {
        connectChildWidgets(this);
    }
    
    void onTableRowsInserted(const QModelIndex &parent, int first, int last) {
        Q_UNUSED(parent)
        if (!m_autoNotifyEnabled) return;
        
        for (int i = first; i <= last; ++i) {
            m_newTableRows.insert(i);
        }
        triggerNotification();
    }
    
    void onTextChanged() {
        if (!m_autoNotifyEnabled) return;
        
        if (auto* textEdit = qobject_cast<QTextEdit*>(sender())) {
            m_newTextPosition = textEdit->document()->blockCount() - 1;
        } else if (auto* plainTextEdit = qobject_cast<QPlainTextEdit*>(sender())) {
            m_newTextPosition = plainTextEdit->document()->blockCount() - 1;
        }
        triggerNotification();
    }
    
    void triggerNotification() {
        if (!m_debounceTimer) {
            m_debounceTimer = new QTimer(this);
            m_debounceTimer->setSingleShot(true);
            connect(m_debounceTimer, &QTimer::timeout, this, [this]() {
                notifyNewContent();
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
        if (!hasNewContent()) return;
        
        if (!m_newTableRows.isEmpty()) {
            for (auto* view : m_trackedViews) {
                if (!view->isVisible()) continue;
                
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
        }
        
        if (m_newTextPosition >= 0) {
            for (auto* scrollArea : m_trackedTextEdits) {
                if (!scrollArea->isVisible()) continue;
                
                QScrollBar* vbar = scrollArea->verticalScrollBar();
                if (vbar && vbar->value() >= vbar->maximum() - 10) {
                    m_newTextPosition = -1;
                    break;
                }
            }
        }
        
        if (!hasNewContent()) {
            clearHighlight();
        }
    }

private:
    template<typename T>
    void connectItemView(T* view) {
        m_trackedViews.insert(view);
        if (auto* model = view->model()) {
            connect(model, &QAbstractItemModel::rowsInserted, 
                    this, &DockTab::onTableRowsInserted, Qt::UniqueConnection);
        }
        if (auto* vbar = view->verticalScrollBar()) {
            connect(vbar, &QScrollBar::valueChanged, 
                    this, &DockTab::onScroll, Qt::UniqueConnection);
        }
    }
    
    template<typename T, typename Signal>
    void connectTextEdit(T* edit, Signal signal) {
        m_trackedTextEdits.insert(edit);
        connect(edit, signal, this, &DockTab::onTextChanged, Qt::UniqueConnection);
        if (auto* vbar = edit->verticalScrollBar()) {
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

/// RAII class for temporarily disabling auto-notifications
/// Usage:
///   {
///       AutoNotifyGuard guard(this);  // disables notifications
///       // ... data loading ...
///   }  // notifications are re-enabled automatically
class AutoNotifyGuard {
public:
    explicit AutoNotifyGuard(DockTab* tab) : m_tab(tab), m_wasEnabled(tab->isAutoNotifyEnabled()) {
        m_tab->setAutoNotifyEnabled(false);
    }
    ~AutoNotifyGuard() {
        m_tab->setAutoNotifyEnabled(m_wasEnabled);
    }
    AutoNotifyGuard(const AutoNotifyGuard&) = delete;
    AutoNotifyGuard& operator=(const AutoNotifyGuard&) = delete;
private:
    DockTab* m_tab;
    bool m_wasEnabled;
};

#endif //ADAPTIXCLIENT_ABSTRACTDOCK_H
