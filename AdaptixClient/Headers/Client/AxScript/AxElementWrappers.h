#ifndef AXELEMENTWRAPPERS_H
#define AXELEMENTWRAPPERS_H

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

class AxScriptEngine;

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
        FileSelector* selector;

public:
    explicit AxSelectorFile(FileSelector* selector, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    FileSelector* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE void setPlaceholder(const QString& text) const;
};



/// SELECTOR CREDENTIALS

class AxDialogCreds : public QDialog {
Q_OBJECT
    QVBoxLayout*  mainLayout   = nullptr;
    QTableWidget* tableWidget  = nullptr;
    QHBoxLayout*  bottomLayout = nullptr;
    QPushButton*  chooseButton = nullptr;
    QSpacerItem*  spacer_1     = nullptr;
    QSpacerItem*  spacer_2     = nullptr;

    QWidget*        searchWidget   = nullptr;
    QHBoxLayout*    searchLayout   = nullptr;
    QLineEdit*      searchLineEdit = nullptr;
    ClickableLabel* hideButton     = nullptr;

    QVector<QString> table_headers;
    QVector<QMap<QString, QString> > credList;
    QMap<QString, CredentialData>    allData;
    QVector<CredentialData> selectedData;

public:
    explicit AxDialogCreds(const QJSValue &headers, QVector<CredentialData> vecCreds, QTableWidget* tableWidget, QPushButton* button, QWidget* parent = nullptr);

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
    QMap<QString, CredentialData> creds;

public:
    explicit AxSelectorCreds(const QJSValue &headers, QTableWidget* tableWidget, QPushButton* button, AxScriptEngine* jsEngine, QWidget* parent = nullptr);

    Q_INVOKABLE void     setSize(int w, int h) const;
    Q_INVOKABLE QJSValue exec() const;
    Q_INVOKABLE void     close() const;
};



/// SELECTOR CREDENTIALS

class AxDialogAgents : public QDialog {
Q_OBJECT
    QVBoxLayout*  mainLayout   = nullptr;
    QTableWidget* tableWidget  = nullptr;
    QHBoxLayout*  bottomLayout = nullptr;
    QPushButton*  chooseButton = nullptr;
    QSpacerItem*  spacer_1     = nullptr;
    QSpacerItem*  spacer_2     = nullptr;

    QWidget*        searchWidget   = nullptr;
    QHBoxLayout*    searchLayout   = nullptr;
    QLineEdit*      searchLineEdit = nullptr;
    ClickableLabel* hideButton     = nullptr;

    QVector<QString> table_headers;
    QVector<QMap<QString, QString> > agentsList;
    QMap<QString, AgentData>    allData;
    QVector<AgentData> selectedData;

public:
    explicit AxDialogAgents(const QJSValue &headers, QVector<AgentData> vecAgents, QTableWidget* tableWidget, QPushButton* button, QWidget* parent = nullptr);

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
    QMap<QString, AgentData> agents;

public:
    explicit AxSelectorAgents(const QJSValue &headers, QTableWidget* tableWidget, QPushButton* button, AxScriptEngine* jsEngine, QWidget* parent = nullptr);

    Q_INVOKABLE void     setSize(int w, int h) const;
    Q_INVOKABLE QJSValue exec() const;
    Q_INVOKABLE void     close() const;
};

#endif
