#ifndef ADAPTIXCLIENT_CONTROLCARD_H
#define ADAPTIXCLIENT_CONTROLCARD_H

#include <QWidget>
#include <QScrollArea>
#include <QVariant>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>
#include <QList>
#include <QColor>

class QLabel;
class QPushButton;
class QGridLayout;
class QVBoxLayout;
class QHBoxLayout;
class QFrame;
class QResizeEvent;
class QMouseEvent;
class QContextMenuEvent;
class QPaintEvent;
class QEvent;
class QKeyEvent;
class QFocusEvent;

class ControlFieldTile : public QFrame
{
Q_OBJECT
public:
    explicit ControlFieldTile(QWidget* parent = nullptr);

    void setCaption(const QString& caption);
    void setValue(const QString& value);
    void setValueFont(const QFont& font);
    void setCaptionFont(const QFont& font);
    void setMonoValue(bool mono);
    void applyColors(const QColor& caption, const QColor& value, const QColor& panelBg, const QColor& panelBorder);
    void reflow(int maxValueWidth);
    void setExpanding(bool expanding);

    QString fullValue() const { return m_fullValue; }
    bool isEmpty() const { return m_fullValue.isEmpty(); }

private:
    QLabel* m_caption = nullptr;
    QLabel* m_value = nullptr;
    QString m_fullValue;
    bool m_mono = false;
};

class ControlCard : public QWidget
{
Q_OBJECT
public:
    enum PrimaryAction { ActionStart, ActionStop, ActionNone };

    enum BodyLayout {
        BodyTwoLine = 0,
        BodyThreeLine = 1
    };

    enum ContentStyle {
        StyleGeneric = 0,
        StyleListener = 1,
        StyleTunnel   = 2
    };

    explicit ControlCard(QWidget* parent = nullptr);

    void setCardId(const QVariant& id);
    QVariant cardId() const { return m_id; }
    QString cardKey() const { return m_key; }

    void setBodyLayout(BodyLayout layout);
    BodyLayout bodyLayout() const { return m_bodyLayout; }

    void setContentStyle(ContentStyle style);
    ContentStyle contentStyle() const { return m_contentStyle; }

    void setTitle(const QString& title);
    void setTitleSuffix(const QString& suffix);
    void setStatus(const QString& status, bool active);
    void setPrimary(const QString& text);
    void setDetail(const QString& text);
    void setSecondary(const QString& text);
    void setSecondaryLead(const QString& text);
    void setTertiary(const QString& text);
    void setSideText(const QString& text);
    void setDateText(const QString& text);
    void setPrimaryPrefix(const QString& prefix);
    void setDetailPrefix(const QString& prefix);
    void setSecondaryPrefix(const QString& prefix);
    void setTertiaryPrefix(const QString& prefix);
    void setDimmed(bool dimmed);
    bool isDimmed() const { return m_dimmed; }

    void setPrimaryAction(PrimaryAction action, const QString& label = QString());
    void setDeleteVisible(bool visible);
    void setDeleteLabel(const QString& label);
    void setGenerateVisible(bool visible);
    void setGenerateLabel(const QString& label);

    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }

    void applyTypography();
    void reflowText();

Q_SIGNALS:
    void primaryActionClicked(const QVariant& id);
    void deleteClicked(const QVariant& id);
    void generateClicked(const QVariant& id);
    void doubleClicked(const QVariant& id);
    void selected(const QVariant& id, Qt::KeyboardModifiers modifiers);
    void contextMenuRequested(const QVariant& id, const QPoint& globalPos);

protected:
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    bool event(QEvent* e) override;

private:
    void applyDimStyle();
    void applyContentStyle();
    void applyButtonStyles();
    void applyZoneChrome();
    void updateElidedTexts();
    void applyBodyVisibility();
    int  titleBandHeight() const;
    QColor styleAccent() const;
    QColor styleSurfaceTint(const QColor& bg, bool darkUi) const;
    QColor widgetBackdrop() const;

    QVariant m_id;
    QString  m_key;
    BodyLayout m_bodyLayout = BodyTwoLine;
    ContentStyle m_contentStyle = StyleGeneric;
    bool m_dimmed = false;
    bool m_selected = false;
    bool m_active = true;
    bool m_hover = false;

    QString m_fullTitle;
    QString m_fullTitleSuffix;
    QString m_fullPrimary;
    QString m_fullDetail;
    QString m_fullSecondary;
    QString m_fullSecondaryLead;
    QString m_fullTertiary;
    QString m_fullSide;
    QString m_fullDate;
    QString m_fullStatus;
    QString m_primaryPrefix;
    QString m_detailPrefix;
    QString m_secondaryPrefix;
    QString m_tertiaryPrefix;
    QString m_titleDrawn;
    QString m_titleSuffixDrawn;
    QString m_statusDrawn;

    QWidget* m_factsZone = nullptr;
    QWidget* m_bottomZone = nullptr;

    ControlFieldTile* m_primaryTile = nullptr;
    ControlFieldTile* m_detailTile  = nullptr;
    QFrame* m_tagChip   = nullptr;  // tags in a framed chip
    QLabel* m_sideLabel = nullptr;
    QLabel* m_dateLabel = nullptr;  // start date
    QWidget* m_factsSpacer = nullptr;

    QLabel* m_metaLead = nullptr;    // reg name
    QLabel* m_metaSep = nullptr;     // " | "
    QLabel* m_metaCaption = nullptr;
    QLabel* m_metaValue = nullptr;   // callback addresses
    QFrame* m_trafficChip = nullptr;
    QLabel* m_statsValue = nullptr;

    QPushButton* m_generateBtn = nullptr;
    QFrame*      m_actionSep   = nullptr;
    QPushButton* m_primaryBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;

    QVBoxLayout* m_rootLayout = nullptr;
    QHBoxLayout* m_factsLayout = nullptr;
    QHBoxLayout* m_bottomLayout = nullptr;
};

struct ControlCardData {
    QVariant id;
    QString  title;
    QString  titleSuffix;     // e.g. operator for client tunnels (" [name]", -1pt)
    QString  status;
    QString  primary;
    QString  detail;
    QString  secondary;       // callback addresses / agent meta
    QString  secondaryLead;   // reg name before CALLBACK
    QString  tertiary;        // traffic chip
    QString  sideText;        // tags next to BIND
    QString  dateText;        // listener start date
    QString  primaryPrefix;
    QString  detailPrefix;
    QString  secondaryPrefix; // "CALLBACK" styled like TYPE/BIND
    QString  tertiaryPrefix;
    bool     active = true;
    ControlCard::BodyLayout bodyLayout = ControlCard::BodyTwoLine;
    ControlCard::ContentStyle contentStyle = ControlCard::StyleGeneric;
    bool     showPrimaryAction = true;
    ControlCard::PrimaryAction primaryAction = ControlCard::ActionStop;
    QString  primaryActionLabel;
    bool     showDelete = true;
    QString  deleteActionLabel;
    bool     showGenerate = false;
    QString  generateActionLabel;

    QString key() const;
};

class ControlCardList : public QScrollArea
{
Q_OBJECT
public:
    explicit ControlCardList(QWidget* parent = nullptr);

    void clear();
    void setCards(const QVector<ControlCardData>& cards);
    void upsertCard(const ControlCardData& data);
    void removeCard(const QVariant& id);
    void setSelectedId(const QVariant& id);
    QVariant selectedId() const { return m_selectedId; }
    QList<QVariant> selectedIds() const;
    bool isIdSelected(const QVariant& id) const;
    void ensureSelected(const QVariant& id);
    int count() const { return m_cards.size(); }
    bool contains(const QVariant& id) const;

public Q_SLOTS:
    void applyTypography();

Q_SIGNALS:
    void primaryActionClicked(const QVariant& id);
    void deleteClicked(const QVariant& id);
    void generateClicked(const QVariant& id);
    void doubleClicked(const QVariant& id);
    void selectionChanged(const QVariant& id);
    void contextMenuRequested(const QVariant& id, const QPoint& globalPos);

protected:
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void focusInEvent(QFocusEvent* e) override;

private:
    void rebuildGrid();
    int columnCountForWidth(int w) const;
    ControlCard* makeCard(const ControlCardData& data);
    void applyData(ControlCard* card, const ControlCardData& data);
    void selectWithModifiers(const QVariant& id, Qt::KeyboardModifiers mods);
    void applySelectionVisuals();
    void moveCurrent(int delta, Qt::KeyboardModifiers mods);
    void ensureCardVisible(const QString& key);
    static QString keyOf(const QVariant& id);

    QWidget* m_host = nullptr;
    QGridLayout* m_grid = nullptr;
    QHash<QString, ControlCard*> m_cards;
    QHash<QString, ControlCardData> m_data;
    QList<QString> m_order;
    QVariant m_selectedId;       // primary / last interacted
    QSet<QString> m_selectedKeys;
    QString m_anchorKey;         // shift-range anchor
    QString m_currentKey;        // keyboard cursor
    int m_cols = 1;
};

#endif
