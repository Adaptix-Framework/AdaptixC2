#ifndef AXELEMENTWRAPPERS_H
#define AXELEMENTWRAPPERS_H

#include <UI/Widgets/AbstractDock.h>

#include <QVariant>
#include <QJSValue>
#include <QBoxLayout>
#include <QGridLayout>
#include <QSpacerItem>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QTimeEdit>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QTabWidget>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <Utils/CustomElements.h>
#include <QAction>
#include <QPointer>
#include <QTableView>
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

class AxScriptEngine;
class AdaptixWidget;

inline const QMap<QString, QString> FIELD_MAP_CREDS = {
    {"username", "Username"},
    {"password", "Password"},
    {"realm",    "Realm"},
    {"type",     "Type"},
    {"tag",      "Tag"},
    {"date",     "Date"},
    {"storage",  "Storage"},
    {"agent_id", "Agent"},
    {"host",     "Host"}
};

inline const QMap<QString, QString> FIELD_MAP_AGENTS = {
    {"id",          "Agent ID"},
    {"type",        "Type"},
    {"listener",    "Listener"},
    {"external_ip", "External"},
    {"internal_ip", "Internal"},
    {"domain",      "Domain"},
    {"computer",    "Computer"},
    {"username",    "Username"},
    {"process",     "Process"},
    {"pid",         "PID"},
    {"tid",         "TID"},
    {"os",          "OS"},
    {"tags",        "Tags"},
};

inline const QMap<QString, QString> FIELD_MAP_LISTENERS = {
    {"name",       "Name"},
    {"type",       "Type"},
    {"protocol",   "Protocol"},
    {"bind_host",  "Bind Host"},
    {"bind_port",  "Bind Port"},
    {"agent_addr", "Agent Addresses"},
    {"status",     "Status"},
    {"date",       "Date"},
};

inline const QMap<QString, QString> FIELD_MAP_TARGETS = {
    {"id",       "Target ID"},
    {"computer", "Computer"},
    {"domain",   "Domain"},
    {"address",  "Address"},
    {"tag",      "Tag"},
    {"os",       "OS"},
    {"os_desc",  "OS Description"},
    {"info",     "Info"},
    {"date",     "Date"},
    {"alive",    "Alive"},
};

inline const QMap<QString, QString> FIELD_MAP_DOWNLOADS = {
    {"id",         "File ID"},
    {"agent_id",   "Agent ID"},
    {"agent_name", "Agent"},
    {"user",       "User"},
    {"computer",   "Computer"},
    {"filename",   "Filename"},
    {"total_size", "Total Size"},
    {"recv_size",  "Received"},
    {"state",      "State"},
    {"date",       "Date"},
};

/// ABSTRACT

class AbstractAxLayout {
public:
    virtual ~AbstractAxLayout() = default;
    virtual QLayout* layout() const = 0;
};


class AbstractAxMenuItem : public QObject {
Q_OBJECT
public:
    explicit AbstractAxMenuItem(QObject* parent = nullptr) : QObject(parent) {}
    virtual void setContext(QVariantList context) = 0;
};


class AbstractAxElement {
public:
    virtual ~AbstractAxElement() = default;
    virtual QVariant jsonMarshal() const = 0;
    virtual void jsonUnmarshal(const QVariant& value) = 0;
};


class AbstractAxSelector {
public:
    // virtual ~AbstractAxElement() = default;
    virtual QJSValue selected_data() const = 0;
};


class AbstractAxVisualElement {
public:
    virtual ~AbstractAxVisualElement() = default;

    virtual QWidget* widget() const = 0;
    virtual void setEnabled(const bool enable) const = 0;
    virtual void setVisible(const bool enable) const = 0;
    virtual bool getEnabled() const = 0;
    virtual bool getVisible() const = 0;
};


// /// BYTES
//
// class AxByteArrayWrapper : public QObject {
// Q_OBJECT
//     QByteArray data;
//
// public:
//     explicit AxByteArrayWrapper(QString str, QObject* parent = nullptr);
//
//     Q_INVOKABLE QString toHex() const;
//     Q_INVOKABLE QString toBase64() const;
//     Q_INVOKABLE QString toString() const;
//     Q_INVOKABLE QVariantList toIntArray() const;
//     Q_INVOKABLE int at(int index) const;
//     Q_INVOKABLE void clear();
//     Q_INVOKABLE int size() const;
//     Q_INVOKABLE bool contains(QObject* other) const;
//
//     Q_INVOKABLE int compare(QObject* other) const;
//     Q_INVOKABLE void trimmed();
//     Q_INVOKABLE void prepend(QObject*  value);
//     Q_INVOKABLE void append(QObject*  value);
//     Q_INVOKABLE void insert(int index, QObject* value);
//     Q_INVOKABLE AxByteArrayWrapper* slice(int start, int length = -1) const;
// };



// MENU

class AxActionWrapper : public AbstractAxMenuItem {
Q_OBJECT
    QJSValue handler;
    QPointer<QAction> pAction;
    QJSEngine* engine;

public:
    explicit AxActionWrapper(const QString& text, const QJSValue& handler, QJSEngine* engine, QObject* parent = nullptr);
    QAction* action() const;
    void setContext(QVariantList context) override;

    void triggerWithContext(const QVariantList& arg) const;
};


class AxSeparatorWrapper : public AbstractAxMenuItem {
Q_OBJECT
    QPointer<QAction> pAction;

public:
    explicit AxSeparatorWrapper(QObject* parent = nullptr);
    QAction* action() const;
    void setContext(QVariantList context) override;
};


class AxMenuWrapper : public AbstractAxMenuItem {
Q_OBJECT
    QPointer<QMenu> pMenu;
    QList<AbstractAxMenuItem*> items;

public:
    explicit AxMenuWrapper(const QString& title, QObject* parent = nullptr);
    QMenu* menu() const;
    void setContext(QVariantList context) override;

    Q_INVOKABLE void addItem(AbstractAxMenuItem* item);
};



/// LAYOUT

class AxBoxLayoutWrapper : public QObject, public AbstractAxLayout {
Q_OBJECT
    QBoxLayout* boxLayout;

public:
    explicit AxBoxLayoutWrapper(QBoxLayout::Direction dir, QObject* parent = nullptr);

    QBoxLayout* layout() const override;

    Q_INVOKABLE void addWidget(QObject* widgetWrapper) const;
};



class AxGridLayoutWrapper : public QObject, public AbstractAxLayout {
Q_OBJECT
    QGridLayout* gridLayout;

public:
    explicit AxGridLayoutWrapper(QObject* parent = nullptr);

    QGridLayout* layout() const override;

    Q_INVOKABLE void addWidget(QObject* widgetWrapper, int row, int col, int rowSpan = 1, int colSpan = 1) const;
};



/// LINE

class AxLineWrapper : public QObject, public AbstractAxVisualElement {
Q_OBJECT
    QFrame* line;

public:
    explicit AxLineWrapper(QFrame::Shape dir, QObject* parent = nullptr);

    QFrame* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }
};



/// SPACER

class AxSpacerWrapper : public QObject {
Q_OBJECT
    QSpacerItem* spacer;

public:
    explicit AxSpacerWrapper(int w, int h, QSizePolicy::Policy hData, QSizePolicy::Policy vData, QObject* parent = nullptr);

    QSpacerItem* widget() const;
};



/// TEXTLINE

class AxTextLineWrapper : public QObject, public AbstractAxElement, public AbstractAxVisualElement {
Q_OBJECT
    QLineEdit* lineedit;

public:
    explicit AxTextLineWrapper(QLineEdit* edit, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    QLineEdit* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE QString text() const;
    Q_INVOKABLE void    setText(const QString& text) const;
    Q_INVOKABLE void    setPlaceholder(const QString& text) const;
    Q_INVOKABLE void    setReadOnly(const bool& readonly) const;

Q_SIGNALS:
    void textChanged(const QString &text);
    void textEdited(const QString &text);
    void returnPressed();
    void editingFinished();
};



/// COMBO

class AxComboBoxWrapper : public QObject, public AbstractAxElement, public AbstractAxVisualElement {
Q_OBJECT
    QComboBox* comboBox;

public:
    explicit AxComboBoxWrapper(QComboBox* comboBox, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    QComboBox* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE void    addItem(const QString& text) const;
    Q_INVOKABLE void    addItems(const QJSValue& array) const;
    Q_INVOKABLE void    setItems(const QJSValue& array) const;
    Q_INVOKABLE void    clear() const;
    Q_INVOKABLE QString currentText() const;
    Q_INVOKABLE void    setCurrentIndex(int index) const;
    Q_INVOKABLE int     currentIndex() const;

Q_SIGNALS:
    void currentTextChanged(const QString &text);
    void currentIndexChanged(int index);
};



/// SPIN

class AxSpinBoxWrapper : public QObject, public AbstractAxElement, public AbstractAxVisualElement {
Q_OBJECT
    QSpinBox* spin;

public:
    explicit AxSpinBoxWrapper(QSpinBox* spin, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    QSpinBox* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE int  value() const;
    Q_INVOKABLE void setValue(int value) const;
    Q_INVOKABLE void setRange(int min, int max) const;

Q_SIGNALS:
    void valueChanged(int);
};



/// DATE

class AxDateEditWrapper : public QObject, public AbstractAxElement, public AbstractAxVisualElement {
Q_OBJECT
    QDateEdit* dateedit;

public:
    explicit AxDateEditWrapper(QDateEdit* edit, const QString &format, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    QDateEdit* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE QString dateString() const;
    Q_INVOKABLE void    setDateString(const QString& date) const;
};



/// TEXTEDIT

class AxTimeEditWrapper : public QObject, public AbstractAxElement, public AbstractAxVisualElement {
Q_OBJECT
    QTimeEdit* timeedit;

public:
    explicit AxTimeEditWrapper(QTimeEdit* edit, const QString &format, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    QTimeEdit* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE QString timeString() const;
    Q_INVOKABLE void    setTimeString(const QString& time) const;
};



/// TEXTMULTI

class AxTextMultiWrapper : public QObject, public AbstractAxElement, public AbstractAxVisualElement {
Q_OBJECT
    QPlainTextEdit* textedit;

public:
    explicit AxTextMultiWrapper(QPlainTextEdit* edit, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    QPlainTextEdit* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE QString text() const;
    Q_INVOKABLE void    setText(const QString& text) const;
    Q_INVOKABLE void    appendText(const QString& text) const;
    Q_INVOKABLE void    setPlaceholder(const QString& text) const;
    Q_INVOKABLE void    setReadOnly(const bool& readonly) const;
};



/// CHECK

class AxCheckBoxWrapper : public QObject, public AbstractAxElement, public AbstractAxVisualElement {
Q_OBJECT
    QCheckBox* check;

public:
    explicit AxCheckBoxWrapper(QCheckBox* box, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    QCheckBox* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE bool isChecked() const;
    Q_INVOKABLE void setChecked(bool checked) const;

Q_SIGNALS:
    void stateChanged();
};



/// LABEL

class AxLabelWrapper : public QObject, public AbstractAxVisualElement {
Q_OBJECT
    QLabel* label;

public:
    explicit AxLabelWrapper(QLabel* label, QObject* parent = nullptr);

    QLabel* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE void setText(const QString& text) const;
    Q_INVOKABLE QString text() const;
};



/// TAB

class AxTabWrapper : public QObject, public AbstractAxVisualElement {
Q_OBJECT
    QTabWidget* tabs;

public:
    explicit AxTabWrapper(QTabWidget* tabs, QObject* parent = nullptr);

    QTabWidget* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE void addTab(QObject* wrapper, const QString &title) const;
};



/// TABLE

class AxTableWidgetWrapper : public QObject, public AbstractAxElement, public AbstractAxVisualElement {
Q_OBJECT
public:
    QTableWidget* table;
    QJSEngine*    engine;

    explicit AxTableWidgetWrapper(const QJSValue &headers, QTableWidget* tableWidget, QJSEngine* jsEngine, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    QTableWidget* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE void     addColumn(const QString &header) const;
    Q_INVOKABLE void     setColumns(const QJSValue &headers) const;
    Q_INVOKABLE void     addItem(const QJSValue &items) const;
    Q_INVOKABLE int      rowCount() const;
    Q_INVOKABLE int      columnCount() const;
    Q_INVOKABLE void     setRowCount(int rows);
    Q_INVOKABLE void     setColumnCount(int cols);
    Q_INVOKABLE int      currentRow() const;
    Q_INVOKABLE int      currentColumn() const;
    Q_INVOKABLE void     setSortingEnabled(bool enable);
    Q_INVOKABLE void     resizeToContent(int column);
    Q_INVOKABLE QString  text(int row, int column) const;
    Q_INVOKABLE void     setText(int row, int column, const QString &text) const;
    Q_INVOKABLE void     setReadOnly(bool read);
    Q_INVOKABLE void     hideColumn(int column);
    Q_INVOKABLE void     setHeadersVisible(bool enable);
    Q_INVOKABLE void     setColumnAlign(int column, const QString &align);
    Q_INVOKABLE void     clear();
    Q_INVOKABLE QJSValue selectedRows();

Q_SIGNALS:
    void cellChanged(int row, int column);
    void cellClicked(int row, int column);
    void cellDoubleClicked(int row, int column);
};



/// LIST

class AxListWidgetWrapper : public QObject, public AbstractAxElement, public AbstractAxVisualElement {
Q_OBJECT
    QListWidget* list;
    QJSEngine*   engine;

    bool readonly = false;

public:
    explicit AxListWidgetWrapper(QListWidget* widget, QJSEngine* engine, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    QListWidget* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE QJSValue items();
    Q_INVOKABLE void     addItem(const QString& text);
    Q_INVOKABLE void     addItems(const QJSValue &items);
    Q_INVOKABLE void     removeItem(int index);
    Q_INVOKABLE QString  itemText(int index) const;
    Q_INVOKABLE void     setItemText(int index, const QString& text);
    Q_INVOKABLE void     clear();
    Q_INVOKABLE int      count() const;
    Q_INVOKABLE int      currentRow() const;
    Q_INVOKABLE void     setCurrentRow(int row);
    Q_INVOKABLE QJSValue selectedRows() const;
    Q_INVOKABLE void     setReadOnly(bool readonly);

Q_SIGNALS:
    void currentTextChanged(const QString &currentText);
    void currentRowChanged(int currentRow);
    void itemClickedText(const QString& text);
    void itemDoubleClickedText(const QString& text);
};



/// BUTTON

class AxButtonWrapper : public QObject, public AbstractAxVisualElement {
Q_OBJECT
    QPushButton* button;

public:
    explicit AxButtonWrapper(QPushButton* btn, QObject* parent = nullptr);

    QPushButton* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

Q_SIGNALS:
    void clicked();
};



/// GROUPBOX


class AxGroupBoxWrapper : public QObject, public AbstractAxElement, public AbstractAxVisualElement {
Q_OBJECT
    QGroupBox*   groupBox;

public:
    explicit AxGroupBoxWrapper(const bool checkable, QGroupBox* box, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    QGroupBox* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE void setTitle(const QString& title);
    Q_INVOKABLE bool isCheckable() const;
    Q_INVOKABLE void setCheckable(bool checkable);
    Q_INVOKABLE bool isChecked() const;
    Q_INVOKABLE void setChecked(bool checked);
    Q_INVOKABLE void setPanel(QObject* panel) const;

Q_SIGNALS:
    void clicked(bool checked = false);
};



/// SCROLLAREA

class AxScrollAreaWrapper : public QObject, public AbstractAxVisualElement {
Q_OBJECT
    QScrollArea* scrollArea;

public:
    explicit AxScrollAreaWrapper(QScrollArea* area, QObject* parent = nullptr);

    QScrollArea* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE void setPanel(QObject* panel) const;
    Q_INVOKABLE void setWidgetResizable(bool resizable);
};


/// SPLITTER

class AxSplitterWrapper : public QObject, public AbstractAxVisualElement {
Q_OBJECT
    QSplitter* splitter;

public:
    explicit AxSplitterWrapper(QSplitter* splitter, QObject* parent = nullptr);

    QSplitter* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE void addPage(QObject* w);
    Q_INVOKABLE void setSizes(const QVariantList& sizes);

Q_SIGNALS:
    void splitterMoved(int pos, int index);
};



/// STACK

class AxStackedWidgetWrapper : public QObject, public AbstractAxVisualElement {
Q_OBJECT
    QStackedWidget* stack;

public:
    explicit AxStackedWidgetWrapper(QStackedWidget* widget, QObject* parent = nullptr);

    QStackedWidget* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE int  addPage(QObject* page);
    Q_INVOKABLE int  insertPage(int index, QObject* page);
    Q_INVOKABLE void removePage(int index);
    Q_INVOKABLE void setCurrentIndex(int index);
    Q_INVOKABLE int  currentIndex() const;
    Q_INVOKABLE int  count() const;

Q_SIGNALS:
    void currentChanged(int index);
};



/// PANEL

class AxPanelWrapper : public QObject, public AbstractAxVisualElement {
Q_OBJECT
    QWidget* panel;

public:
    explicit AxPanelWrapper(QWidget* w, QObject* parent = nullptr);

    QWidget* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE void setLayout(QObject* layoutWrapper) const;
};



/// CONTAINER

class AxContainerWrapper : public QObject {
Q_OBJECT
    QJSEngine* engine;
    QMap<QString, QObject*> widgets;

public:
    explicit AxContainerWrapper(QJSEngine* jsEngine, QObject* parent = nullptr);

    Q_INVOKABLE void put(const QString& id, QObject* wrapper);
    Q_INVOKABLE QObject* get(const QString& id);
    Q_INVOKABLE bool contains(const QString& id) const;
    Q_INVOKABLE void remove(const QString& id);
    Q_INVOKABLE QString toJson();
    Q_INVOKABLE void fromJson(const QString& jsonString);
    Q_INVOKABLE QJSValue toProperty();
    Q_INVOKABLE void fromProperty(const QJSValue& obj);
};



/// DIALOG

class AxDialogWrapper : public QObject {
Q_OBJECT
    QDialog* dialog;
    QVBoxLayout* layout;
    QLayout* userLayout = nullptr;
    QDialogButtonBox* buttons;

public:
    explicit AxDialogWrapper(const QString& title, QWidget* parent = nullptr);

    Q_INVOKABLE void setLayout(QObject* layoutWrapper);
    Q_INVOKABLE void setSize(int w, int h) const;
    Q_INVOKABLE bool exec() const;
    Q_INVOKABLE void close() const;
    Q_INVOKABLE void setButtonsText(const QString& ok_text, const QString& cancel_text) const;
};



/// SELECTOR FILE

class AxSelectorFile : public QObject, public AbstractAxElement, public AbstractAxVisualElement {
Q_OBJECT
    QLineEdit* lineEdit;
    QString    content;

public:
    explicit AxSelectorFile(QLineEdit* edit, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    QLineEdit* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE void setPlaceholder(const QString& text) const;

private Q_SLOTS:
    void onSelectFile();
};



/// SELECTOR CREDENTIALS

class AxCredsTableModel : public QAbstractTableModel {
Q_OBJECT
    QVector<CredentialData> m_data;
    QVector<QString> m_headers;
    QVector<QString> m_fieldKeys;

public:
    explicit AxCredsTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void setHeaders(const QVector<QString>& headers, const QVector<QString>& fieldKeys) {
        beginResetModel();
        m_headers = headers;
        m_fieldKeys = fieldKeys;
        endResetModel();
    }

    void setData(const QVector<CredentialData>& data) {
        beginResetModel();
        m_data = data;
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_data.size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_headers.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_data.size() || index.column() >= m_fieldKeys.size())
            return QVariant();

        if (role == Qt::DisplayRole || role == Qt::UserRole) {
            const auto& cred = m_data[index.row()];
            const QString& key = m_fieldKeys[index.column()];
            if (key == "id")       return cred.CredId;
            if (key == "username") return cred.Username;
            if (key == "password") return cred.Password;
            if (key == "realm")    return cred.Realm;
            if (key == "type")     return cred.Type;
            if (key == "tag")      return cred.Tag;
            if (key == "date")     return cred.Date;
            if (key == "storage")  return cred.Storage;
            if (key == "agent_id") return cred.AgentId;
            if (key == "host")     return cred.Host;
        }
        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section < m_headers.size())
            return m_headers[section];
        return QVariant();
    }

    CredentialData getCredential(int row) const {
        if (row >= 0 && row < m_data.size())
            return m_data[row];
        return CredentialData();
    }
};

class AxCredsFilterProxyModel : public QSortFilterProxyModel {
Q_OBJECT
    QString m_filterText;

public:
    explicit AxCredsFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setFilterText(const QString& text) {
        m_filterText = text;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        if (m_filterText.isEmpty())
            return true;

        for (int col = 0; col < sourceModel()->columnCount(); ++col) {
            QModelIndex idx = sourceModel()->index(sourceRow, col, sourceParent);
            if (idx.data().toString().contains(m_filterText, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }
};

class AxDialogCreds : public QDialog {
Q_OBJECT
    QVBoxLayout*  mainLayout   = nullptr;
    QTableView*   tableView    = nullptr;
    QHBoxLayout*  bottomLayout = nullptr;
    QPushButton*  chooseButton = nullptr;
    QSpacerItem*  spacer_1     = nullptr;
    QSpacerItem*  spacer_2     = nullptr;

    QWidget*        searchWidget   = nullptr;
    QHBoxLayout*    searchLayout   = nullptr;
    QLineEdit*      searchLineEdit = nullptr;
    ClickableLabel* hideButton     = nullptr;

    AxCredsTableModel*       tableModel = nullptr;
    AxCredsFilterProxyModel* proxyModel = nullptr;

    QVector<CredentialData> selectedData;

public:
    explicit AxDialogCreds(const QJSValue &headers, QVector<CredentialData> vecCreds, QWidget* parent = nullptr);

    QVector<CredentialData> data();

public Q_SLOTS:
    void onClicked();
    void handleSearch();
    void clearSearch();
};

class AxSelectorCreds : public QObject {
Q_OBJECT
    AxDialogCreds*  dialog;
    AxScriptEngine* scriptEngine;

public:
    explicit AxSelectorCreds(const QJSValue &headers, AxScriptEngine* jsEngine, QWidget* parent = nullptr);

    Q_INVOKABLE void     setSize(int w, int h) const;
    Q_INVOKABLE QJSValue exec() const;
    Q_INVOKABLE void     close() const;
};



/// SELECTOR AGENTS

class AxAgentsTableModel : public QAbstractTableModel {
Q_OBJECT
    QVector<AgentData> m_data;
    QVector<QString> m_headers;
    QVector<QString> m_fieldKeys;

public:
    explicit AxAgentsTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void setHeaders(const QVector<QString>& headers, const QVector<QString>& fieldKeys) {
        beginResetModel();
        m_headers = headers;
        m_fieldKeys = fieldKeys;
        endResetModel();
    }

    void setData(const QVector<AgentData>& data) {
        beginResetModel();
        m_data = data;
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_data.size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_headers.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_data.size() || index.column() >= m_fieldKeys.size())
            return QVariant();

        if (role == Qt::DisplayRole || role == Qt::UserRole) {
            const auto& agent = m_data[index.row()];
            const QString& key = m_fieldKeys[index.column()];

            QString username = agent.Username;
            if (agent.Elevated)
                username = "* " + username;
            if (!agent.Impersonated.isEmpty())
                username += " [" + agent.Impersonated + "]";

            QString process = QString("%1 (%2)").arg(agent.Process).arg(agent.Arch);

            QString os = "unknown";
            if (agent.Os == OS_WINDOWS)    os = "windows";
            else if (agent.Os == OS_LINUX) os = "linux";
            else if (agent.Os == OS_MAC)   os = "macos";

            if (key == "id")          return agent.Id;
            if (key == "type")        return agent.Name;
            if (key == "listener")    return agent.Listener;
            if (key == "external_ip") return agent.ExternalIP;
            if (key == "internal_ip") return agent.InternalIP;
            if (key == "domain")      return agent.Domain;
            if (key == "computer")    return agent.Computer;
            if (key == "username")    return username;
            if (key == "process")     return process;
            if (key == "pid")         return agent.Pid;
            if (key == "tid")         return agent.Tid;
            if (key == "os")          return os;
            if (key == "tags")        return agent.Tags;
        }
        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section < m_headers.size())
            return m_headers[section];
        return QVariant();
    }

    AgentData getAgent(int row) const {
        if (row >= 0 && row < m_data.size())
            return m_data[row];
        return AgentData();
    }
};

class AxAgentsFilterProxyModel : public QSortFilterProxyModel {
Q_OBJECT
    QString m_filterText;

public:
    explicit AxAgentsFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setFilterText(const QString& text) {
        m_filterText = text;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        if (m_filterText.isEmpty())
            return true;

        for (int col = 0; col < sourceModel()->columnCount(); ++col) {
            QModelIndex idx = sourceModel()->index(sourceRow, col, sourceParent);
            if (idx.data().toString().contains(m_filterText, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }
};

class AxDialogAgents : public QDialog {
Q_OBJECT
    QVBoxLayout*  mainLayout   = nullptr;
    QTableView*   tableView    = nullptr;
    QHBoxLayout*  bottomLayout = nullptr;
    QPushButton*  chooseButton = nullptr;
    QSpacerItem*  spacer_1     = nullptr;
    QSpacerItem*  spacer_2     = nullptr;

    QWidget*        searchWidget   = nullptr;
    QHBoxLayout*    searchLayout   = nullptr;
    QLineEdit*      searchLineEdit = nullptr;
    ClickableLabel* hideButton     = nullptr;

    AxAgentsTableModel*       tableModel = nullptr;
    AxAgentsFilterProxyModel* proxyModel = nullptr;

    QVector<AgentData> selectedData;

public:
    explicit AxDialogAgents(const QJSValue &headers, QVector<AgentData> vecAgents, QWidget* parent = nullptr);

    QVector<AgentData> data();

public Q_SLOTS:
    void onClicked();
    void handleSearch();
    void clearSearch();
};

class AxSelectorAgents : public QObject {
Q_OBJECT
    AxDialogAgents*  dialog;
    AxScriptEngine* scriptEngine;

public:
    explicit AxSelectorAgents(const QJSValue &headers, AxScriptEngine* jsEngine, QWidget* parent = nullptr);

    Q_INVOKABLE void     setSize(int w, int h) const;
    Q_INVOKABLE QJSValue exec() const;
    Q_INVOKABLE void     close() const;
};



/// SELECTOR LISTENERS

class AxListenersTableModel : public QAbstractTableModel {
Q_OBJECT
    QVector<ListenerData> m_data;
    QVector<QString> m_headers;
    QVector<QString> m_fieldKeys;

public:
    explicit AxListenersTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void setHeaders(const QVector<QString>& headers, const QVector<QString>& fieldKeys) {
        beginResetModel();
        m_headers = headers;
        m_fieldKeys = fieldKeys;
        endResetModel();
    }

    void setData(const QVector<ListenerData>& data) {
        beginResetModel();
        m_data = data;
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_data.size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_headers.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_data.size() || index.column() >= m_fieldKeys.size())
            return QVariant();

        if (role == Qt::DisplayRole || role == Qt::UserRole) {
            const auto& listener = m_data[index.row()];
            const QString& key = m_fieldKeys[index.column()];

            if (key == "name")       return listener.Name;
            if (key == "type")       return listener.ListenerType;
            if (key == "protocol")   return listener.ListenerProtocol;
            if (key == "bind_host")  return listener.BindHost;
            if (key == "bind_port")  return listener.BindPort;
            if (key == "agent_addr") return listener.AgentAddresses;
            if (key == "status")     return listener.Status;
            if (key == "date")       return listener.Date;
        }
        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section < m_headers.size())
            return m_headers[section];
        return QVariant();
    }

    ListenerData getListener(int row) const {
        if (row >= 0 && row < m_data.size())
            return m_data[row];
        return ListenerData();
    }
};

class AxListenersFilterProxyModel : public QSortFilterProxyModel {
Q_OBJECT
    QString m_filterText;

public:
    explicit AxListenersFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setFilterText(const QString& text) {
        m_filterText = text;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        if (m_filterText.isEmpty())
            return true;

        for (int col = 0; col < sourceModel()->columnCount(); ++col) {
            QModelIndex idx = sourceModel()->index(sourceRow, col, sourceParent);
            if (idx.data().toString().contains(m_filterText, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }
};

class AxDialogListeners : public QDialog {
Q_OBJECT
    QVBoxLayout*  mainLayout   = nullptr;
    QTableView*   tableView    = nullptr;
    QHBoxLayout*  bottomLayout = nullptr;
    QPushButton*  chooseButton = nullptr;
    QSpacerItem*  spacer_1     = nullptr;
    QSpacerItem*  spacer_2     = nullptr;

    QWidget*        searchWidget   = nullptr;
    QHBoxLayout*    searchLayout   = nullptr;
    QLineEdit*      searchLineEdit = nullptr;
    ClickableLabel* hideButton     = nullptr;

    AxListenersTableModel*       tableModel = nullptr;
    AxListenersFilterProxyModel* proxyModel = nullptr;

    QVector<ListenerData> selectedData;

public:
    explicit AxDialogListeners(const QJSValue &headers, QVector<ListenerData> vecListeners, QWidget* parent = nullptr);

    QVector<ListenerData> data();

public Q_SLOTS:
    void onClicked();
    void handleSearch();
    void clearSearch();
};

class AxSelectorListeners : public QObject {
Q_OBJECT
    AxDialogListeners* dialog;
    AxScriptEngine*    scriptEngine;

public:
    explicit AxSelectorListeners(const QJSValue &headers, AxScriptEngine* jsEngine, QWidget* parent = nullptr);

    Q_INVOKABLE void     setSize(int w, int h) const;
    Q_INVOKABLE QJSValue exec() const;
    Q_INVOKABLE void     close() const;
};



/// SELECTOR TARGETS

class AxTargetsTableModel : public QAbstractTableModel {
Q_OBJECT
    QVector<TargetData> m_data;
    QVector<QString> m_headers;
    QVector<QString> m_fieldKeys;

public:
    explicit AxTargetsTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void setHeaders(const QVector<QString>& headers, const QVector<QString>& fieldKeys) {
        beginResetModel();
        m_headers = headers;
        m_fieldKeys = fieldKeys;
        endResetModel();
    }

    void setData(const QVector<TargetData>& data) {
        beginResetModel();
        m_data = data;
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_data.size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_headers.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_data.size() || index.column() >= m_fieldKeys.size())
            return QVariant();

        if (role == Qt::DisplayRole || role == Qt::UserRole) {
            const auto& target = m_data[index.row()];
            const QString& key = m_fieldKeys[index.column()];

            QString os = "unknown";
            if (target.Os == OS_WINDOWS)    os = "windows";
            else if (target.Os == OS_LINUX) os = "linux";
            else if (target.Os == OS_MAC)   os = "macos";

            if (key == "id")       return target.TargetId;
            if (key == "computer") return target.Computer;
            if (key == "domain")   return target.Domain;
            if (key == "address")  return target.Address;
            if (key == "tag")      return target.Tag;
            if (key == "os")       return os;
            if (key == "os_desc")  return target.OsDesc;
            if (key == "info")     return target.Info;
            if (key == "date")     return target.Date;
            if (key == "alive")    return target.Alive ? "Yes" : "No";
        }
        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section < m_headers.size())
            return m_headers[section];
        return QVariant();
    }

    TargetData getTarget(int row) const {
        if (row >= 0 && row < m_data.size())
            return m_data[row];
        return TargetData();
    }
};

class AxTargetsFilterProxyModel : public QSortFilterProxyModel {
Q_OBJECT
    QString m_filterText;

public:
    explicit AxTargetsFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setFilterText(const QString& text) {
        m_filterText = text;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        if (m_filterText.isEmpty())
            return true;

        for (int col = 0; col < sourceModel()->columnCount(); ++col) {
            QModelIndex idx = sourceModel()->index(sourceRow, col, sourceParent);
            if (idx.data().toString().contains(m_filterText, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }
};

class AxDialogTargets : public QDialog {
Q_OBJECT
    QVBoxLayout*  mainLayout   = nullptr;
    QTableView*   tableView    = nullptr;
    QHBoxLayout*  bottomLayout = nullptr;
    QPushButton*  chooseButton = nullptr;
    QSpacerItem*  spacer_1     = nullptr;
    QSpacerItem*  spacer_2     = nullptr;

    QWidget*        searchWidget   = nullptr;
    QHBoxLayout*    searchLayout   = nullptr;
    QLineEdit*      searchLineEdit = nullptr;
    ClickableLabel* hideButton     = nullptr;

    AxTargetsTableModel*       tableModel = nullptr;
    AxTargetsFilterProxyModel* proxyModel = nullptr;

    QVector<TargetData> selectedData;

public:
    explicit AxDialogTargets(const QJSValue &headers, QVector<TargetData> vecTargets, QWidget* parent = nullptr);

    QVector<TargetData> data();

public Q_SLOTS:
    void onClicked();
    void handleSearch();
    void clearSearch();
};

class AxSelectorTargets : public QObject {
Q_OBJECT
    AxDialogTargets* dialog;
    AxScriptEngine*  scriptEngine;

public:
    explicit AxSelectorTargets(const QJSValue &headers, AxScriptEngine* jsEngine, QWidget* parent = nullptr);

    Q_INVOKABLE void     setSize(int w, int h) const;
    Q_INVOKABLE QJSValue exec() const;
    Q_INVOKABLE void     close() const;
};



/// SELECTOR DOWNLOADS

class AxDownloadsTableModel : public QAbstractTableModel {
Q_OBJECT
    QVector<DownloadData> m_data;
    QVector<QString> m_headers;
    QVector<QString> m_fieldKeys;

public:
    explicit AxDownloadsTableModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void setHeaders(const QVector<QString>& headers, const QVector<QString>& fieldKeys) {
        beginResetModel();
        m_headers = headers;
        m_fieldKeys = fieldKeys;
        endResetModel();
    }

    void setData(const QVector<DownloadData>& data) {
        beginResetModel();
        m_data = data;
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_data.size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_headers.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_data.size() || index.column() >= m_fieldKeys.size())
            return QVariant();

        if (role == Qt::DisplayRole || role == Qt::UserRole) {
            const auto& download = m_data[index.row()];
            const QString& key = m_fieldKeys[index.column()];

            QString state;
            switch (download.State) {
                case DOWNLOAD_STATE_RUNNING:  state = "running";  break;
                case DOWNLOAD_STATE_STOPPED:  state = "stopped";  break;
                case DOWNLOAD_STATE_FINISHED: state = "finished"; break;
                default:                      state = "canceled"; break;
            }

            if (key == "id")         return download.FileId;
            if (key == "agent_id")   return download.AgentId;
            if (key == "agent_name") return download.AgentName;
            if (key == "user")       return download.User;
            if (key == "computer")   return download.Computer;
            if (key == "filename")   return download.Filename;
            if (key == "total_size") return download.TotalSize;
            if (key == "recv_size")  return download.RecvSize;
            if (key == "state")      return state;
            if (key == "date")       return download.Date;
        }
        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal && section < m_headers.size())
            return m_headers[section];
        return QVariant();
    }

    DownloadData getDownload(int row) const {
        if (row >= 0 && row < m_data.size())
            return m_data[row];
        return DownloadData();
    }
};

class AxDownloadsFilterProxyModel : public QSortFilterProxyModel {
Q_OBJECT
    QString m_filterText;

public:
    explicit AxDownloadsFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setFilterText(const QString& text) {
        m_filterText = text;
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override {
        if (m_filterText.isEmpty())
            return true;

        for (int col = 0; col < sourceModel()->columnCount(); ++col) {
            QModelIndex idx = sourceModel()->index(sourceRow, col, sourceParent);
            if (idx.data().toString().contains(m_filterText, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }
};

class AxDialogDownloads : public QDialog {
Q_OBJECT
    QVBoxLayout*  mainLayout   = nullptr;
    QTableView*   tableView    = nullptr;
    QHBoxLayout*  bottomLayout = nullptr;
    QPushButton*  chooseButton = nullptr;
    QSpacerItem*  spacer_1     = nullptr;
    QSpacerItem*  spacer_2     = nullptr;

    QWidget*        searchWidget   = nullptr;
    QHBoxLayout*    searchLayout   = nullptr;
    QLineEdit*      searchLineEdit = nullptr;
    ClickableLabel* hideButton     = nullptr;

    AxDownloadsTableModel*       tableModel = nullptr;
    AxDownloadsFilterProxyModel* proxyModel = nullptr;

    QVector<DownloadData> selectedData;

public:
    explicit AxDialogDownloads(const QJSValue &headers, QVector<DownloadData> vecDownloads, QWidget* parent = nullptr);

    QVector<DownloadData> data();

public Q_SLOTS:
    void onClicked();
    void handleSearch();
    void clearSearch();
};

class AxSelectorDownloads : public QObject {
Q_OBJECT
    AxDialogDownloads* dialog;
    AxScriptEngine*    scriptEngine;

public:
    explicit AxSelectorDownloads(const QJSValue &headers, AxScriptEngine* jsEngine, QWidget* parent = nullptr);

    Q_INVOKABLE void     setSize(int w, int h) const;
    Q_INVOKABLE QJSValue exec() const;
    Q_INVOKABLE void     close() const;
};



/// DOCK WIDGET

namespace KDDockWidgets::QtWidgets { class DockWidget; }

class AxDockWrapper : public DockTab {
Q_OBJECT
    QString dockId;
    QString dockTitle;
    QWidget* contentWidget = nullptr;

public:
    explicit AxDockWrapper(AdaptixWidget* w, const QString& id, const QString& title, const QString& location);
    ~AxDockWrapper() override;

    QString id() const { return dockId; }

    Q_INVOKABLE void setLayout(QObject* layoutWrapper);
    Q_INVOKABLE void setSize(int w, int h) const;
    Q_INVOKABLE void show();
    Q_INVOKABLE void hide();
    Q_INVOKABLE void close();
    Q_INVOKABLE bool isVisible() const;
    Q_INVOKABLE void setTitle(const QString& title);

Q_SIGNALS:
    void closed();
    void shown();
    void hidden();
};

#endif
