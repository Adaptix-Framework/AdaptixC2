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


class AbstractAxVisualElement {
public:
    virtual ~AbstractAxVisualElement() = default;

    virtual QWidget* widget() const = 0;
    virtual void setEnabled(const bool enable) const = 0;
    virtual void setVisible(const bool enable) const = 0;
    virtual bool getEnabled() const = 0;
    virtual bool getVisible() const = 0;
};



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
    void setContext(QVariantList context);

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
    Q_INVOKABLE void setText(const QString& text) const;
    Q_INVOKABLE void setPlaceholder(const QString& text) const;

signals:
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

    Q_INVOKABLE void addItem(const QString& text) const;
    Q_INVOKABLE void addItems(const QJSValue& array) const;
    Q_INVOKABLE void setItems(const QJSValue& array) const;
    Q_INVOKABLE void clear() const;
    Q_INVOKABLE QString currentText() const;
    Q_INVOKABLE void setCurrentIndex(int index) const;
    Q_INVOKABLE int currentIndex() const;

signals:
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

    Q_INVOKABLE int value() const;
    Q_INVOKABLE void setValue(int value) const;
    Q_INVOKABLE void setRange(int min, int max) const;

signals:
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
    Q_INVOKABLE void setDateString(const QString& date) const;
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
    Q_INVOKABLE void setTimeString(const QString& time) const;
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
    Q_INVOKABLE void setText(const QString& text) const;
    Q_INVOKABLE void appendText(const QString& text) const;
    Q_INVOKABLE void setPlaceholder(const QString& text) const;
    Q_INVOKABLE void setReadOnly(const bool& readonly) const;
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

signals:
    void stateChanged();
};



/// FILE SELECTOR

class AxFileSelectorWrapper : public QObject, public AbstractAxElement, public AbstractAxVisualElement {
Q_OBJECT
    FileSelector* selector;

public:
    explicit AxFileSelectorWrapper(FileSelector* selector, QObject* parent = nullptr);

    QVariant jsonMarshal() const override;
    void jsonUnmarshal(const QVariant& value) override;

    FileSelector* widget() const override;
    Q_INVOKABLE void setEnabled(const bool enable) const override { widget()->setEnabled(enable); }
    Q_INVOKABLE void setVisible(const bool enable) const override { widget()->setVisible(enable); }
    Q_INVOKABLE bool getEnabled() const override { return widget()->isEnabled(); }
    Q_INVOKABLE bool getVisible() const override { return widget()->isVisible(); }

    Q_INVOKABLE void setPlaceholder(const QString& text) const;
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

signals:
    void clicked();
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
};

#endif //AXELEMENTWRAPPERS_H
