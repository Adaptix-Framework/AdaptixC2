#ifndef ADAPTIXCLIENT_LISTENERSFEEDWIDGET_H
#define ADAPTIXCLIENT_LISTENERSFEEDWIDGET_H

#include <UI/Widgets/AbstractDock.h>
#include <Utils/CustomElements/ControlCard.h>
#include <main.h>

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QVariant>

class AdaptixWidget;

class ListenersFeedWidget : public QWidget
{
Q_OBJECT
    AdaptixWidget* m_adaptixWidget = nullptr;
    KDDockWidgets::QtWidgets::DockWidget* dockWidget = nullptr;

    ControlCardList* m_cardList       = nullptr;
    QLineEdit*       m_search         = nullptr;
    QComboBox*       m_protocolFilter = nullptr;
    QPushButton*     m_addBtn         = nullptr;

    QList<ListenerData> m_items;
    QString m_selectedName;

    void rebuildVisible();
    ControlCardData toCard(const ListenerData& l) const;
    bool matchesFilter(const ListenerData& l) const;
    ListenerData* findByName(const QString& name);
    const ListenerData* findByName(const QString& name) const;

public:
    explicit ListenersFeedWidget(AdaptixWidget* w);
    ~ListenersFeedWidget() override;

    KDDockWidgets::QtWidgets::DockWidget* dock();

    void SetUpdatesEnabled(bool enabled);
    void Clear();
    void AddListenerItem(const ListenerData& newListener);
    void EditListenerItem(const ListenerData& newListener);
    void RemoveListenerItem(const QString& listenerName);

    struct ListenerInfo {
        QString name;
        QString regName;
        QString tags;
        bool    valid = false;
    };
    ListenerInfo currentListenerInfo() const;

public Q_SLOTS:
    void onCreateListener();
    void onEditListener();
    void onRemoveListener();
    void onPauseListener();
    void onResumeListener();
    void onSetTag();
    void onGenerateAgent();
    void onCreateConnector();

private Q_SLOTS:
    void onSearchChanged(const QString& text);
    void onProtocolFilterChanged(int);
    void onCardPrimary(const QVariant& id);
    void onCardDelete(const QVariant& id);
    void onCardGenerate(const QVariant& id);
    void onCardDoubleClick(const QVariant& id);
    void onCardSelected(const QVariant& id);
    void onCardContextMenu(const QVariant& id, const QPoint& globalPos);
};

#endif
